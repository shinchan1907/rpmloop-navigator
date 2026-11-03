package io.bytenex.rpmloop.ble

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.BluetoothStatusCodes
import android.bluetooth.le.BluetoothLeScanner
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.location.Location
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.os.ParcelUuid
import android.util.Log
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import org.json.JSONObject
import java.util.UUID

enum class BleConnectionState { DISCONNECTED, SCANNING, CONNECTING, CONNECTED }

data class BleStatus(
    val state: BleConnectionState = BleConnectionState.DISCONNECTED,
    val deviceName: String? = null,
    /** null = handshake not attempted yet; otherwise a human-readable result. */
    val handshake: String? = null,
    /** Honest extra info (e.g. "Bluetooth is off", "Connection lost — rescanning"). */
    val detail: String? = null
)

/**
 * Owns the whole BLE session with the RPMLOOP device:
 * scan (name / service-UUID filter) -> connect -> MTU 512 -> service discovery
 * -> enable notifications (handshake, device_cmd, file_ctrl) -> hello /
 * hello_ack handshake -> position / nav / polyline writes, plus the raw
 * plumbing for the v3.1 file transfer (see FileTransferClient).
 *
 * Session writes are serialised through a small queue: Android's stack
 * only tolerates one outstanding operation at a time. (FILE_DATA chunks
 * are the one deliberate exception — see writeFileDataRaw.)
 *
 * Assumes the caller (CompanionService) has already verified runtime
 * permissions; SecurityExceptions are caught and surfaced as status detail.
 */
class BleManager(private val context: Context) {

    companion object {
        private const val TAG = "BleManager"
    }

    private val bluetoothManager =
        context.getSystemService(Context.BLUETOOTH_SERVICE) as? BluetoothManager
    private val adapter: BluetoothAdapter? get() = bluetoothManager?.adapter

    private val _status = MutableStateFlow(BleStatus())
    val status: StateFlow<BleStatus> = _status.asStateFlow()

    @Volatile
    private var engineRunning = false

    private var scanner: BluetoothLeScanner? = null
    private var gatt: BluetoothGatt? = null
    private var handshakeChar: BluetoothGattCharacteristic? = null
    private var positionChar: BluetoothGattCharacteristic? = null
    private var navChar: BluetoothGattCharacteristic? = null
    private var polylineChar: BluetoothGattCharacteristic? = null
    private var deviceCmdChar: BluetoothGattCharacteristic? = null
    private var fileDataChar: BluetoothGattCharacteristic? = null
    private var fileCtrlChar: BluetoothGattCharacteristic? = null

    @Volatile
    private var handshakeDone = false

    /** Negotiated ATT MTU (23 until onMtuChanged says otherwise). */
    @Volatile
    var negotiatedMtu: Int = 23
        private set

    /** Usable ATT payload per write: MTU minus the 3-byte ATT header. */
    fun attPayloadSize(): Int = (negotiatedMtu - 3).coerceAtLeast(20)

    fun isHandshakeDone(): Boolean = handshakeDone

    /** v3.1: parsed JSON messages arriving on FILE_CTRL land here. Set by
     *  FileTransferClient; invoked on a binder thread. */
    @Volatile
    var fileCtrlListener: ((JSONObject) -> Unit)? = null

    /** v3.1: invoked when the BLE link drops, so a transfer can abort. */
    @Volatile
    var fileLinkDownListener: (() -> Unit)? = null

    @Volatile
    private var latestNavJson: String? = null

    @Volatile
    private var navOpQueued = false

    @Volatile
    private var latestLocation: Location? = null

    @Volatile
    private var positionOpQueued = false

    private val mainHandler = Handler(Looper.getMainLooper())

    // ---- serialised GATT operation queue ------------------------------------
    private val opQueue = ArrayDeque<() -> Boolean>()
    private var opInFlight = false

    @Synchronized
    private fun enqueue(op: () -> Boolean) {
        opQueue.addLast(op)
        pumpQueueLocked()
    }

    @Synchronized
    private fun pumpQueueLocked() {
        if (opInFlight) return
        val op = opQueue.removeFirstOrNull() ?: return
        opInFlight = true
        val started = try {
            op()
        } catch (e: Exception) {
            Log.w(TAG, "GATT op failed to start", e)
            false
        }
        if (!started) {
            opInFlight = false
            pumpQueueLocked()
        }
    }

    @Synchronized
    private fun completeOp() {
        opInFlight = false
        pumpQueueLocked()
    }

    @Synchronized
    private fun clearQueue() {
        opQueue.clear()
        opInFlight = false
        positionOpQueued = false
        navOpQueued = false
    }

    // ---- public control ------------------------------------------------------

    fun start() {
        if (engineRunning) return
        engineRunning = true
        _status.update {
            it.copy(handshake = null, detail = null)
        }
        startScan()
    }

    @SuppressLint("MissingPermission")
    fun stop() {
        engineRunning = false
        stopScan()
        clearQueue()
        fileLinkDownListener?.invoke()
        handshakeDone = false
        latestLocation = null
        try {
            gatt?.disconnect()
            gatt?.close()
        } catch (e: Exception) {
            Log.w(TAG, "Error closing GATT", e)
        }
        gatt = null
        handshakeChar = null
        positionChar = null
        navChar = null
        polylineChar = null
        deviceCmdChar = null
        fileDataChar = null
        fileCtrlChar = null
        negotiatedMtu = 23
        latestNavJson = null
        _status.value = BleStatus()
    }

    /**
     * Called at ~1 Hz with fresh GPS fixes. Only the newest fix is kept;
     * a position write already waiting in the queue will pick it up when it
     * executes. Nothing is sent before the handshake completes — the hello /
     * hello_ack exchange is what opens a protocol session.
     */
    fun sendPosition(location: Location) {
        latestLocation = location
        if (!handshakeDone || positionChar == null) return
        if (positionOpQueued) return
        positionOpQueued = true
        enqueue {
            positionOpQueued = false
            val loc = latestLocation
            val ch = positionChar
            if (!handshakeDone || loc == null || ch == null) {
                false
            } else {
                val bytes = RpmloopProtocol.buildPosition(loc).toByteArray(Charsets.UTF_8)
                writeCharacteristic(ch, bytes)
            }
        }
    }

    /**
     * Sends a spec-format "nav" payload. Same coalescing pattern as
     * position: at most one nav write waits in the queue, and it carries the
     * newest payload when it executes — a burst of engine updates can never
     * flood the queue with stale maneuvers.
     */
    fun sendNav(json: String) {
        latestNavJson = json
        if (!handshakeDone || navChar == null) return
        if (navOpQueued) return
        navOpQueued = true
        enqueue {
            navOpQueued = false
            val payload = latestNavJson
            val ch = navChar
            if (!handshakeDone || payload == null || ch == null) {
                false
            } else {
                writeCharacteristic(ch, payload.toByteArray(Charsets.UTF_8))
            }
        }
    }

    /**
     * Sends a whole route polyline: [chunks] are complete "poly" JSON
     * messages (see RoutePolylineProvider) and are written strictly in
     * order through the queue. Returns false when there is no live session
     * to send on — the caller surfaces that honestly.
     */
    fun sendPolyline(chunks: List<String>): Boolean {
        if (!handshakeDone || polylineChar == null || chunks.isEmpty()) return false
        for (chunk in chunks) {
            val bytes = chunk.toByteArray(Charsets.UTF_8)
            enqueue {
                val ch = polylineChar ?: return@enqueue false
                writeCharacteristic(ch, bytes)
            }
        }
        return true
    }

    // ---- protocol v3.1 file transfer plumbing ---------------------------------
    // FILE_CTRL control JSON goes through the serialised queue (write with
    // response, so ordering against other session traffic is guaranteed).
    // FILE_DATA chunks deliberately BYPASS the queue: they are
    // WRITE_NO_RSP and paced by FileTransferClient's own sliding window;
    // routing them through the one-op-at-a-time queue would serialise the
    // window away. The Android stack arbitrates the two write streams.

    fun writeFileCtrl(json: String) {
        val bytes = json.toByteArray(Charsets.UTF_8)
        enqueue {
            val ch = fileCtrlChar ?: return@enqueue false
            writeCharacteristic(ch, bytes)
        }
    }

    /** Raw unqueued FILE_DATA write. Returns false if the stack is busy or
     *  the link is gone — the caller retries after a short pause. */
    @SuppressLint("MissingPermission")
    fun writeFileDataRaw(bytes: ByteArray): Boolean {
        val g = gatt ?: return false
        val ch = fileDataChar ?: return false
        return try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                g.writeCharacteristic(
                    ch,
                    bytes,
                    BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
                ) == BluetoothStatusCodes.SUCCESS
            } else {
                ch.writeType = BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
                @Suppress("DEPRECATION")
                ch.value = bytes
                @Suppress("DEPRECATION")
                g.writeCharacteristic(ch)
            }
        } catch (e: Exception) {
            false
        }
    }

    fun fileTransferReady(): Boolean =
        handshakeDone && fileDataChar != null && fileCtrlChar != null

    // ---- scanning ------------------------------------------------------------

    @SuppressLint("MissingPermission")
    private fun startScan() {
        val ad = adapter
        if (ad == null || !ad.isEnabled) {
            _status.update {
                it.copy(
                    state = BleConnectionState.DISCONNECTED,
                    detail = "Bluetooth is off or unavailable on this phone"
                )
            }
            return
        }
        val leScanner = try {
            ad.bluetoothLeScanner
        } catch (e: Exception) {
            null
        }
        if (leScanner == null) {
            _status.update {
                it.copy(
                    state = BleConnectionState.DISCONNECTED,
                    detail = "BLE scanner unavailable"
                )
            }
            return
        }
        scanner = leScanner
        // Name filter OR service-UUID filter (a result matching either is delivered).
        val filters = listOf(
            ScanFilter.Builder().setDeviceName(RpmloopProtocol.DEVICE_NAME).build(),
            ScanFilter.Builder()
                .setServiceUuid(ParcelUuid(RpmloopProtocol.SERVICE_UUID))
                .build()
        )
        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()
        _status.update {
            it.copy(state = BleConnectionState.SCANNING, detail = it.detail)
        }
        try {
            leScanner.startScan(filters, settings, scanCallback)
        } catch (e: SecurityException) {
            _status.update {
                it.copy(
                    state = BleConnectionState.DISCONNECTED,
                    detail = "Bluetooth scan permission missing"
                )
            }
        }
    }

    @SuppressLint("MissingPermission")
    private fun stopScan() {
        try {
            scanner?.stopScan(scanCallback)
        } catch (e: Exception) {
            // Scanner may already be stopped; nothing useful to do.
        }
        scanner = null
    }

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            if (!engineRunning) return
            val scanRecord = result.scanRecord ?: return
            val nameMatches = scanRecord.deviceName == RpmloopProtocol.DEVICE_NAME
            val uuidMatches = scanRecord.serviceUuids
                ?.contains(ParcelUuid(RpmloopProtocol.SERVICE_UUID)) == true
            if (!nameMatches && !uuidMatches) return
            stopScan()
            connect(result.device)
        }

        override fun onScanFailed(errorCode: Int) {
            if (!engineRunning) return
            _status.update {
                it.copy(
                    state = BleConnectionState.DISCONNECTED,
                    detail = "Scan failed (error $errorCode)"
                )
            }
        }
    }

    // ---- connection / GATT ---------------------------------------------------

    @SuppressLint("MissingPermission")
    private fun connect(device: BluetoothDevice) {
        _status.update {
            it.copy(
                state = BleConnectionState.CONNECTING,
                deviceName = RpmloopProtocol.DEVICE_NAME,
                detail = null
            )
        }
        gatt = try {
            device.connectGatt(context, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
        } catch (e: SecurityException) {
            _status.update {
                it.copy(
                    state = BleConnectionState.DISCONNECTED,
                    detail = "Bluetooth connect permission missing"
                )
            }
            null
        }
    }

    private val gattCallback = object : BluetoothGattCallback() {

        @SuppressLint("MissingPermission")
        override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
            if (g !== gatt) return // stale callback from an older connection
            when (newState) {
                BluetoothProfile.STATE_CONNECTED -> {
                    // Protocol JSON payloads exceed the default 20-byte ATT
                    // payload, so negotiate MTU 512 (spec max_mtu) first.
                    val mtuRequested = try {
                        g.requestMtu(RpmloopProtocol.REQUESTED_MTU)
                    } catch (e: Exception) {
                        false
                    }
                    if (!mtuRequested) {
                        g.discoverServices()
                    }
                }
                BluetoothProfile.STATE_DISCONNECTED -> handleDisconnect()
            }
        }

        @SuppressLint("MissingPermission")
        override fun onMtuChanged(g: BluetoothGatt, mtu: Int, status: Int) {
            if (g !== gatt) return
            negotiatedMtu = mtu
            Log.d(TAG, "MTU negotiated: $mtu (status $status)")
            g.discoverServices()
        }

        override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
            if (g !== gatt) return
            if (status != BluetoothGatt.GATT_SUCCESS) {
                _status.update { it.copy(detail = "Service discovery failed") }
                return
            }
            val service = g.getService(RpmloopProtocol.SERVICE_UUID)
            if (service == null) {
                _status.update {
                    it.copy(detail = "RPMLOOP service not found on this device")
                }
                return
            }
            handshakeChar = service.getCharacteristic(RpmloopProtocol.HANDSHAKE_UUID)
            positionChar = service.getCharacteristic(RpmloopProtocol.POSITION_UUID)
            navChar = service.getCharacteristic(RpmloopProtocol.NAV_STATE_UUID)
            polylineChar = service.getCharacteristic(RpmloopProtocol.POLYLINE_UUID)
            deviceCmdChar = service.getCharacteristic(RpmloopProtocol.DEVICE_CMD_UUID)
            // Protocol v3.1 file transfer (older firmware without these
            // characteristics simply leaves them null; transfers then report
            // "not supported by this device" — position/nav are unaffected).
            fileDataChar = service.getCharacteristic(RpmloopProtocol.FILE_DATA_UUID)
            fileCtrlChar = service.getCharacteristic(RpmloopProtocol.FILE_CTRL_UUID)
            if (handshakeChar == null) {
                _status.update { it.copy(detail = "Handshake characteristic missing") }
                return
            }
            _status.update {
                it.copy(state = BleConnectionState.CONNECTED, detail = null)
            }
            // Serialised: enable handshake notifications, enable device_cmd
            // notifications, then write hello. The ack arrives as a
            // notification on the handshake characteristic.
            handshakeChar?.let { enqueueEnableNotify(it) }
            deviceCmdChar?.let { enqueueEnableNotify(it) }
            fileCtrlChar?.let { enqueueEnableNotify(it) }
            enqueue {
                val ch = handshakeChar ?: return@enqueue false
                val bytes = RpmloopProtocol.buildHello().toByteArray(Charsets.UTF_8)
                writeCharacteristic(ch, bytes)
            }
        }

        override fun onDescriptorWrite(
            g: BluetoothGatt,
            descriptor: BluetoothGattDescriptor,
            status: Int
        ) {
            completeOp()
        }

        override fun onCharacteristicWrite(
            g: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            status: Int
        ) {
            completeOp()
        }

        // Pre-API 33 callback (still invoked on older devices).
        @Suppress("DEPRECATION")
        override fun onCharacteristicChanged(
            g: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic
        ) {
            val value = characteristic.value ?: return
            handleNotification(characteristic.uuid, value)
        }

        // API 33+ callback.
        override fun onCharacteristicChanged(
            g: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            value: ByteArray
        ) {
            handleNotification(characteristic.uuid, value)
        }
    }

    @SuppressLint("MissingPermission")
    private fun handleDisconnect() {
        try {
            gatt?.close()
        } catch (e: Exception) {
            // ignore
        }
        gatt = null
        handshakeChar = null
        positionChar = null
        navChar = null
        polylineChar = null
        deviceCmdChar = null
        fileDataChar = null
        fileCtrlChar = null
        negotiatedMtu = 23
        latestNavJson = null
        handshakeDone = false
        clearQueue()
        fileLinkDownListener?.invoke()
        if (engineRunning) {
            _status.update {
                it.copy(
                    state = BleConnectionState.SCANNING,
                    deviceName = null,
                    handshake = null,
                    detail = "Connection lost — rescanning"
                )
            }
            mainHandler.postDelayed({
                if (engineRunning) startScan()
            }, 1500)
        } else {
            _status.value = BleStatus()
        }
    }

    private fun handleNotification(uuid: UUID, value: ByteArray) {
        val text = value.toString(Charsets.UTF_8)
        when (uuid) {
            RpmloopProtocol.HANDSHAKE_UUID -> {
                val ack = RpmloopProtocol.parseHelloAck(text)
                if (ack != null) {
                    handshakeDone = true
                    val summary = buildString {
                        append("OK — ").append(ack.deviceName)
                        if (ack.fwVersion.isNotEmpty()) append(" · fw ").append(ack.fwVersion)
                        append(" · SD card: ").append(if (ack.hasSd) "yes" else "no")
                        if (ack.displayWidth != null && ack.displayHeight != null) {
                            append(" · ").append(ack.displayWidth)
                                .append("×").append(ack.displayHeight)
                        }
                    }
                    _status.update { it.copy(handshake = summary, detail = null) }
                } else {
                    _status.update {
                        it.copy(detail = "Handshake failed: unexpected response from device")
                    }
                }
            }
            RpmloopProtocol.DEVICE_CMD_UUID -> {
                // Device -> app commands (media_cmd / brightness). Receiving and
                // logging them is Phase 1; acting on them is Phase 2
                // (see phase2/MediaSessionController).
                Log.d(TAG, "device_cmd received: $text")
            }
            RpmloopProtocol.FILE_CTRL_UUID -> {
                val parsed = try {
                    JSONObject(text)
                } catch (e: Exception) {
                    null
                }
                if (parsed != null) {
                    fileCtrlListener?.invoke(parsed)
                } else {
                    Log.w(TAG, "FILE_CTRL: unparseable message ignored")
                }
            }
        }
    }

    // ---- low-level helpers -----------------------------------------------------

    @SuppressLint("MissingPermission")
    private fun enqueueEnableNotify(ch: BluetoothGattCharacteristic) {
        enqueue {
            val g = gatt ?: return@enqueue false
            if (ch.properties and BluetoothGattCharacteristic.PROPERTY_NOTIFY == 0) {
                return@enqueue false
            }
            val localOk = try {
                g.setCharacteristicNotification(ch, true)
            } catch (e: Exception) {
                false
            }
            if (!localOk) return@enqueue false
            val cccd = ch.getDescriptor(RpmloopProtocol.CCCD_UUID)
                ?: return@enqueue false
            writeDescriptor(cccd, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)
        }
    }

    @SuppressLint("MissingPermission")
    private fun writeDescriptor(descriptor: BluetoothGattDescriptor, value: ByteArray): Boolean {
        val g = gatt ?: return false
        return try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                g.writeDescriptor(descriptor, value) == BluetoothStatusCodes.SUCCESS
            } else {
                @Suppress("DEPRECATION")
                descriptor.value = value
                @Suppress("DEPRECATION")
                g.writeDescriptor(descriptor)
            }
        } catch (e: Exception) {
            false
        }
    }

    @SuppressLint("MissingPermission")
    private fun writeCharacteristic(
        ch: BluetoothGattCharacteristic,
        bytes: ByteArray
    ): Boolean {
        val g = gatt ?: return false
        return try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                g.writeCharacteristic(
                    ch,
                    bytes,
                    BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
                ) == BluetoothStatusCodes.SUCCESS
            } else {
                ch.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
                @Suppress("DEPRECATION")
                ch.value = bytes
                @Suppress("DEPRECATION")
                g.writeCharacteristic(ch)
            }
        } catch (e: Exception) {
            false
        }
    }
}

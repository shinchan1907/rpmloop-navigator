package io.bytenex.rpmloop.service

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Intent
import android.content.pm.ServiceInfo
import android.os.Binder
import android.os.Build
import android.os.IBinder
import androidx.core.app.NotificationCompat
import io.bytenex.rpmloop.MainActivity
import io.bytenex.rpmloop.R
import io.bytenex.rpmloop.ble.BleConnectionState
import io.bytenex.rpmloop.ble.BleManager
import io.bytenex.rpmloop.location.LocationProvider
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch

/**
 * Foreground service (location type) that owns the BLE session and the GPS
 * stream so both keep running with the screen off. The persistent
 * notification always shows the current connection state.
 */
class CompanionService : Service() {

    companion object {
        const val ACTION_START = "io.bytenex.rpmloop.action.START"
        const val ACTION_STOP = "io.bytenex.rpmloop.action.STOP"

        private const val NOTIFICATION_ID = 1001
        private const val CHANNEL_ID = "rpmloop_connection"

        /** True while the service engine is running; lets the Activity
         *  re-attach after rotation or process return. */
        @Volatile
        var isRunning: Boolean = false
            private set
    }

    inner class LocalBinder : Binder() {
        fun getService(): CompanionService = this@CompanionService
    }

    private val binder = LocalBinder()

    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main)

    private lateinit var bleManager: BleManager
    private lateinit var locationProvider: LocationProvider

    private val _state = MutableStateFlow(AppUiState())
    val state: StateFlow<AppUiState> = _state.asStateFlow()

    @Volatile
    private var started = false

    override fun onCreate() {
        super.onCreate()
        createNotificationChannel()
        bleManager = BleManager(applicationContext)
        locationProvider = LocationProvider(applicationContext) { location ->
            onNewLocation(location)
        }
        scope.launch {
            bleManager.status.collect { ble ->
                _state.update {
                    it.copy(
                        connection = ble.state,
                        deviceName = ble.deviceName,
                        handshake = ble.handshake,
                        detail = ble.detail
                    )
                }
                if (started) refreshNotification()
            }
        }
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == ACTION_STOP) {
            stopEverything()
            return START_NOT_STICKY
        }
        // ACTION_START (or a sticky restart with a null intent)
        promoteToForeground()
        startEngine()
        return START_STICKY
    }

    override fun onBind(intent: Intent?): IBinder = binder

    override fun onDestroy() {
        if (started) {
            locationProvider.stop()
            bleManager.stop()
        }
        started = false
        isRunning = false
        scope.cancel()
        super.onDestroy()
    }

    /** Stops scanning, BLE and GPS, removes the notification and the service. */
    fun stopEverything() {
        if (!started) {
            stopSelf()
            return
        }
        started = false
        isRunning = false
        locationProvider.stop()
        bleManager.stop()
        _state.value = AppUiState()
        stopForeground(STOP_FOREGROUND_REMOVE)
        stopSelf()
    }

    private fun startEngine() {
        if (started) return
        started = true
        isRunning = true
        bleManager.start()
        locationProvider.start()
    }

    private fun onNewLocation(location: android.location.Location) {
        _state.update {
            it.copy(
                latitude = location.latitude,
                longitude = location.longitude,
                speedKmh = if (location.hasSpeed()) {
                    location.speed.toDouble() * 3.6
                } else {
                    null
                },
                headingDeg = if (location.hasBearing()) {
                    location.bearing.toDouble()
                } else {
                    null
                }
            )
        }
        bleManager.sendPosition(location)
    }

    // ---- foreground notification ----------------------------------------------

    private fun createNotificationChannel() {
        val channel = NotificationChannel(
            CHANNEL_ID,
            "Connection status",
            NotificationManager.IMPORTANCE_LOW
        ).apply {
            description = "Persistent notification while RPMLOOP Companion is running"
        }
        val manager = getSystemService(NotificationManager::class.java)
        manager?.createNotificationChannel(channel)
    }

    private fun connectionText(state: AppUiState): String = when (state.connection) {
        BleConnectionState.DISCONNECTED -> "Disconnected"
        BleConnectionState.SCANNING -> "Scanning for RPMLOOP…"
        BleConnectionState.CONNECTING -> "Connecting to RPMLOOP…"
        BleConnectionState.CONNECTED ->
            state.handshake?.let { "Connected · $it" } ?: "Connected to RPMLOOP"
    }

    private fun buildNotification(state: AppUiState): Notification {
        val contentIntent = PendingIntent.getActivity(
            this,
            0,
            Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )
        val stopIntent = PendingIntent.getService(
            this,
            1,
            Intent(this, CompanionService::class.java).setAction(ACTION_STOP),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )
        return NotificationCompat.Builder(this, CHANNEL_ID)
            .setSmallIcon(R.drawable.ic_nav_arrow)
            .setContentTitle("RPMLOOP Companion")
            .setContentText(connectionText(state))
            .setOngoing(true)
            .setContentIntent(contentIntent)
            .addAction(0, "Stop", stopIntent)
            .build()
    }

    private fun promoteToForeground() {
        val notification = buildNotification(_state.value)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            startForeground(
                NOTIFICATION_ID,
                notification,
                ServiceInfo.FOREGROUND_SERVICE_TYPE_LOCATION
            )
        } else {
            startForeground(NOTIFICATION_ID, notification)
        }
    }

    private fun refreshNotification() {
        val manager = getSystemService(NotificationManager::class.java)
        manager?.notify(NOTIFICATION_ID, buildNotification(_state.value))
    }
}

package com.sunnygupta.rpmloopnav;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.Set;
import java.util.UUID;

public class BleManager {
    private static final String TAG = "RPMLoopBle";
    public static BleManager instance;

    public static BleManager getInstance() {
        return instance;
    }

    // RPMLoop Nav UUIDs
    public static final UUID SERVICE_UUID = UUID.fromString("4E415601-5250-4D4C-6F70-704E61760001");
    public static final UUID CHAR_CMD_NAV = UUID.fromString("4E415602-5250-4D4C-6F70-704E61760002");
    public static final UUID CHAR_STATUS_ACK = UUID.fromString("4E415603-5250-4D4C-6F70-704E61760003");
    public static final UUID CLIENT_CONFIG_DESCRIPTOR = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb");

    public interface BleListener {
        void onConnectionStateChange(String status, String deviceName, int rssi);
        void onAckReceived(String payload);
        void onLog(String level, String message);
        void onRssiUpdated(int rssi);
    }

    private final Context context;
    private final BleListener listener;
    private final Handler mainHandler = new Handler(Looper.getMainLooper());

    private BluetoothAdapter bluetoothAdapter;
    private BluetoothLeScanner bleScanner;
    private BluetoothGatt bluetoothGatt;
    private BluetoothDevice lastDevice;
    private BluetoothGattCharacteristic cmdCharacteristic;
    private BluetoothGattCharacteristic ackCharacteristic;

    private boolean isScanning = false;
    private boolean isConnected = false;
    private boolean shouldAutoReconnect = true;
    private int currentRssi = -60;
    private Runnable rssiPoller;

    public BleManager(Context context, BleListener listener) {
        this.context = context;
        this.listener = listener;
        this.bluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
        instance = this;
    }

    public boolean isConnected() {
        return isConnected;
    }

    public int getCurrentRssi() {
        return currentRssi;
    }

    public boolean isBluetoothEnabled() {
        return bluetoothAdapter != null && bluetoothAdapter.isEnabled();
    }

    private boolean isRpmLoopDevice(String name, String address) {
        if (name != null) {
            String upper = name.toUpperCase();
            if (upper.contains("RPMLOOP") || upper.contains("RPMLOPP") || upper.contains("RPM")) {
                return true;
            }
        }
        return false;
    }

    public void startScan() {
        if (bluetoothAdapter == null || !bluetoothAdapter.isEnabled()) {
            listener.onLog("ERR", "Bluetooth is turned OFF. Please turn on Bluetooth.");
            return;
        }

        shouldAutoReconnect = true;

        // STEP 1: Check if RPMLOOP is ALREADY connected at OS level!
        // (If phone paired or connected in system settings, scanner won't see it!)
        try {
            BluetoothManager bm = (BluetoothManager) context.getSystemService(Context.BLUETOOTH_SERVICE);
            if (bm != null) {
                List<BluetoothDevice> connected = bm.getConnectedDevices(BluetoothProfile.GATT);
                for (BluetoothDevice dev : connected) {
                    String name = dev.getName();
                    if (isRpmLoopDevice(name, dev.getAddress())) {
                        listener.onLog("INFO", "Found already connected RPMLOOP (" + dev.getAddress() + "). Linking...");
                        connectToDevice(dev);
                        return;
                    }
                }
            }

            // Also check bonded/paired devices
            Set<BluetoothDevice> bonded = bluetoothAdapter.getBondedDevices();
            if (bonded != null) {
                for (BluetoothDevice dev : bonded) {
                    String name = dev.getName();
                    if (isRpmLoopDevice(name, dev.getAddress())) {
                        listener.onLog("INFO", "Found paired RPMLOOP (" + dev.getAddress() + "). Linking...");
                        connectToDevice(dev);
                        return;
                    }
                }
            }
        } catch (SecurityException ignored) {}

        // STEP 2: If not already connected/bonded, start BLE scan
        bleScanner = bluetoothAdapter.getBluetoothLeScanner();
        if (bleScanner == null) {
            listener.onLog("ERR", "Bluetooth LE Scanner unavailable.");
            return;
        }

        if (isScanning) return;
        isScanning = true;
        listener.onLog("INFO", "Scanning for RPMLOOP display...");
        listener.onConnectionStateChange("SCANNING", "", 0);

        List<ScanFilter> filters = new ArrayList<>();
        ScanSettings settings = new ScanSettings.Builder()
                .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
                .build();

        try {
            bleScanner.startScan(filters, settings, scanCallback);
            mainHandler.postDelayed(() -> {
                if (isScanning && !isConnected) {
                    stopScan();
                    listener.onLog("WARN", "Scan finished. Ensure ESP32 is powered on.");
                    listener.onConnectionStateChange("DISCONNECTED", "", 0);
                }
            }, 10000);
        } catch (SecurityException e) {
            listener.onLog("ERR", "Bluetooth permission error: " + e.getMessage());
        }
    }

    public void stopScan() {
        if (!isScanning) return;
        isScanning = false;
        if (bleScanner != null) {
            try {
                bleScanner.stopScan(scanCallback);
            } catch (SecurityException ignored) {}
        }
    }

    public void disconnect() {
        shouldAutoReconnect = false;
        stopScan();
        stopRssiPoller();
        if (bluetoothGatt != null) {
            try {
                bluetoothGatt.disconnect();
                bluetoothGatt.close();
            } catch (SecurityException ignored) {}
            bluetoothGatt = null;
        }
        isConnected = false;
        cmdCharacteristic = null;
        ackCharacteristic = null;
        listener.onConnectionStateChange("DISCONNECTED", "", 0);
        listener.onLog("INFO", "Disconnected from RPMLOOP.");
    }

    private final ScanCallback scanCallback = new ScanCallback() {
        @Override
        public void onScanResult(int callbackType, ScanResult result) {
            BluetoothDevice device = result.getDevice();
            String name = "";
            try {
                name = device.getName();
            } catch (SecurityException ignored) {}

            if (name == null && result.getScanRecord() != null) {
                name = result.getScanRecord().getDeviceName();
            }

            if (isRpmLoopDevice(name, device.getAddress())) {
                listener.onLog("INFO", "Discovered " + (name != null ? name : "RPMLOOP") + " (" + device.getAddress() + ")");
                stopScan();
                connectToDevice(device);
            }
        }

        @Override
        public void onScanFailed(int errorCode) {
            isScanning = false;
            listener.onLog("ERR", "BLE Scan error code: " + errorCode);
            listener.onConnectionStateChange("DISCONNECTED", "", 0);
        }
    };

    public void connectToDevice(BluetoothDevice device) {
        lastDevice = device;
        listener.onLog("INFO", "Establishing GATT connection to " + device.getAddress() + "...");
        listener.onConnectionStateChange("CONNECTING", "RPMLOOP", 0);

        if (bluetoothGatt != null) {
            try {
                bluetoothGatt.disconnect();
                bluetoothGatt.close();
            } catch (Exception ignored) {}
            bluetoothGatt = null;
        }

        try {
            bluetoothGatt = device.connectGatt(context, false, gattCallback, BluetoothDevice.TRANSPORT_LE);
        } catch (SecurityException e) {
            listener.onLog("ERR", "connectGatt failed: " + e.getMessage());
        }
    }

    private final BluetoothGattCallback gattCallback = new BluetoothGattCallback() {
        @Override
        public void onConnectionStateChange(BluetoothGatt gatt, int status, int newState) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                Log.w(TAG, "GATT error status " + status + " newState " + newState);
                try {
                    gatt.disconnect();
                    gatt.close();
                } catch (Exception ignored) {}
                bluetoothGatt = null;
                isConnected = false;
                cmdCharacteristic = null;
                ackCharacteristic = null;
                stopRssiPoller();
                listener.onConnectionStateChange("DISCONNECTED", "", 0);
                listener.onLog("WARN", "BLE connection reset (GATT status " + status + "). Reconnecting...");

                if (shouldAutoReconnect && lastDevice != null) {
                    mainHandler.postDelayed(() -> {
                        if (!isConnected) connectToDevice(lastDevice);
                    }, 2000);
                }
                return;
            }

            if (newState == BluetoothProfile.STATE_CONNECTED) {
                isConnected = true;
                mainHandler.post(() -> {
                    listener.onConnectionStateChange("CONNECTED", "RPMLOOP", currentRssi);
                    listener.onLog("INFO", "Connected to RPMLOOP hardware! Discovering services...");
                });

                // Immediately discover services with 200ms delay for Samsung GATT stack
                mainHandler.postDelayed(() -> {
                    if (bluetoothGatt != null) {
                        try {
                            bluetoothGatt.discoverServices();
                        } catch (SecurityException ignored) {}
                    }
                }, 200);

            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                isConnected = false;
                cmdCharacteristic = null;
                ackCharacteristic = null;
                stopRssiPoller();
                try {
                    gatt.close();
                } catch (Exception ignored) {}
                bluetoothGatt = null;
                listener.onConnectionStateChange("DISCONNECTED", "", 0);
                listener.onLog("WARN", "RPMLOOP link closed. Reconnecting in 2s...");

                if (shouldAutoReconnect && lastDevice != null) {
                    mainHandler.postDelayed(() -> {
                        if (!isConnected) connectToDevice(lastDevice);
                    }, 2000);
                }
            }
        }

        @Override
        public void onServicesDiscovered(BluetoothGatt gatt, int status) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                BluetoothGattService service = gatt.getService(SERVICE_UUID);
                if (service != null) {
                    cmdCharacteristic = service.getCharacteristic(CHAR_CMD_NAV);
                    ackCharacteristic = service.getCharacteristic(CHAR_STATUS_ACK);

                    // Step 1 of GATT setup: Enable Notifications
                    if (ackCharacteristic != null) {
                        try {
                            gatt.setCharacteristicNotification(ackCharacteristic, true);
                            BluetoothGattDescriptor descriptor = ackCharacteristic.getDescriptor(CLIENT_CONFIG_DESCRIPTOR);
                            if (descriptor != null) {
                                descriptor.setValue(BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE);
                                gatt.writeDescriptor(descriptor);
                                // MTU and priority will be chained in onDescriptorWrite!
                                return;
                            }
                        } catch (SecurityException ignored) {}
                    }

                    // Fallback if no descriptor
                    try {
                        gatt.requestMtu(256);
                    } catch (SecurityException ignored) {}
                } else {
                    listener.onLog("WARN", "GATT Service not found, retrying discovery...");
                    mainHandler.postDelayed(() -> {
                        if (bluetoothGatt != null) {
                            try { bluetoothGatt.discoverServices(); } catch (SecurityException ignored) {}
                        }
                    }, 500);
                }
            } else {
                listener.onLog("ERR", "Services discovery failed: " + status);
            }
        }

        @Override
        public void onDescriptorWrite(BluetoothGatt gatt, BluetoothGattDescriptor descriptor, int status) {
            // Step 2 of GATT setup: Request higher MTU after descriptor write finishes
            mainHandler.post(() -> listener.onLog("INFO", "GATT Notification descriptor registered."));
            try {
                gatt.requestMtu(256);
            } catch (SecurityException ignored) {}
        }

        @Override
        public void onMtuChanged(BluetoothGatt gatt, int mtu, int status) {
            // Step 3 of GATT setup: Configure priority and establish link
            try {
                gatt.requestConnectionPriority(BluetoothGatt.CONNECTION_PRIORITY_BALANCED);
            } catch (SecurityException ignored) {}

            mainHandler.post(() -> {
                listener.onLog("INFO", "MTU negotiated: " + mtu + " bytes. BLE link stable.");
                listener.onConnectionStateChange("CONNECTED", "RPMLOOP", currentRssi);
                listener.onLog("INFO", "RPMLOOP Nav Link Ready & Synchronized!");
                startRssiPoller();

                // Send HELLO handshake
                writeJsonCommand("{\"v\":1,\"type\":\"HELLO\",\"seq\":1,\"sid\":\"android_m31\"}");
            });
        }

        @Override
        public void onCharacteristicChanged(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic) {
            if (CHAR_STATUS_ACK.equals(characteristic.getUuid())) {
                byte[] data = characteristic.getValue();
                if (data != null && data.length > 0) {
                    String str = new String(data, StandardCharsets.UTF_8);
                    mainHandler.post(() -> listener.onAckReceived(str));
                }
            }
        }

        @Override
        public void onReadRemoteRssi(BluetoothGatt gatt, int rssi, int status) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                currentRssi = rssi;
                mainHandler.post(() -> listener.onRssiUpdated(rssi));
            }
        }
    };

    public boolean writeJsonCommand(String json) {
        if (!isConnected || bluetoothGatt == null || cmdCharacteristic == null) {
            return false;
        }

        try {
            cmdCharacteristic.setValue(json.getBytes(StandardCharsets.UTF_8));
            cmdCharacteristic.setWriteType(BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE);
            return bluetoothGatt.writeCharacteristic(cmdCharacteristic);
        } catch (SecurityException e) {
            listener.onLog("ERR", "Write failed: " + e.getMessage());
            return false;
        }
    }

    private void startRssiPoller() {
        stopRssiPoller();
        rssiPoller = new Runnable() {
            @Override
            public void run() {
                if (isConnected && bluetoothGatt != null) {
                    try {
                        bluetoothGatt.readRemoteRssi();
                    } catch (SecurityException ignored) {}
                    mainHandler.postDelayed(this, 15000);
                }
            }
        };
        mainHandler.postDelayed(rssiPoller, 3000);
    }

    private void stopRssiPoller() {
        if (rssiPoller != null) {
            mainHandler.removeCallbacks(rssiPoller);
            rssiPoller = null;
        }
    }
}

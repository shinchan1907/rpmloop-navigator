# RPMLOOP Navigator — Android Companion App

A production-grade Android companion application for the RPMLOOP motorcycle navigation system. Engineered for high-contrast daylight visibility, offline map capability, and seamless hardware synchronization with the ESP32-S3 over Bluetooth Low Energy (BLE).

---

## 🌟 Key Features

1. **Official Google Maps Offline Mirroring:**
   - Intercepts turn-by-turn navigation notifications from official Google Maps via `GoogleMapsNotificationService`.
   - **Works 100% offline** when using downloaded Google Maps offline areas.
   - Decodes maneuver types (`TURN_RIGHT`, `SLIGHT_LEFT`, `ROUNDABOUT`, `UTURN`, `DESTINATION`), distance to next turn, and road names, transmitting them instantly to the ESP32-S3 hardware display.

2. **In-App Direct Routing & Search:**
   - Integrated Photon / OpenStreetMap geocoder with instant `Enter` key and `ROUTE` button triggers.
   - OSRM driving engine calculating route geometry, distance, and duration.
   - Dual-action route launch:
     - 🚗 **START NAVIGATION (IN-APP):** Live driving HUD on-screen with real-time turn guidance streamed to the ESP32.
     - 📍 **START IN GOOGLE MAPS:** 1-tap intent launch into the official Google Maps app.

3. **High-Contrast Daylight Light Theme:**
   - Crisp white cards (`#FFFFFF`), dark slate text (`#0F172A`), Google blue accents (`#1A73E8`), and green status indicators (`#10B981`) designed specifically for direct sunlight visibility on motorcycle handlebar mounts.

4. **Continuous 2-Second Phone GPS Telemetry:**
   - Real hardware GPS coordinates (`lat`, `lon`, `speed_kmh`, `heading`, `accuracy`) streamed over BLE every 2 seconds.
   - Zero placeholder/simulated data — displays actual vehicle position on both mobile and desktop simulator.

5. **Robust BLE GATT Synchronization:**
   - Specially optimized GATT handshake for Samsung Galaxy (M31/M32/S-series) and Android 12-14 devices, eliminating GATT 133 connection drops.
   - Auto-reconnect engine and persistent device pairing.

6. **Offline Tile Storage (IndexedDB):**
   - 1-tap "Cache Visible Area Offline" stores OpenStreetMap / CartoDB Voyager tiles locally for zero-data mountain rides.

7. **Hardware Remote Controls:**
   - Remote LCD backlight brightness slider (0–100%) sending framed packets to the ESP32.
   - Hardware ping and diagnostic telemetry logging.

---

## 📂 Source Code Structure

```
android/
├── AndroidManifest.xml         # Permissions (Fine GPS, BLE Connect/Scan, Notification Listener)
├── assets/
│   └── index.html              # High-contrast Leaflet UI, OSRM routing, Google sync panel
├── res/                        # App icons, splash screens, activity layouts
├── src/com/sunnygupta/rpmloopnav/
│   ├── MainActivity.java       # WebView bridge, location listener, permission dispatcher
│   ├── BleManager.java         # Android BLE GATT connection manager & sequential queuing
│   └── GoogleMapsNotificationService.java # Notification listener for Google Maps turn extraction
├── build.sh                    # WSL/Linux one-click AAPT2 & D8 APK build script
└── RPMLoopNav.apk              # Ready-to-install signed debug APK
```

---

## 🚀 Installation & Setup

### Pre-built APK
Download and install [`RPMLoopNav.apk`](RPMLoopNav.apk) directly on your Android phone.

### Permissions Setup
On first launch, grant the requested permissions:
1. **Location:** Select **"While using the app"** and enable **"Precise Location"**.
2. **Nearby Devices / Bluetooth:** Grant permission to discover and connect to the RPMLOOP display.
3. **Notification Access (For Google Maps Mirroring):**
   - Tap **"GRANT NOTIFICATION ACCESS"** in the Google Sync tab.
   - Toggle **RPMLoop Nav** to **ON** in Android's "Notification Access" settings.

---

## 🛠️ Building from Source

The app uses a lightweight build pipeline using Android SDK CLI tools (`aapt2`, `d8`, `apksigner`):

```bash
# In WSL Ubuntu or Linux with Android Build-Tools installed:
cd companion_app/android
chmod +x build.sh
./build.sh
```

The output APK is generated at `RPMLoopNav.apk`.

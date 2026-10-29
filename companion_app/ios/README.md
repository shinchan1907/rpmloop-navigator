# RPMLOOP Navigator — iOS Companion App

A native SwiftUI companion application built for iOS 16+ devices (iPhone). Manages the CoreBluetooth BLE GATT client, streams live vehicle telemetry and route maneuvers to the RPMLOOP ESP32-S3 hardware, and provides full dashboard diagnostics.

---

## 🌟 Key Features

1. **Native SwiftUI Architecture:**
   - 100% pure SwiftUI UI components using modern declarative design.
   - Dynamic dark/light adaptive color schemes.
   - Views:
     - `NavDashboardView.swift`: Live HUD speedometer, turn arrows, distance bar, road name.
     - `ConnectionView.swift`: BLE scanning, signal strength meter (RSSI), auto-reconnect.
     - `DiagnosticsView.swift`: Framed serial ACK telemetry, packet loss counter, latency RTT.
     - `SettingsView.swift`: Display backlight brightness, auto-night mode, units (km/h vs mph).
     - `SimulationView.swift`: Route demo playback for offline bench-testing.

2. **CoreBluetooth BLE Engine (`BLEManager.swift`):**
   - Discovers RPMLOOP advertisement packets with custom 128-bit Service UUID (`4E415601-5250-4D4C-6F70-704E61760001`).
   - Automatically negotiates optimal MTU and connection intervals.
   - Background Bluetooth central mode enabled for continuous navigation guidance when the iPhone is locked or in a pocket.

3. **CoreLocation Integration (`NavigationEngine.swift`):**
   - High-accuracy GPS tracking with automotive navigation filter.
   - Heading calculation with magnetic declination compensation.

---

## 📂 Source Code Structure

```
ios/
├── Info.plist                  # Background modes (bluetooth-central, location), privacy strings
├── Package.swift               # Swift Package Manager manifest
├── xtool.yml                   # Toolchain packaging definition
├── RPMLoopNav_v2.ipa           # Pre-built, signed IPA ready for Sideloadly / AltStore
└── Sources/RPMLoppNav/
    ├── RPMLoppNavApp.swift     # App entry point
    ├── ContentView.swift       # Tab navigation controller
    ├── BLE/
    │   └── BLEManager.swift    # CoreBluetooth Central Manager & GATT sync
    ├── Models/
    │   └── ProtocolModels.swift # Codable protocol data models (v1)
    ├── Services/
    │   └── NavigationEngine.swift # GPS, heading, and OSRM route parser
    └── Views/
        ├── NavDashboardView.swift
        ├── ConnectionView.swift
        ├── DiagnosticsView.swift
        ├── SettingsView.swift
        └── SimulationView.swift
```

---

## 📲 Sideloading onto iPhone

You can install `RPMLoopNav_v2.ipa` without an Apple Developer account using Sideloadly or AltStore:

### Using Sideloadly (Windows / macOS)
1. Download and install [Sideloadly](https://sideloadly.io/).
2. Connect your iPhone via USB cable and trust the computer.
3. Drag [`RPMLoopNav_v2.ipa`](RPMLoopNav_v2.ipa) into Sideloadly.
4. Enter your Apple ID and click **Start**.
5. On your iPhone, go to **Settings > General > VPN & Device Management** and tap **Trust [Your Apple ID]**.
6. Open **RPMLoop Nav** and pair with your ESP32-S3!

---

## 🛠️ Building with Xcode

```bash
# Open Package.swift or generate an Xcode project:
swift package generate-xcodeproj
open RPMLoppNav.xcodeproj
```
Select your connected iPhone, choose your Personal Team signing certificate, and click **Build & Run** (`Cmd + R`).

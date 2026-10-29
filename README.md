<div align="center">

# RPMLOOP Navigator

### Your next turn. Your own hardware.

An open-source motorcycle navigation dashboard, designed around the Royal Enfield Hunter 350.

[![License: MIT](https://img.shields.io/badge/License-MIT-65d9e8?style=flat-square)](LICENSE)
[![Hardware: P3 prototype](https://img.shields.io/badge/Hardware-P3%20prototype-222831?style=flat-square)](printing_files/README.md)
[![Platform: ESP32-S3](https://img.shields.io/badge/Platform-ESP32--S3-222831?style=flat-square)](#the-platform)
[![Companion: Android & iOS](https://img.shields.io/badge/Companion-Android%20%7C%20iOS-34D399?style=flat-square)](companion_app/)

![RPMLOOP Navigator — map UI design visualization](docs/images/hero-map-day.png)

**A compact instrument for the ride ahead.**

[Android APK](companion_app/android/RPMLoopNav.apk) · [iOS IPA](companion_app/ios/RPMLoopNav_v2.ipa) · [Desktop Simulator](simulator/) · [Dual Firmware](firmware/) · [Printing Files](printing_files/) · [Hardware Pinout](hardware/) · [Contributing](CONTRIBUTING.md)

</div>

RPMLOOP Navigator brings a dedicated navigation display to the motorcycle cockpit. The architecture is modular and local-first: your smartphone handles high-accuracy GPS tracking, geocoding, and routing (including **100% offline Google Maps turn mirroring**), an ESP32-S3 renders the turn-by-turn guidance and dashboard telemetry, and a purpose-built enclosure keeps the hardware compact, serviceable, and weather-resistant.

---

## 🗺️ Complete System Architecture

```mermaid
flowchart TD
    subgraph Mobile Companion Apps
        Android["Android Companion App\n(Java + Leaflet + Offline Tiles)\n• Google Maps Notification Mirroring\n• Photon Geocoder & OSRM Engine\n• 2s Real GPS Telemetry Stream"]
        iOS["iOS Companion App\n(Native SwiftUI + CoreBluetooth)\n• CoreLocation Engine\n• Diagnostics & Simulation"]
    end

    subgraph Wireless Link
        BLE["Bluetooth Low Energy (BLE 5.0 GATT)\nService UUID: 4E415601-5250-4D4C-6F70-704E61760001\nVersioned JSON Protocol (v1)"]
    end

    subgraph ESP32-S3 Microcontroller
        MCU["Waveshare ESP32-S3 Mini / Zero"]
        LED["Onboard WS2812 Mini RGB LED\n🔴 Red: Disconnected / Standby\n🔵 Blue: Connected & Active"]
    end

    subgraph Target 1: Production Motorcycle Hardware
        PhysicalLCD["2.8-inch SPI TFT LCD (ST7789/ILI9341)\nNative 320x240 Resolution\n• 5 kHz PWM Backlight Dimming\n• P3 3D-Printed Weatherproof Case"]
    end

    subgraph Target 2: Desktop Simulator & Bench Lab
        Bridge["Python Bridge Server (bridge.py)\n• COBS & CRC-16 USB CDC Serial\n• WebSocket Broadcast on port 8080\n• Local Wi-Fi APK Distribution"]
        BrowserSim["Web Browser Simulator\n• Virtual 320x240 LCD HUD\n• Real-Time Leaflet GPS Phone Tracker\n• Raw Diagnostic Stream & Replay"]
    end

    Android <-->|BLE GATT| BLE
    iOS <-->|BLE GATT| BLE
    BLE <--> MCU
    MCU --> LED

    MCU -->|Direct SPI Bus\n(firmware/production_hardware)| PhysicalLCD
    MCU <-->|USB CDC Framed Serial\n(firmware/simulator_bridge)| Bridge
    Bridge <--> BrowserSim
```

---

## 📂 Repository Directory Map

```
rpmloop-navigator/
├── companion_app/              # Mobile Smartphone Companion Apps
│   ├── android/                # Native Android App (AAPT2/D8 build, Google Maps sync, APK)
│   ├── ios/                    # Native iOS SwiftUI App (CoreBluetooth, Sideloadly IPA)
│   └── README.md               # Mobile companion guide & comparison
├── firmware/                   # ESP32-S3 Dual Firmware Suite
│   ├── protocol/               # Official Protocol Specification & Test Fixtures
│   ├── simulator_bridge/       # Firmware 1: USB Framed Serial to Desktop Simulator + WS2812 LED
│   ├── production_hardware/    # Firmware 2: Direct 2.8" SPI TFT LCD (ST7789/ILI9341) Driver
│   └── README.md               # Firmware architecture & flashing guide
├── simulator/                  # Desktop Browser Simulator & Hardware Bridge
│   ├── dashboard/              # Virtual 320x240 LCD HUD + Live Leaflet Phone GPS Tracker
│   ├── bridge/                 # Python Starlette/Uvicorn Serial-to-WebSocket bridge
│   └── README.md               # Simulator setup & usage instructions
├── hardware/                   # Electronics & Electrical Integration
│   └── README.md               # Full GPIO pin mapping, power consumption & USB-C specs
├── printing_files/             # P3 Enclosure 3D Printing & Manufacturing Package
│   ├── 01_Rigid_STL/           # 8 rigid ASA parts (housing, bezel, lid, cradle, tabs)
│   ├── 02_Flexible_Prototype_STL/ # Flexible gasket & sealing washer templates
│   ├── 03_Images/              # Assembly diagrams & individual part renders
│   ├── 04_CAD/                 # SolidCraft models & STEP interchange files
│   ├── 05_Verification/        # Mesh verification reports & clearance checks
│   ├── 06_Source/              # Parametric CAD generator scripts
│   └── README.md               # Slicing guidelines, tolerances, and hardware notes
├── docs/                       # Interactive HTML mockups, diagrams, and hero imagery
└── rendering/                  # Blender studio scenes and product render scripts
```

---

## 📱 Mobile Companion Apps

The smartphone handles the cellular connection, geocoding search, and GPS satellite fix, keeping the motorcycle display lightweight and power-efficient.

### 🤖 Android Companion App ([`companion_app/android/`](companion_app/android/))
- **Pre-built APK:** Download [`RPMLoopNav.apk`](companion_app/android/RPMLoopNav.apk) *(66 KB)* directly.
- **Google Maps Offline Sync:** Intercepts turn-by-turn directions from the official Google Maps app via `GoogleMapsNotificationService`. Works **100% offline** with downloaded offline map areas.
- **In-App Navigation:** Integrated Photon search with instant `ROUTE` button, OSRM route calculations, and high-contrast daylight UI.
- **Real Phone GPS Stream:** Broadcasts live phone coordinates (`lat`, `lon`, `speed_kmh`, `heading`, `accuracy`) over BLE every 2 seconds.
- **Samsung Bluetooth Stability:** Sequenced GATT queue eliminating GATT 133 connection drops on Samsung Galaxy M/A/S-series phones.
- **Offline Map Tile Storage:** Integrated IndexedDB tile cache for zero-data riding.

### 🍎 iOS Companion App ([`companion_app/ios/`](companion_app/ios/))
- **Pre-built IPA:** Install [`RPMLoopNav_v2.ipa`](companion_app/ios/RPMLoopNav_v2.ipa) *(267 KB)* via Sideloadly or AltStore.
- **SwiftUI Architecture:** Clean declarative interface with live speedometer, maneuver cards, and telemetry logs.
- **CoreBluetooth Central:** Seamless pairing to `RPMLOOP` BLE peripheral with background navigation modes.

---

## ⚡ ESP32-S3 Dual Firmware Suite

The firmware is split into two specialized builds based on your development stage:

| Feature | 1. Simulator Bridge Firmware | 2. Production Hardware Firmware |
|---|---|---|
| **Directory** | [`firmware/simulator_bridge/`](firmware/simulator_bridge/) | [`firmware/production_hardware/`](firmware/production_hardware/) |
| **Target Output** | USB CDC Framed Serial (`bridge.py`) | 2.8" SPI TFT LCD (ST7789/ILI9341) |
| **WS2812 Status LED** | 🔴 Red: Disconnected / 🔵 Blue: Connected | 🔴 Red: Disconnected / 🔵 Blue: Connected |
| **Use Case** | Bench testing without physical LCD wiring | Motorcycle cockpit installation |
| **Telemetry** | Streams `ACCEPTED_STATE` (`lat`, `lon`, `speed`) | Renders Google Maps styled vector HUD |
| **Protocol** | Protocol v1 (JSON over BLE & Serial) | Protocol v1 (JSON over BLE & Serial) |

### Flashing via PlatformIO
```bash
# To flash Simulator Bridge Firmware:
cd firmware/simulator_bridge && pio run --target upload

# To flash Production Hardware Firmware:
cd firmware/production_hardware && pio run --target upload
```

---

## 🖥️ Desktop Simulator & Web UI

Test the complete navigation flow right on your PC without touching a soldering iron:

1. Connect the ESP32-S3 via USB-C.
2. Start the bridge:
   ```bash
   cd simulator/bridge
   pip install -r requirements.txt
   python bridge.py
   ```
3. Open **`http://localhost:8080/`** in your browser.
4. Open the mobile companion app on your phone and tap **PAIR**. The virtual display and live Leaflet GPS tracker will immediately mirror your phone's movements!

---

## 📌 Hardware Wiring Pinout (Waveshare ESP32-S3 to 2.8" LCD)

| Signal Name | ESP32-S3 GPIO | 2.8" LCD Pin | Function |
|---|---|---|---|
| **LCD_MOSI** | GPIO 11 | SDI / MOSI | SPI Master Data Out |
| **LCD_MISO** | GPIO 13 | SDO / MISO | SPI Master Data In |
| **LCD_SCLK** | GPIO 12 | SCK / CLK | SPI Clock |
| **LCD_CS** | GPIO 10 | CS | Display Chip Select (Active Low) |
| **LCD_DC** | GPIO 9 | DC / RS | Data / Command Select |
| **LCD_RST** | GPIO 8 | RESET | Display Hardware Reset |
| **LCD_BL** | GPIO 7 | LED / BL | 5 kHz PWM Backlight Dimming |
| **TOUCH_CS** | GPIO 6 | T_CS | Touch Controller Chip Select |
| **TOUCH_IRQ** | GPIO 5 | T_IRQ | Touch Pen Interrupt |
| **SD_CS** | GPIO 4 | SD_CS | MicroSD Chip Select |
| **WS2812** | GPIO 21 | Onboard RGB | Red: Disconnected / Blue: Connected |
| **VCC / GND** | 5V / GND | VCC / GND | System Power |

---

## 🖨️ 3D Printing Files & Enclosure

Start with **[printing_files/README.md](printing_files/README.md)** for part quantities, materials, wall thicknesses, and tolerances:

| Folder | Contents |
|---|---|
| [`01_Rigid_STL`](printing_files/01_Rigid_STL/) | Housing, bezel, rear lid, ESP32 cradle, handed retaining tabs and insert coupon |
| [`02_Flexible_Prototype_STL`](printing_files/02_Flexible_Prototype_STL/) | Gasket/washer templates and millimetre-scale SVG outlines |
| [`03_Images`](printing_files/03_Images/) | Individual part images and technical assembly views |
| [`04_CAD`](printing_files/04_CAD/) | Native SolidCraft assembly and STEP interchange files |
| [`05_Verification`](printing_files/05_Verification/) | Digital mesh and interference evidence |
| [`06_Source`](printing_files/06_Source/) | CAD generators and frozen command scripts |

Print rigid parts in **ASA** for UV and weather resistance. Use **TPU / Silicone** for gaskets and sealing washers.

---

## 🚀 Quickstart for Makers & AI Agents

To pick up this repository and get a complete running system:

```bash
# 1. Clone the repository
git clone https://github.com/shinchan1907/rpmloop-navigator.git
cd rpmloop-navigator

# 2. Install the Android Companion App
# Either copy companion_app/android/RPMLoopNav.apk to your phone
# Or build from source in WSL/Linux:
cd companion_app/android && ./build.sh && cd ../..

# 3. Flash the ESP32-S3 Simulator Firmware
cd firmware/simulator_bridge && pio run -t upload && cd ../..

# 4. Launch Desktop Simulator & Bridge
cd simulator/bridge && python bridge.py
# Open http://localhost:8080/ in your browser
```

---

## License

Published under the [MIT License](LICENSE). Third-party components, tools, and trademarks retain their respective ownership. Royal Enfield, Waveshare, ESP32, USB-C, and Google Maps references identify intended compatibility; this project is independent and community-driven.

---

<div align="center">
  <strong>RPMLOOP Navigator</strong><br>
  Open hardware. Built to evolve.<br>
  <a href="https://www.instagram.com/rpmloop/">@rpmloop</a>
</div>

# RPMLOOP Navigator

An open-source, motorcycle handlebar cockpit navigator engineered for the **Royal Enfield Hunter 350** (and universal motorcycle handlebars), powered by an **ESP32-S3**, **2.8" ST7789V SPI Display**, and **HR2046 Resistive Touchscreen**.

Unlike basic turn-by-turn notification displays, **RPMLOOP Navigator** streams full, live **Wireless Android Auto (Google Maps Navigation)** in real time over Wi-Fi with zero lag, interactive touch zooming, and crisp daylight visibility.

---

## 🌟 Key Features

* **Live Wireless Android Auto:** Connects wirelessly to your Android smartphone over Wi-Fi and streams real-time Google Maps H.264 video feed directly to your handlebars.
* **Full-Screen Navigation View:** Android Auto side rail is automatically cropped out so the full 320×240 display is 100% dedicated to map directions, turns, ETA, and lane guidance.
* **Interactive Touchscreen (Map Zoom & Pan):** Dedicated HR2046 resistive digitizer on a separate SPI bus (`SPI3`) allows you to tap the map to zoom in (`+`), zoom out (`-`), pan across the route, and re-center.
* **Daylight High-Contrast Theme:** Tuned for bright outdoor daylight riding with Google Maps' signature light green parks, clean roads, and high-visibility direction cards.
* **Custom Automotive Setup Screen:** Clean setup dashboard showing connection stepper (`WI-FI` → `PHONE` → `LINK` → `MAPS`), active hotspot credentials, and real-time status.
* **Ultra-Debloated Architecture:** Custom-engineered to fit and execute smoothly on cost-effective **ESP32-S3FH4R2** hardware (**4 MB Flash, 2 MB PSRAM**):
  * Reused display framebuffer for UI sprites (saving 153 KB PSRAM).
  * Lightweight 2-slot H.264 access unit pipeline with parallel dual-core YUV scaler.
  * Internal RAM optimization for TLS record buffers and AES-GCM decryption.
* **Weatherproof Handlebar Enclosure:** Precision-designed 3D printable housing with silicone sealing gaskets, cradle mount, and Hunter 350 handlebar clamp (available in [`printing_files/`](printing_files/)).

---

## ⚡ Hardware Specifications

| Component | Specification |
|---|---|
| **Microcontroller** | ESP32-S3 (ESP32-S3FH4R2: Dual-Core Xtensa @ 240 MHz, 4 MB Flash, 2 MB Quad PSRAM) |
| **Display Panel** | 2.8" SPI TFT LCD (ST7789V driver, 320×240 resolution, landscape mode) |
| **Touch Controller** | HR2046 / XPT2046 Resistive Touch Digitizer on dedicated `SPI3_HOST` |
| **Display Library** | LovyanGFX v1.2+ (Optimized DMA & sub-core parallel scaler) |
| **Wireless Protocol** | Wi-Fi 802.11 b/g/n (SoftAP / Hotspot) + TLS AES-GCM + Android Auto AAP |
| **Video Decoder** | Software H.264 Baseline Profile decoder (`esp_h264` / `tinyh264`) |

---

## 🔌 Pinout Mapping

```text
ESP32-S3 Wiring
├── Display (SPI2_HOST)
│   ├── SCK   ────── GPIO 6
│   ├── MOSI  ────── GPIO 7
│   ├── MISO  ────── GPIO 8
│   ├── CS    ────── GPIO 9
│   ├── DC    ────── GPIO 10
│   ├── RST   ────── GPIO 11
│   └── BL    ────── 3.3V (Hardwired / On)
└── Resistive Touch (SPI3_HOST)
    ├── T_CLK ────── GPIO 1
    ├── T_DIN ────── GPIO 2 (MOSI)
    ├── T_DO  ────── GPIO 3 (MISO)
    ├── T_IRQ ────── GPIO 4
    └── T_CS  ────── GPIO 5
```

---

## 🚀 Quick Start (1-Click Flashing)

If you just want to flash the pre-compiled firmware without installing ESP-IDF:

1. Connect your ESP32-S3 board to your PC via USB.
2. Open terminal in [`firmware/bin/`](firmware/bin/).
3. **On Windows:**
   ```cmd
   flash.bat
   ```
4. **On Linux / macOS:**
   ```bash
   chmod +x flash.sh
   ./flash.sh
   ```
5. Enter your COM port (e.g., `COM3` on Windows or `/dev/ttyACM0` on Linux).

---

## 🛠️ Building from Source (ESP-IDF)

### Prerequisites
* **ESP-IDF v5.3+** (or v6.x) installed and configured in your environment.

### Build and Flash
```bash
cd firmware

# Set target to ESP32-S3
idf.py set-target esp32s3

# Build project
idf.py build

# Flash to device and start serial monitor
idf.py -p PORT flash monitor
```

---

## 📱 Phone Setup & Connection Guide

### 1. Wi-Fi Hotspot Setup
* Power up the board. The setup screen will display:
  * **Hotspot SSID:** `RPL Loop`
  * **Password:** `88888888`
* Connect your Android phone's Wi-Fi to **RPL Loop** (or turn on your phone's Personal Hotspot with SSID `RPL Loop` and password `88888888`).

### 2. Enable Android Auto Developer Mode & Start Headunit Server
1. On your Android phone, go to **Settings** → search for **Android Auto**.
2. Scroll to the bottom and tap **Version and permission info** 10 times consecutively until Developer Mode is unlocked.
3. Tap the **3 dots** icon in the top right corner.
4. Tap **Start headunit server**.
5. Within 2–3 seconds, the RPMLOOP screen will progress (`WI-FI` → `PHONE` → `LINK` → `MAPS`) and launch Google Maps navigation!

### 3. Setting Daylight (Green/Light) Map Colors
By default, Android Auto on Samsung and other Android devices may follow your phone's system Dark Theme. To enforce bright daylight navigation:
* **Android Auto:** Phone Settings → Android Auto → Display → **"Day/Night mode for maps"** → select **Day**.
* **Google Maps:** Google Maps app → Settings → Navigation settings → Map display → **"Color scheme"** → select **Day**.

### 4. Interactive Touch Control
* Because the digitizer is **Resistive (HR2046)**, operate it with slight pressure using your **fingernail or a stylus** (unlike capacitive smartphone glass).
* Tapping the screen reveals the **`+`** and **`-`** zoom controls to zoom in and out of the map without altering the direction text banner size.

---

## 🏍️ 3D Printing & Enclosure Files

All 3D CAD models, manufacturing step files, and verification assets for the handlebar-mounted enclosure are located in [`printing_files/`](printing_files/):

* `01_STL/`: Ready-to-slice 3D print models (`housing.stl`, `front_bezel.stl`, `rear_lid.stl`, `esp32_cradle.stl`).
* `02_Rubber_Gaskets/`: Silicone gasket CAD models for IP-rated rain and dust sealing (`display_gasket.step`, `joint_gasket.step`).
* `03_Images/`: Dimensional renders and mechanical cross-sections.
* `04_CAD/`: Full parametric STEP assembly models.

---

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

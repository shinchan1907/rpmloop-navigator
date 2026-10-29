# RPMLOOP Navigator — Hardware & Electronics

This section details the physical electronics, microcontroller pinouts, power delivery, and enclosure integration for the RPMLOOP motorcycle navigator.

---

## 🛠️ Bill of Materials (BOM)

| Item | Component | Specification | Notes |
|---|---|---|---|
| **MCU** | Waveshare ESP32-S3 Mini / Zero | Dual-Core 240 MHz, 4MB Flash, 2MB PSRAM, BLE 5.0 | Fits in rotated cradle behind display |
| **Display** | 2.8-inch SPI TFT LCD | 320 × 240 pixels (RGB), ST7789 or ILI9341 controller | 85 × 48 mm PCB footprint |
| **Touch** | Resistive Touchscreen (Optional) | XPT2046 SPI controller | Handled via SPI pins |
| **Enclosure** | P3 3D Printed Assembly | 8 rigid ASA printed parts + TPU seal templates | See [`printing_files/`](../printing_files/) |
| **Mounting** | Action-Camera Two-Finger Mount | Integrated M5 through-hole | Clamps to handlebar ball-mount |
| **Fasteners** | Brass Heat-Set Inserts | M2 × 4mm (OD 3.5mm) | Melted into bezel and lid bosses |
| **Screws** | Stainless M2 Socket Cap Screws | M2 × 8mm & M2 × 12mm | Secures bezel and rear lid |
| **Power** | USB-C 5V Motorcycle Outlet | Right-angle low-profile USB-C cable | Enters through bottom 10.5 × 4.2mm slot |

---

## 📌 ESP32-S3 Pin Mapping Table

| Function | Pin Name | ESP32-S3 GPIO | Connected To |
|---|---|---|---|
| **SPI Data Out** | `MOSI` | GPIO 11 | Display `SDI` / `MOSI` |
| **SPI Data In** | `MISO` | GPIO 13 | Touch `SDO` / MicroSD `MISO` |
| **SPI Clock** | `SCLK` | GPIO 12 | Display `SCK` / `CLK` |
| **Display Select** | `CS` | GPIO 10 | Display `CS` (Active Low) |
| **Display Data/Cmd**| `DC` | GPIO 9 | Display `DC` / `RS` |
| **Display Reset** | `RST` | GPIO 8 | Display `RESET` |
| **Backlight PWM** | `BL` | GPIO 7 | Display `LED` / `BL` (5 kHz PWM) |
| **Touch Select** | `T_CS` | GPIO 6 | Touch `T_CS` |
| **Touch Interrupt** | `T_IRQ` | GPIO 5 | Touch `T_IRQ` |
| **MicroSD Select** | `SD_CS` | GPIO 4 | MicroSD `CS` |
| **Onboard Status** | `RGB` | GPIO 21 | WS2812 GRB Mini LED |
| **Main Power** | `5V` | 5V In / VBUS | Motorcycle USB-C Outlet |
| **Ground** | `GND` | GND | System Common Ground |

---

## ⚡ Power Supply & Waterproofing

1. **Power Ingress:** Power is delivered directly via the bottom USB-C slot from the motorcycle's handlebar USB socket. A flexible TPU/silicone gasket seals the cable plug against road spray and rain.
2. **Backlight Current:** At 100% duty cycle, the 2.8" LCD backlight draws ~65mA. The ESP32-S3 draws ~120mA with BLE active, resulting in a total operating current of under 200mA (easily powered by any 5V 1A motorcycle USB port).
3. **Internal Expansions:** The P3 enclosure reserves physical interior volume for a future battery pack and an M8M active GPS antenna assembly.

---

## 🖨️ 3D Printing Files

All CAD models (`.step`, `.solidcraft`), individual print STLs, and slicing instructions are located in:
👉 **[`printing_files/README.md`](../printing_files/README.md)**

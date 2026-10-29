# RPMLOOP Firmware — Production Hardware (Physical 2.8" SPI LCD)

This is the production firmware flashed directly onto the ESP32-S3 microcontroller when assembled inside the physical RPMLOOP P3 3D-printed enclosure with the 2.8-inch SPI TFT LCD.

---

## 🖥️ Hardware Specifications

- **Microcontroller:** Waveshare ESP32-S3 Mini / Zero (Dual-Core Xtensa LX7 @ 240 MHz, 2.4 GHz BLE 5.0)
- **Display Module:** 2.8-inch SPI TFT LCD (320 × 240 pixels landscape / 240 × 320 portrait)
- **Controller IC:** ST7789V / ILI9341
- **Touch Controller:** XPT2046 Resistive Touch via SPI
- **Storage:** Internal SPI MicroSD card slot
- **Status LED:** Onboard WS2812 mini RGB LED on GPIO 21

---

## 📌 Wiring Pinout (Waveshare ESP32-S3 to 2.8" LCD)

| Signal Name | ESP32-S3 GPIO | 2.8" LCD Pin | Description |
|---|---|---|---|
| **LCD_MOSI** | GPIO 11 | SDI / MOSI | SPI Master Data Out |
| **LCD_MISO** | GPIO 13 | SDO / MISO | SPI Master Data In (Touch / SD) |
| **LCD_SCLK** | GPIO 12 | SCK / CLK | SPI Clock |
| **LCD_CS** | GPIO 10 | CS | Display Chip Select (Active Low) |
| **LCD_DC** | GPIO 9 | DC / RS | Command / Data Select |
| **LCD_RST** | GPIO 8 | RESET | Display Hardware Reset |
| **LCD_BL** | GPIO 7 | LED / BL | 5 kHz PWM Backlight Dimming |
| **TOUCH_CS** | GPIO 6 | T_CS | Touch Controller Chip Select |
| **TOUCH_IRQ** | GPIO 5 | T_IRQ | Touch Pen Down Interrupt |
| **SD_CS** | GPIO 4 | SD_CS | MicroSD Card Chip Select |
| **WS2812** | GPIO 21 | Onboard RGB | Red: Disconnected / Blue: Connected |
| **VCC** | 3.3V / 5V | VCC | Regulated DC Power |
| **GND** | GND | GND | System Ground |

---

## ⚡ Flashing Instructions

```bash
cd firmware/production_hardware
pio run --target upload
```

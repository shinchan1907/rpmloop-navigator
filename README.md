# RPMLOOP Navigator

**Status: codebase reset on 2026-11-02.** All previous firmware, companion-app,
simulator and docs code was removed at the owner's direction for a
from-scratch rebuild. Only `printing_files/` (the Hunter 350 enclosure print
package) is retained. The old code (up to firmware 2.4.2) remains fully
recoverable from this repo's git history (last old-code commit `8398831`).

## Hardware (bench unit, verified)

- **MCU:** ESP32-S3 Mini dev board, **4 MB flash**, PSRAM fitted
- **Display module:** generic red 2.8" SPI TFT, 240×320, resistive touch
  - Panel: **ST7789V** (not ILI9341), landscape via rotation=1 → 320×240
  - LovyanGFX: `Panel_ST7789`, invert=false, SPI write 16 MHz
- **Touch:** HR2046 on its own SPI bus (SPI3_HOST); measured calibration
  raw X 270–3780, raw Y 280–3650, axis-swapped, offset_rotation=4

## Pin connections (the ONLY peripherals in the new build)

| Signal | GPIO |
|---|---|
| Display SCK | 6 |
| Display MOSI | 7 |
| Display MISO | 8 |
| Display CS | 9 |
| Display DC | 10 |
| Display RESET | 11 |
| Touch T_CLK | 1 |
| Touch T_DIN | 2 |
| Touch T_DO | 3 |
| Touch T_IRQ | 4 |
| Touch T_CS | 5 |

Display VCC/LED → 3V3 (backlight hardwired, no PWM — brightness is done in
software). **No SD card and no SD module in this build.** Touch IRQ (GPIO4)
must be used by the new firmware (interrupt-driven touch).

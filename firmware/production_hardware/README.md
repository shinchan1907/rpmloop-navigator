# RPMLOOP Firmware — Production Hardware (Physical 2.8" SPI LCD + LVGL UI)

Firmware flashed onto the ESP32-S3 in the physical RPMLOOP unit: a BLE
peripheral speaking protocol v1 to the companion apps, plus a complete
embedded LVGL UI on the 2.8" panel (8 screens, touchscreen input, mock
data provider for standalone bench demos).

**Product scope (final):** the device is ONLY the ESP32-S3 + this display
module. All navigation data comes from the phone companion apps over BLE;
the module's touchscreen is the only input. There is no GPS module, no
encoder/buttons/joystick and no SD card in this product.

---

## 🖥️ Hardware (bench-verified 2026-10-29)

- **Microcontroller:** ESP32-S3 (dev: ESP32-S3 Mini; production target:
  ESP32-S3-WROOM-1-N16R8 custom PCB — same pin map)
- **Display Module:** generic red 2.8" SPI TFT, **ILI9341**, 240×320 native,
  used landscape **320×240**
- **Touch:** HR2046 resistive (XPT2046-compatible) on the module — the
  product's **only input**, driven via LovyanGFX `Touch_XPT2046` on the
  shared display SPI bus
- **Status LED:** onboard WS2812 on the original Waveshare target (GPIO 21)
- **Not used:** the module's own microSD slot (back pads) — out of product
  scope, no pins assigned

### Panel quirks (proven on the bench — encoded in `include/lgfx_config.h` / `pinout_config.h`)

| Quirk | Setting |
|---|---|
| Clone panel shows inverted colours | LovyanGFX panel `invert = true` |
| 40 MHz SPI garbles pixels on jumper wires | SPI write clock **16 MHz** (raise on soldered PCB) |
| Plain landscape rotation renders mirrored text | LovyanGFX rotation **5** (`TFT_ROTATION`) |
| Backlight LED pin wired straight to 3V3 | No BL GPIO/PWM — brightness = LVGL dim overlay |

### Touch calibration ⚠️

`TOUCH_CAL_X_MIN/X_MAX/Y_MIN/Y_MAX` in `include/pinout_config.h` are
**unverified presets** (200…3900, typical XPT2046 values) — they have NOT
been measured on this module yet. Before trusting taps:

1. Run LovyanGFX `calibrateTouch()` once on the real module (it shows
   corner targets and returns 8 raw values), or log the raw ADC readings
   at the four screen corners.
2. Put the measured ranges into the four `TOUCH_CAL_*` defines and rebuild.
3. If taps land swapped/mirrored versus the picture, adjust the touch
   `offset_rotation` (0–7) in `include/lgfx_config.h` — not the ranges.

## 📌 Wiring Pinout (ESP32-S3 → 2.8" module)

| Signal | GPIO | Notes |
|---|---|---|
| TFT SCLK | 6 | SPI2 |
| TFT MOSI (SDI) | 7 | |
| TFT MISO (SDO) | 8 | |
| TFT CS | 9 | ⚠️ old Waveshare map had CS=10/DC=9 — swapped, do not mix |
| TFT DC | 10 | |
| TFT RST | 11 | |
| TFT LED / VCC | 3V3 | always on |
| Touch T_CLK | 6 | shares TFT SCLK |
| Touch T_DIN | 7 | shares TFT MOSI |
| Touch T_DO | 8 | shares TFT MISO |
| Touch T_CS | 12 | HR2046 chip select |
| Touch T_IRQ | 13 | PENIRQ, LOW while pressed |

## 🧩 Embedded UI (LVGL 8.3 + LovyanGFX)

- **Screens:** Boot → Home · Nav · Maps · Music · Headset · Tracking · Settings
- **Nav screen:** 104 px dark left panel (turn arrow, distance, street,
  instruction, speed, remaining/ETA), Google-style light map, GPS chip
  top-right, circular recentre button that appears when the map is panned.
- **Map:** procedural `MapView` component (road grid, park/water, route
  polyline #1A73E8 with white casing, position arrow) with pan/zoom/lock —
  rendering lives inside `MapView::render()`, so screens never change if
  the world renderer does.
- **Data seam:** screens only READ `BikeState` (`include/bike_state.h`).
  Providers write it: `MockProvider` today (animated demo data) and the BLE
  handler (`bike_state_apply_nav`) when the phone is navigating — no UI
  changes needed either way. Actions go through the `bike_control_*()` seam.
- **Controls (touch only):** tap Home tiles / buttons to navigate, every
  sub-screen has a Home button (top corner); drag the map to pan, tap the
  on-screen **+ / −** buttons to zoom, tap the recentre button to re-lock
  on position; Music prev/play/next and Tracking start/reset are tap
  buttons; Settings sliders are dragged directly.

## ⚡ Build & Flash

```bash
cd firmware/production_hardware
pio run                 # build (esp32s3_production)
pio run -t upload       # flash over USB-C
```

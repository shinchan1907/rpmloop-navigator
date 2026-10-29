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

- **Microcontroller:** ESP32-S3 Mini (4 MB flash) — the product's only
  target (the 16 MB custom-PCB target was dropped by owner decision)
- **Display Module:** generic red 2.8" SPI TFT, **ST7789V** (chip ID probed
  on hardware: 0x85 0x85 0x52 — long misdiagnosed as ILI9341; the command
  sets overlap, so ILI9341 init produced a readable but wrong picture),
  240×320 native, used landscape **320×240**
- **Touch:** HR2046 resistive (XPT2046-compatible) on the module — the
  product's **only input**, driven via LovyanGFX `Touch_XPT2046` on its
  own SPI pins (SPI3 peripheral, polled — T_IRQ not wired)
- **Status LED:** onboard WS2812 on the original Waveshare target (GPIO 21)
- **Not used:** the module's own microSD slot (back pads) — out of product
  scope, no pins assigned

### Panel quirks (proven on the bench — encoded in `include/lgfx_config.h` / `pinout_config.h`)

| Quirk | Setting |
|---|---|
| Panel is ST7789V (ID-probed), not ILI9341 | LovyanGFX `Panel_ST7789` |
| ST7789V shows true colours with invert OFF (invert=true inverts them) | panel `invert = false` |
| 40 MHz SPI garbles pixels on jumper wires | SPI write clock **16 MHz** |
| Landscape orientation | LovyanGFX rotation **1** (`TFT_ROTATION`) |
| Picture shifted on some ST7789 batches | panel `offset_x`/`offset_y` (0 on this module — the knob if a batch shifts) |
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
| Touch T_CLK | 1 | SPI3, touch has its own pins |
| Touch T_DIN | 2 | ESP32 → touch (MOSI) |
| Touch T_DO | 3 | touch → ESP32 (MISO); S3 strapping pin, safe as input (Hi-Z at boot) |
| Touch T_CS | 5 | HR2046 chip select |
| Touch T_IRQ | — | **not connected** (driver polls) |

## 🧩 Embedded UI (LVGL 8.3 + LovyanGFX)

- **Screens:** Boot → Home · Nav · Maps · Music · Headset · Tracking · Settings
- **Nav screen (2.2.2):** TomTom/Google-style full-bleed 2D nav — the map
  fills all 320×240 (river band, building blocks, route). Over it: a
  two-segment blue instruction banner (maneuver arrow + big distance +
  street, and a "Then" preview of the next maneuver), a left rail of
  round recentre/+/- buttons, a bottom-centre speed cluster (red-ring
  speed-limit disc + current speed) and a remaining-distance/ETA pill.
- **Map:** procedural `MapView` component (road grid, river, buildings,
  park/water, route polyline #1A73E8 with white casing, two-tone position
  chevron) with drag-pan/zoom/lock — rendering lives inside
  `MapView::render()`, so screens never change if the world renderer does.
- **Data seam:** screens only READ `BikeState` (`include/bike_state.h`).
  Providers write it: `MockProvider` today (animated demo data) and the BLE
  handler (`bike_state_apply_nav`) when the phone is navigating — no UI
  changes needed either way. Actions go through the `bike_control_*()` seam.
- **Controls (touch only):** tap Home tiles / buttons to navigate, every
  sub-screen has a Home button (top corner); on Nav/Maps drag the map to
  pan and use the on-screen **+ / −** round buttons to zoom, tap the
  recentre button to re-lock on position; Music prev/play/next and
  Tracking start/reset are tap buttons; Settings sliders are dragged
  directly.

## ⚡ Build & Flash

```bash
cd firmware/production_hardware
pio run                 # build (esp32s3_production)
pio run -t upload       # flash over USB-C
```

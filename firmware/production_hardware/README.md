# RPMLOOP Firmware — Production Hardware (Physical 2.8" SPI LCD + LVGL UI)

Firmware flashed onto the ESP32-S3 in the physical RPMLOOP unit: a BLE
peripheral speaking protocol v1 to the companion apps, plus a complete
embedded LVGL UI on the 2.8" panel (8 screens, touchscreen input). Demo
data is **OFF by default** — the device boots honest and shows real empty
states until the phone feeds it; a MockProvider demo scene is available
behind the Settings "Demo mode" switch.

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

### Touch calibration

`TOUCH_CAL_X_MIN/X_MAX/Y_MIN/Y_MAX` in `include/pinout_config.h` were
**measured on the bench unit on 2026-10-29** (5-point stylus sampling):
raw X 270…3780, raw Y 280…3650. The touch film is axis-swapped versus
the screen (raw X tracks screen Y, raw Y tracks screen X), which is
handled by touch `offset_rotation = 4` in `include/lgfx_config.h` — the
ranges stay in the driver's own frame. If a different module reads
differently, re-measure the raw values at the screen corners/centre and
refit the two ranges; keep `offset_rotation` unless taps come out
rotated or mirrored.

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

- **Screens:** Splash → Connect → Home · Nav · Maps · Music · Tracking · Settings (8 screens; the Headset screen was removed in 2.4.0 — the rider's headset pairs with the phone, never with this device)
- **Boot flow (2.4.0):** a spectacular animated splash (~3.4 s — the
  sky/lime wallpaper dimmed ~40%, a pulsing lime ring behind the logo,
  "RPM LOOP" letters that pop in with a zoom + fade + rise, and an RPM
  gauge sweeping 0→92→70 with a red zone, needle and % counter) lands on
  the **Connect** screen ("Connect your phone"; tap to start the pulsing-ring
  waiting state — BLE advertising is always on, the screen is a UI state).
  A linked phone shows a brief green-check "Connected" beat, then Home;
  if the phone is already linked at splash end, Connect is skipped. If
  the BLE link drops on any other screen, the UI returns to Connect with
  a "Connection lost" note.
- **Wallpaper + vibrant theme (2.4.0):** every non-map screen sits on a
  sky-blue→cyan wallpaper image with neon-lime light trails
  (`src/ui/bg_wallpaper.c`, 320×240 RGB565, generated asset) — no more
  flat navy gradient. Neon cards (lime `#8BE000`, cyan `#2ED3E6`, pink
  `#FF4D8D`, amber `#FFB020`, radius 16) get a glossy vertical gradient,
  a 1 px darker border and a soft shadow so they pop on the bright
  background; dark text on the bright cards. Home is full-bleed: four
  146×78 tiles in a 2×2 grid (Navigate / Music / Tracking / Settings)
  with a live connection pill and status lines that show **real**
  BikeState values ("No active route", "Nothing playing", "Not
  started"), plus a bottom status strip (street + live speed while
  navigating, "Phone connected" when linked, "Ready" otherwise).
  Music / Tracking / Settings / Connect layouts were stretched to fill
  the full 320×240 the same way (glass panels keep white text readable
  over the wallpaper's bright midriff).
- **Quick shade (2.4.0):** swipe DOWN from the top edge of any screen
  (look for the small white pull pill) to slide in a quick panel: big
  **HOME** button, a brightness slider (same control as Settings), a
  Bluetooth Connected / Not connected row, and a Settings shortcut;
  swipe up or tap ✕ to close. This is also the way back Home from the
  **Nav** screen. On Nav/Maps — where a drag means map pan — the swipe
  is caught by a transparent 320×22 strip along the top edge instead of
  the gesture recogniser; panning anywhere else on the map is unaffected.
- **Demo mode (Settings, default OFF):** `MockProvider` animates
  BikeState ONLY while the switch is on (demo route, demo tracks, fake
  speed, headset toggling). Turning it off wipes every mock-owned field
  back to idle defaults. The clock ticks regardless (no RTC on the bench).
- **Nav screen (2.2.2, idle overlay 2.3.0):** TomTom/Google-style full-bleed 2D nav — the map
  fills all 320×240 (river band, building blocks, route). Over it: a
  two-segment blue instruction banner (maneuver arrow + big distance +
  street, and a "Then" preview of the next maneuver), a left rail of
  round recentre/+/- buttons, a bottom-centre speed cluster (red-ring
  speed-limit disc + current speed) and a remaining-distance/ETA pill.
  With no active route, all that chrome hides behind a centred
  "No active route — start navigation on your phone" pill (map still draws).
- **Map:** procedural `MapView` component (road grid, river, buildings,
  park/water, route polyline #1A73E8 with white casing, two-tone position
  chevron) with drag-pan/zoom/lock — rendering lives inside
  `MapView::render()`, so screens never change if the world renderer does.
- **Data seam:** screens only READ `BikeState` (`include/bike_state.h`).
  Providers write it: the BLE handler (`bike_state_apply_nav`) when the
  phone is navigating, and `MockProvider` ONLY while Demo Mode is ON —
  no UI changes needed either way. Actions go through the
  `bike_control_*()` seam (sole exception: the Settings Demo-mode switch
  binds `BikeState.demoMode` directly).
- **Controls (touch only):** tap Home tiles / buttons to navigate, every
  sub-screen has a Home button (top corner) — and every screen has the
  swipe-down quick shade (see above), which is the Home route from Nav;
  on Nav/Maps drag the map to
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

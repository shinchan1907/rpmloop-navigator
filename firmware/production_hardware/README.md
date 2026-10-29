# RPMLOOP Firmware — Production Hardware (Physical 2.8" SPI LCD + LVGL UI)

Firmware flashed onto the ESP32-S3 in the physical RPMLOOP unit: a BLE
peripheral speaking protocol v1 to the companion apps, plus a complete
embedded LVGL UI on the 2.8" panel (8 screens, encoder/joystick/button
input, mock data provider for standalone bench demos).

---

## 🖥️ Hardware (bench-verified 2026-10-29)

- **Microcontroller:** ESP32-S3 (dev: ESP32-S3 Mini; production target:
  ESP32-S3-WROOM-1-N16R8 custom PCB — same pin map)
- **Display Module:** generic red 2.8" SPI TFT, **ILI9341**, 240×320 native,
  used landscape **320×240**
- **Touch:** HR2046 resistive on the module — present but **unused by design**
  (glove-friendly physical controls only)
- **Storage:** the module's own microSD slot (back pads) — reserved, not used yet
- **Status LED:** onboard WS2812 on the original Waveshare target (GPIO 21)

### Panel quirks (proven on the bench — encoded in `include/lgfx_config.h` / `pinout_config.h`)

| Quirk | Setting |
|---|---|
| Clone panel shows inverted colours | LovyanGFX panel `invert = true` |
| 40 MHz SPI garbles pixels on jumper wires | SPI write clock **16 MHz** (raise on soldered PCB) |
| Plain landscape rotation renders mirrored text | LovyanGFX rotation **5** (`TFT_ROTATION`) |
| Backlight LED pin wired straight to 3V3 | No BL GPIO/PWM — brightness = LVGL dim overlay |

## 📌 Wiring Pinout (ESP32-S3 → 2.8" module + controls)

| Signal | GPIO | Notes |
|---|---|---|
| TFT SCLK | 6 | SPI2 |
| TFT MOSI (SDI) | 7 | |
| TFT MISO (SDO) | 8 | |
| TFT CS | 9 | ⚠️ old Waveshare map had CS=10/DC=9 — swapped, do not mix |
| TFT DC | 10 | |
| TFT RST | 11 | |
| TFT LED / VCC | 3V3 | always on |
| Encoder CLK / DT / SW | 12 / 13 / 4 | EC11, push = select |
| Joystick X / Y / SW | 5 / 1 / 2 | ADC1 + push, firmware deadzone |
| Buttons BACK / PLAY / HOME | 41 / 42 / 47 | active-low, pull-ups |
| Status LED | 48 | plain LED (custom PCB) |
| SD (reserved) SCK/MOSI/MISO/CS | 14 / 15 / 16 / 17 | module's rear slot pads |
| GPS (reserved) RX / TX | 18 / 21 | NEO-M8N UART1 |

## 🧩 Embedded UI (LVGL 8.3 + LovyanGFX)

- **Screens:** Boot → Home · Nav · Maps · Music · Headset · Tracking · Settings
- **Nav screen:** 104 px dark left panel (turn arrow, distance, street,
  instruction, speed, remaining/ETA), Google-style light map, GPS chip
  top-right, circular recentre button that appears when the map is panned.
- **Map:** procedural `MapView` component (road grid, park/water, route
  polyline #1A73E8 with white casing, position arrow) with pan/zoom/lock —
  a future SD TileStore can replace its drawing without touching screens.
- **Data seam:** screens only READ `BikeState` (`include/bike_state.h`).
  Providers write it: `MockProvider` today (animated demo data), the BLE
  handler (`bike_state_apply_nav`) when the phone is navigating, and future
  GPS/SD/A2DP drivers later — no UI changes needed. Button actions go
  through the `bike_control_*()` seam.
- **Controls:** encoder = focus/zoom, encoder push & joystick push = select,
  joystick = pan map / prev-next / adjust sliders, BACK = back, HOME = home
  screen, PLAY = play-pause (Music) / start-pause (Tracking).

## ⚡ Build & Flash

```bash
cd firmware/production_hardware
pio run                 # build (esp32s3_production)
pio run -t upload       # flash over USB-C
```

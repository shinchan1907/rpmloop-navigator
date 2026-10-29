# RPMLOOP Firmware — Simulator & Development Bridge

This firmware variant is designed for bench development, protocol verification, and desktop simulator integration. It turns the Waveshare ESP32-S3 Mini into a high-speed hardware bridge between the mobile phone (over BLE) and the PC desktop simulator (over USB CDC serial).

---

## 🌟 Key Features

1. **Hardware-Accurate WS2812 RGB LED Status Indicator:**
   - Active state feedback via the onboard mini RGB LED (GRB color order):
     - 🔴 **SOLID RED (`rgb(64, 0, 0)`):** BLE Disconnected / Standby (advertising as `RPMLOOP`).
     - 🔵 **SOLID BLUE (`rgb(0, 0, 64)`):** BLE Connected to Mobile Companion App.
   - Enforced in both `onConnect`/`onDisconnect` callbacks and main `loop()` state assertion.

2. **USB CDC Framed Serial Output:**
   - Emits COBS-encoded and CRC-16 checked packets over USB CDC serial at 115200 baud.
   - Streams `ACCEPTED_STATE` packets with live `lat`, `lon`, `speed_kmh`, `maneuver`, `distance`, and `heading` to `bridge.py` for real-time rendering in the desktop browser.

3. **High-Compatibility Navigation Protocol Parser:**
   - Full support for all protocol aliases:
     - `dist_m` & `distance_m`
     - `road` & `road_name`
     - `spd_mps` & `speed_kmh`
     - `lat`, `lon`, `head_deg`, `gps_acc_m`
   - Handles `HELLO`, `PING`, `SET_BRIGHTNESS`, `NAV_START`, `NAV_UPDATE`, `NAV_STOP`.

---

## ⚡ Flashing Instructions

```bash
cd firmware/simulator_bridge
pio run --target upload
```

To flash via `esptool.py` directly:
```bash
python -m esptool --chip esp32s3 --port COM3 --baud 921600 write_flash 0x10000 .pio/build/esp32s3/firmware.bin
```

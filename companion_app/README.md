# RPMLOOP Companion (Android) — Phase 1

Companion app for the RPMLOOP Navigator bike display (ESP32-S3). The phone
supplies GPS over BLE; the device renders offline maps from its SD tile pack.

Protocol: **RPMLOOP BLE Protocol v3.0.0** — implemented exactly as defined in
`protocol/protocol_spec.json` and `protocol/fixtures/*.json` (firmware repo).

## What Phase 1 does

- **Permissions flow** — BLUETOOTH_SCAN + BLUETOOTH_CONNECT (Android 12+),
  ACCESS_FINE_LOCATION, POST_NOTIFICATIONS (Android 13+). Nothing starts
  until all are granted; anything denied is named on screen.
- **BLE session** (`ble/BleManager.kt`):
  scan (filter: device name `RPMLOOP` or the service UUID) → connect →
  **MTU 512** (the protocol JSON does not fit the default 20-byte ATT
  payload) → service discovery → enable notifications on the handshake and
  device_cmd characteristics → write `hello` → device replies `hello_ack`
  as a notification. GATT operations are serialised through an internal
  queue. If the link drops while running, scanning resumes automatically.
- **GPS streaming** (`location/LocationProvider.kt`): Fused Location at
  ~1 Hz. Once the handshake completes, each fix is written to the position
  characteristic as a `pos` JSON message in exactly the fixture format.
  Fields a fix doesn't provide (speed/bearing/accuracy) are omitted, not
  invented. Position is not sent before the handshake — hello/hello_ack
  opens the session.
- **Foreground service** (`service/CompanionService.kt`, location type):
  keeps scan/connection/GPS alive with the screen off; persistent
  notification shows the live connection state and has a Stop action.
- **One main screen**: status (Disconnected / Scanning / Connecting /
  Connected + device name), Start/Stop, handshake result line, and live
  latitude / longitude / speed / heading. Empty states are honest
  ("Waiting for GPS fix…", "—") — there is no demo data anywhere.

## Protocol reference (v3.0.0)

| Characteristic | UUID | Phase 1 use |
|---|---|---|
| Service | `4fafc201-1fb5-459e-8fcc-c5c9c331914b` | discovery |
| handshake | `beb5483e-…-b7f5-ea07361b26a8` | write `hello`, notify `hello_ack` |
| position | `beb5483e-…-b7f5-ea07361b26a9` | write `pos` at ~1 Hz |
| nav_state | `beb5483e-…-b7f5-ea07361b26aa` | Phase 2 |
| polyline | `beb5483e-…-b7f5-ea07361b26ab` | Phase 2 |
| media_state | `beb5483e-…-b7f5-ea07361b26ac` | Phase 2 |
| media_art | `beb5483e-…-b7f5-ea07361b26ad` | Phase 2 |
| tracking | `beb5483e-…-b7f5-ea07361b26ae` | Phase 2 |
| device_cmd | `beb5483e-…-b7f5-ea07361b26af` | notify — received & logged in Phase 1 |

Hello (app → device):
```json
{"type":"hello","version":"3.0.0","app_name":"RPMLOOP Companion","platform":"android","capabilities":["gps","nav","media","tracking"]}
```
Position (app → device, ~1 Hz):
```json
{"type":"pos","lat":31.326015,"lon":75.576180,"speed_kmh":42.5,"heading":185.2,"accuracy_m":4.2,"ts":1730500000}
```

## Open / build in Android Studio

1. Open this `companion_app` folder in Android Studio (AGP 8.5.2, Kotlin
   2.0.20, Gradle 8.7, minSdk 26, targetSdk 34).
2. Let it sync (it downloads the Gradle 8.7 distribution and dependencies —
   `play-services-location`, Material 3, coroutines).
3. Run on a physical Android phone — BLE and GPS need real hardware; the
   emulator cannot test either.

> **Wrapper note:** this project was authored on a machine without a JDK or
> Gradle, so `gradle/wrapper/gradle-wrapper.properties` is present but
> `gradle-wrapper.jar` could not be generated and **no local build was run**.
> If Android Studio reports the wrapper jar as missing, run
> `gradle wrapper --gradle-version 8.7` once in this folder with any local
> Gradle 8.x installation; the generated jar lands in `gradle/wrapper/`.

## Permissions the app needs

| Permission | Why |
|---|---|
| BLUETOOTH_SCAN | Find the RPMLOOP advertiser |
| BLUETOOTH_CONNECT | Connect / GATT session |
| ACCESS_FINE_LOCATION | GPS fixes (and BLE scans on Android < 12) |
| POST_NOTIFICATIONS | Foreground-service notification (Android 13+) |
| FOREGROUND_SERVICE_LOCATION | Location-type foreground service (Android 14) |

No background-location permission is requested — the foreground service is
what keeps GPS alive with the screen off.

## Phase 2 TODO (stubbed, not faked)

- `nav/MapsNotificationParser.kt` — parse Google Maps navigation
  notifications → `nav` messages.
- `nav/RoutePolylineProvider.kt` — fetch route geometry (Directions/OSRM)
  → chunked `poly` messages.
- `media/MediaSessionController.kt` — media session readout → `media`
  messages + album art; act on `media_cmd` device commands.
- `tracking/TrackingManager.kt` — trip start/stop in the app, live `track`
  stats, trip history. (Per product decision, tracking setup lives in the
  app; the device is display-only.)

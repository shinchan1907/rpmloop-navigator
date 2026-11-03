# RPMLOOP Companion (Android) — Full version (2.0)

Companion app for the RPMLOOP Navigator bike display (ESP32-S3). The phone
supplies GPS over BLE, computes turn-by-turn navigation, downloads offline
map tiles and pushes them to the display's SD card over Bluetooth.

Protocols: **RPMLOOP BLE Protocol v3.0.0** (session / position / nav /
polyline) implemented exactly as defined in `protocol/protocol_spec.json`
and `protocol/fixtures/*.json` in the firmware repo, plus the **v3.1 file
transfer extension** (tile push) defined below.

> **Verification status:** this project was authored on a machine with no
> JDK, Gradle or Android SDK, so **it has not been compiler-verified** and
> no local build has been run. It is written to be opened in Android Studio
> (AGP 8.5.2, Kotlin 2.0.20, Gradle 8.7, minSdk 26, targetSdk 34) and built
> there — expect to fix any small compile issues on first sync. Runtime
> behaviour (BLE, GPS, tile transfer) additionally needs a physical phone
> and the device.

## What it does

- **Connection + GPS (unchanged base)** — permissions flow; BLE scan
  (name `RPMLOOP` or service UUID) → connect → MTU 512 → discovery →
  `hello`/`hello_ack` handshake → `pos` writes at ~1 Hz, all inside a
  location-type foreground service that keeps running with the screen off.
- **Navigation** — destination search (Nominatim), intermediate stops
  ("break points") + one destination, OSRM routing, an in-service nav
  engine that emits spec-format `nav` updates and streams the route as
  chunked `poly` messages. Off-route detection with automatic rerouting,
  arrival detection. Works fully with the phone screen off / in a pocket.
- **Offline tiles** — downloads Google Maps roadmap tiles (zooms 13–16)
  for the planned route corridor or a 5 km radius around you, then
  transfers them to the device's SD card over BLE (protocol v3.1 below),
  with resume and a sent-manifest so nothing is sent twice.
- **Not in this version:** media control and trip tracking — the
  `media/MediaSessionController.kt` and `tracking/TrackingManager.kt`
  classes remain clearly marked stubs and produce no data.

## Navigation flow

1. Press **Start** (top) so the foreground service runs — navigation lives
   there, not in the Activity.
2. **Search** a place (Nominatim, `limit=5`). Tap a result → *Add as stop*
   or *Set as destination*. Your current GPS fix is always the start.
   Stops can be added in any number and removed individually; there is
   exactly one destination.
3. **Plan route** — the app calls OSRM
   (`route/v1/driving`, `overview=full&geometries=geojson&steps=true`)
   through start → stops → destination, shows distance/duration, and
   sends the polyline to the device as sequenced `poly` chunks
   (route geometry decimated to ≥ 4 m spacing for the device copy; chunk
   size respects the negotiated MTU).
4. **Start navigation** — the engine (in the service) projects each GPS
   fix onto the route and, whenever the payload changes, writes a `nav`
   message: next maneuver (OSRM maneuvers mapped onto the spec's
   `maneuver_types`), distance to it, current/next street, remaining km
   and ETA. A 10 s keepalive re-send prevents a stale display.
   - **Off-route:** > 60 m from the polyline for 3 consecutive fixes →
     automatic reroute from the current position through the stops not
     yet passed (20 s cooldown between reroutes); new polyline is sent.
   - **Arrival:** within 25 m of the destination → one `destination` nav
     update, then a final `{"type":"nav","active":false}`, and the engine
     stops cleanly.
5. **Stop** ends navigation with the same inactive message. Editing the
   stop list mid-navigation also stops it (the route no longer matches).

The foreground notification shows the next instruction while navigating.

## Offline tiles flow

### 1. Set a Google Maps Tile API key (one-time setup)

Downloads are **disabled until a key is saved** — there is no key in the
source code, and none is ever logged.

1. In Google Cloud Console, create/select a project and enable the
   **Maps Tile API** (a billing account is required by Google for that
   API; tile usage is billed by Google, not by this app).
2. Create an API key under *APIs & Services → Credentials* (restrict it to
   the Maps Tile API, and ideally to this app).
3. In the app: *Offline map tiles → Google Maps Tile API key* → paste the
   key → **Save**. It is stored only in app-private SharedPreferences.

### 2. Download tiles to the phone

- **Route tiles** — every tile the planned route passes through, plus a
  1-tile margin, zooms 13–16.
- **Nearby 5 km** — every tile covering a 5 km radius around the current
  GPS fix, zooms 13–16.

Both first show an estimate (tile count + approximate size, from
per-zoom averages of ~6 KB at z13–14 and ~10 KB at z15–16) and ask for
confirmation. Downloads use a Tile API session
(`POST /v1/createSession` with
`{"mapType":"roadmap","language":"en-US","region":"IN"}`, then
`GET /v1/2dtiles/{z}/{x}/{y}?session=…&key=…`), run 4-at-a-time with
progress and cancel, skip tiles already on the phone, and retry a failed
tile up to 3 times (a 401/403 refreshes the session). Tiles are stored as
`files/tiles/{z}/{x}/{y}.jpg`, mirroring the SD card layout.

### 3. Send tiles to the device (protocol v3.1)

Requires a connected, handshaken device whose firmware exposes the two
v3.1 characteristics. Every downloaded tile not in the sent-manifest is
sent sequentially, with overall progress (files, current file %, approx.
KB/s). A file that errors is retried once, then skipped and reported. If
the link drops mid-file, the run stops and the file resumes next time via
the device-reported offset.

## Protocol reference

### v3.0.0 (from `protocol/protocol_spec.json`)

| Characteristic | UUID (…b7f5-ea07361b26xx) | Use in this version |
|---|---|---|
| Service | `4fafc201-1fb5-459e-8fcc-c5c9c331914b` | discovery |
| handshake | `…a8` | write `hello`, notify `hello_ack` |
| position | `…a9` | write `pos` at ~1 Hz |
| nav_state | `…aa` | write `nav` updates |
| polyline | `…ab` | write chunked `poly` messages |
| media_state | `…ac` | not used (stub feature) |
| media_art | `…ad` | not used (stub feature) |
| tracking | `…ae` | not used (stub feature) |
| device_cmd | `…af` | notify — received & logged |

Position (app → device, ~1 Hz):
```json
{"type":"pos","lat":31.326015,"lon":75.576180,"speed_kmh":42.5,"heading":185.2,"accuracy_m":4.2,"ts":1730500000}
```
Nav (app → device, on change):
```json
{"type":"nav","active":true,"maneuver":"turn_right","distance_m":150,"street":"GT Road","next_street":"Model Town Rd","eta_min":12,"remaining_km":5.3}
```
Polyline chunk (app → device, in order, per route):
```json
{"type":"poly","route_id":"r48213","seq":0,"total":3,"points":[[31.326015,75.57618],[31.327001,75.577002]]}
```

### v3.1 file transfer (this app's extension, same service)

| Characteristic | UUID | Properties |
|---|---|---|
| FILE_DATA | `beb5483e-36e1-4688-b7f5-ea07361b26b0` | WRITE_NO_RSP |
| FILE_CTRL | `beb5483e-36e1-4688-b7f5-ea07361b26b1` | WRITE + NOTIFY |

Control JSON is written to FILE_CTRL; device replies arrive as FILE_CTRL
notifications:

1. App → `{"type":"file_begin","path":"/tiles/16/29876/16981.jpg","size":18342}`
2. Device → `{"type":"file_ready","offset":N}` — bytes already on the SD
   card; the app streams from offset N (resume).
3. App streams FILE_DATA chunks: 4-byte header
   `[seq u16 LE][payload_len u16 LE]` + payload, payload ≤ MTU − 3 − 4
   bytes. `seq` counts chunks from 0 starting at the resume offset.
   Sliding window: never more than 16 unacked chunks.
4. Device → `{"type":"file_ack","seq":K}` (last contiguous chunk
   received) whenever it has consumed data.
5. App → `{"type":"file_end","size":18342}` after the final ack.
6. Device → `{"type":"file_done"}` (app marks the tile sent) or
   `{"type":"file_error","reason":"…"}` (app retries the file once, then
   skips it).

Timeouts on the app side: 10 s waiting for `file_ready`, 15 s stall while
streaming, 15 s waiting for `file_done`.

## Open / build in Android Studio

1. Open this `companion_app` folder in Android Studio and let it sync
   (downloads the Gradle 8.7 distribution and dependencies —
   `play-services-location`, Material 3, coroutines).
2. Run on a physical Android phone — BLE and GPS need real hardware.
3. **Wrapper note:** `gradle/wrapper/gradle-wrapper.properties` is present
   but `gradle-wrapper.jar` could not be generated on the authoring
   machine; if Android Studio reports it missing, run
   `gradle wrapper --gradle-version 8.7` once in this folder with any
   local Gradle 8.x installation.

## Permissions the app needs

| Permission | Why |
|---|---|
| BLUETOOTH_SCAN | Find the RPMLOOP advertiser |
| BLUETOOTH_CONNECT | Connect / GATT session |
| ACCESS_FINE_LOCATION | GPS fixes (and BLE scans on Android < 12) |
| POST_NOTIFICATIONS | Foreground-service notification (Android 13+) |
| FOREGROUND_SERVICE_LOCATION | Location-type foreground service (Android 14) |
| INTERNET | Nominatim search, OSRM routing, Tile API downloads |

No background-location permission is requested — the foreground service is
what keeps GPS alive with the screen off.

# RPMLopp Nav Protocol Specification (v1.0.0)

## Overview
RPMLopp Nav coordinates an iOS companion app, an ESP32-S3 BLE peripheral, and a desktop bridge/virtual display over USB Serial.

```
+------------------+         Bluetooth LE (GATT)         +--------------------+
| iOS Companion App| ==================================> |  ESP32-S3 Firmware |
| (RPMLopp Nav App)| <---------------------------------- |   (RPMLopp Nav)    |
+------------------+   Status / ACK (Notification)       +---------+----------+
                                                                   |
                                                      USB Serial   | Framing:
                                                      [RPMLOPP_*]  | 115200 baud
                                                                   v
                                                         +--------------------+
                                                         |   Desktop Bridge   |
                                                         |  (Node.js/Serial)  |
                                                         +---------+----------+
                                                                   | WebSocket
                                                                   v
                                                         +--------------------+
                                                         | Desktop Dashboard  |
                                                         |   + Virtual LCD    |
                                                         +--------------------+
```

## BLE GATT Service & Characteristics

* **Advertised Name**: `RPMLopp Nav`
* **Custom Service UUID**: `4E415601-5250-4D4C-6F70-704E61760001`

### Characteristics
1. **Command & Navigation Data (`CMD_NAV`)**:
   * UUID: `4E415602-5250-4D4C-6F70-704E61760002`
   * Properties: `Write`, `Write Without Response`
   * Direction: Phone -> ESP32
   * Payloads: `HELLO`, `PING`, `NAV_START`, `NAV_UPDATE`, `NAV_PAUSE`, `NAV_RESUME`, `NAV_STOP`.

2. **Status & Acknowledgements (`STATUS_ACK`)**:
   * UUID: `4E415603-5250-4D4C-6F70-704E61760003`
   * Properties: `Notify`, `Read`
   * Direction: ESP32 -> Phone
   * Payloads: `HELLO_ACK`, `PONG`, `ACK`, `ERROR`.

3. **Device Info & Capabilities (`DEVICE_INFO`)**:
   * UUID: `4E415604-5250-4D4C-6F70-704E61760004`
   * Properties: `Read`
   * Returns: Static JSON with board model, firmware version, protocol version, screen resolution.

---

## Packet Envelope & Fragmentation

For packets within negotiated MTU (typically 185-512 bytes on iOS), complete JSON strings are sent directly.
When fragmented, packets are framed with a 4-byte chunk prefix:
`[chunk_index: 1B][total_chunks: 1B][seq: 2B][data...]`

### Navigation Update Schema (`NAV_UPDATE`)
```json
{
  "v": 1,
  "type": "NAV_UPDATE",
  "seq": 104,
  "sid": "sess_894a",
  "rid": "route_test_01",
  "t_ms": 1729864200000,
  "payload": {
    "pos_valid": true,
    "lat": 37.7749,
    "lon": -122.4194,
    "spd_mps": 14.2,
    "head_deg": 90.0,
    "maneuver": "TURN_RIGHT",
    "dist_m": 350,
    "road": "Broadway Ave",
    "instr": "Turn right onto Broadway Ave",
    "rem_dist_m": 4800,
    "rem_time_s": 520,
    "progress_pct": 28,
    "off_route": false,
    "gps_acc_m": 3.8,
    "is_sim": true
  },
  "crc": 18274
}
```

### Acknowledgement Schema (`ACK`)
```json
{
  "v": 1,
  "type": "ACK",
  "seq": 104,
  "sid": "sess_894a",
  "status": "ACCEPTED",
  "crc": 29811
}
```

---

## USB Serial Framing

To guarantee that ESP-IDF / FreeRTOS boot logs or debug output never corrupt protocol data, all serial packets emitted by the ESP32 follow distinct prefixes:

* `[RPMLOPP_EVT] <json>`: Emitted whenever the ESP32 accepts a navigation update or changes internal state.
* `[RPMLOPP_ACK] <json>`: Emitted whenever an application acknowledgement is sent to the phone.
* `[RPMLOPP_LOG] <json>`: System diagnostics, memory stats, BLE peer connections.
* `[RPMLOPP_ERR] <json>`: Validation errors, checksum failures, stale timeout alerts.

Example USB serial stream:
```text
[RPMLOPP_LOG] {"level":"INFO","msg":"BLE connected: 58:D3:49:1B:2C:FA"}
[RPMLOPP_EVT] {"event":"ACCEPTED_STATE","seq":104,"state":{"maneuver":"TURN_RIGHT","dist_m":350,"road":"Broadway Ave","spd_mps":14.2},"ts":1729864200000}
[RPMLOPP_ACK] {"seq":104,"status":"ACCEPTED"}
```

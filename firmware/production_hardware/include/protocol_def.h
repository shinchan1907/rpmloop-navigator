#ifndef PROTOCOL_DEF_H
#define PROTOCOL_DEF_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// ==============================================================================
// RPMLOOP BLE PROTOCOL v1 — shared by phone apps, simulator bridge and this
// production firmware. UUIDs / message types / payload field names are the
// wire contract: do not rename. (Verified identical to
// firmware/simulator_bridge/include/protocol_def.h on 2026-10-29.)
// ==============================================================================
#define RPMLOPP_BLE_DEVICE_NAME        "RPMLOOP"
#define RPMLOPP_SERVICE_UUID           "4E415601-5250-4D4C-6F70-704E61760001"
#define RPMLOPP_CHAR_CMD_NAV_UUID      "4E415602-5250-4D4C-6F70-704E61760002"
#define RPMLOPP_CHAR_STATUS_ACK_UUID   "4E415603-5250-4D4C-6F70-704E61760003"
#define RPMLOPP_CHAR_DEVICE_INFO_UUID  "4E415604-5250-4D4C-6F70-704E61760004"

#define RPMLOPP_PROTOCOL_VERSION       1
#define RPMLOPP_FIRMWARE_VERSION       "2.4.1"
#define RPMLOPP_HARDWARE_MODEL         "ESP32-S3 (Mini) + 2.8in ST7789V SPI"

#define RPMLOPP_STALE_TIMEOUT_MS       5000
#define RPMLOPP_VIRTUAL_LCD_W          320
#define RPMLOPP_VIRTUAL_LCD_H          240

enum NavStatus {
    NAV_STATUS_DISCONNECTED = 0,
    NAV_STATUS_CONNECTED_IDLE = 1,
    NAV_STATUS_NAVIGATING = 2,
    NAV_STATUS_PAUSED = 3,
    NAV_STATUS_STALE = 4
};

enum ManeuverType {
    MANEUVER_NONE = 0,
    MANEUVER_STRAIGHT = 1,
    MANEUVER_SLIGHT_RIGHT = 2,
    MANEUVER_TURN_RIGHT = 3,
    MANEUVER_SHARP_RIGHT = 4,
    MANEUVER_UTURN = 5,
    MANEUVER_SHARP_LEFT = 6,
    MANEUVER_TURN_LEFT = 7,
    MANEUVER_SLIGHT_LEFT = 8,
    MANEUVER_ROUNDABOUT = 9,
    MANEUVER_DESTINATION = 10
};

// Parsed phone -> device navigation state (one per NAV_* packet).
struct NavigationState {
    bool valid;
    bool is_sim;
    NavStatus status;
    uint32_t seq;
    char session_id[32];
    char route_id[32];
    double lat;
    double lon;
    float speed_mps;
    float heading_deg;
    ManeuverType maneuver;
    uint32_t dist_to_turn_m;
    char road[64];
    char instruction[128];
    uint32_t rem_dist_m;
    uint32_t rem_time_s;
    uint8_t progress_pct;
    bool off_route;
    float gps_acc_m;
    uint32_t last_update_ms;
};

// Maneuver string <-> enum helpers (implemented in src/protocol_util.cpp).
// NOTE: production main.cpp called parseManeuver() before 2.1.0 but the
// function only existed in the simulator bridge — that was a build break.
ManeuverType parseManeuver(const char* m);
const char* maneuverToString(ManeuverType m);

#endif // PROTOCOL_DEF_H

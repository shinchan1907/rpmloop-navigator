#ifndef BIKE_STATE_H
#define BIKE_STATE_H

#include <stdint.h>
#include <stdbool.h>
#include "protocol_def.h"

// ==============================================================================
// BikeState — the SINGLE source of truth for everything on screen.
//
// Data flow (the "provider seam"):
//   PROVIDERS write BikeState          UI screens only READ BikeState
//   ┌─────────────────────────────┐    ┌──────────────────────────────┐
//   │ MockProvider (today)        │    │ Boot / Home / Nav / Maps /   │
//   │ BLE nav feed (bike_state_   │ -> │ Music / Headset / Tracking / │
//   │   apply_nav, from main.cpp) │    │ Settings                     │
//   │ GPS / SD / A2DP (future)    │    └──────────────────────────────┘
//   └─────────────────────────────┘
// A future real driver replaces/augments a provider WITHOUT touching the UI:
// it just writes the same fields (e.g. the BLE handler already translates
// protocol NavigationState -> BikeState in bike_state_apply_nav()).
// Screens must never keep their own copies of these values.
// ==============================================================================

// Turn direction codes for BikeState.turnDir (UI arrow selection).
enum TurnDir {
    TURN_DIR_LEFT = 0,
    TURN_DIR_RIGHT = 1,
    TURN_DIR_STRAIGHT = 2,
    TURN_DIR_UTURN = 3,
    TURN_DIR_ROUNDABOUT = 4,
    TURN_DIR_DESTINATION = 5
};

struct BikeState {
    // ---- navigation ----
    bool   navigating;        // a route is active (mock demo or live BLE)
    bool   navStale;          // live feed silent > RPMLOPP_STALE_TIMEOUT_MS
    bool   offRoute;
    float  distToTurn_m;      // distance to next maneuver
    int    turnDir;           // TurnDir enum (current maneuver)
    int    nextTurnDir;       // TurnDir enum (the maneuver AFTER this one,
                              // shown in the Nav banner's "Then" segment)
    uint8_t speedLimitKmh;    // posted speed limit (speed cluster ring)
    char   streetName[40];    // road after the turn
    char   instruction[96];   // full instruction line
    float  remainKm;          // remaining route distance
    int    etaMin;            // remaining time, minutes
    uint8_t progressPct;      // route progress 0..100
    float  speedKmh;          // current speed
    float  gpsAccuracy_m;     // GPS accuracy estimate
    int    zoomLevel;         // map zoom 14..18
    bool   mapLocked;         // true = map follows position (recentre hidden)

    // ---- music / headset ----
    bool   headsetConnected;
    bool   playing;
    char   trackTitle[48];
    char   trackArtist[48];
    int    trackPosSec;
    int    trackDurSec;

    // ---- ride tracking ----
    float  tripKm;
    int    rideTimeSec;
    float  maxKmh;
    float  avgKmh;
    bool   trackingActive;

    // ---- system ----
    int    clockHour;
    int    clockMin;
    bool   usbPower;
    bool   bleConnected;      // phone linked over BLE
    int    brightnessPct;     // 40..100 (software dim overlay; no BL pin)
    int    volumePct;         // 0..100 (stored only until A2DP lands)
};

extern BikeState g_bike_state;

void bike_state_init(void);

// ---- BLE protocol seam (called from main.cpp BLE handlers) ----
// Translate one parsed protocol packet into BikeState. While a live session
// is fresh (< stale timeout), MockProvider leaves the nav fields alone.
void bike_state_apply_nav(const NavigationState& s);
void bike_state_on_ble_connect(void);
void bike_state_on_ble_disconnect(void);
bool bike_state_nav_live(uint32_t nowMs);   // fresh live nav feed present?
void bike_state_check_stale(uint32_t nowMs);

// ---- UI control seam (button actions; MockProvider implements them today,
// real drivers tomorrow — screens call these, never write state directly) --
void bike_control_toggle_play(void);
void bike_control_next_track(void);
void bike_control_prev_track(void);
void bike_control_toggle_tracking(void);
void bike_control_reset_tracking(void);
void bike_control_toggle_headset(void);
void bike_control_set_volume(int pct);
void bike_control_set_brightness(int pct);

// Maneuver enum (protocol) -> TurnDir (UI).
int turn_dir_from_maneuver(ManeuverType m);

#endif // BIKE_STATE_H

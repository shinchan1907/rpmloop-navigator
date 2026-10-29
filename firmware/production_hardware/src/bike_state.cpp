#include <Arduino.h>
#include <string.h>
#include "bike_state.h"

BikeState g_bike_state;

// Live-feed bookkeeping (not UI state, so it lives outside BikeState).
static uint32_t s_lastNavMs = 0;
static bool     s_liveSession = false;   // NAV_START..NAV_STOP window

void bike_state_init(void) {
    memset(&g_bike_state, 0, sizeof(g_bike_state));
    BikeState& b = g_bike_state;
    // Idle defaults (2.3.0): the device boots HONEST — no route, no track,
    // no speed until the phone feeds real data over BLE or the user turns
    // Demo Mode ON in Settings (MockProvider then seeds its demo scene).
    b.navigating = false;
    b.turnDir = TURN_DIR_STRAIGHT;
    b.nextTurnDir = TURN_DIR_STRAIGHT;
    b.speedLimitKmh = 0;
    b.zoomLevel = 16;
    b.mapLocked = true;
    b.demoMode = false;
    b.clockHour = 10;
    b.clockMin = 24;
    b.usbPower = true;
    b.brightnessPct = 100;
    b.volumePct = 70;
    s_lastNavMs = 0;
    s_liveSession = false;
}

int turn_dir_from_maneuver(ManeuverType m) {
    switch (m) {
        case MANEUVER_TURN_LEFT:
        case MANEUVER_SLIGHT_LEFT:
        case MANEUVER_SHARP_LEFT:   return TURN_DIR_LEFT;
        case MANEUVER_TURN_RIGHT:
        case MANEUVER_SLIGHT_RIGHT:
        case MANEUVER_SHARP_RIGHT:  return TURN_DIR_RIGHT;
        case MANEUVER_UTURN:        return TURN_DIR_UTURN;
        case MANEUVER_ROUNDABOUT:   return TURN_DIR_ROUNDABOUT;
        case MANEUVER_DESTINATION:  return TURN_DIR_DESTINATION;
        case MANEUVER_STRAIGHT:
        case MANEUVER_NONE:
        default:                    return TURN_DIR_STRAIGHT;
    }
}

bool bike_state_nav_live(uint32_t nowMs) {
    return s_liveSession && (nowMs - s_lastNavMs) <= RPMLOPP_STALE_TIMEOUT_MS;
}

void bike_state_apply_nav(const NavigationState& s) {
    BikeState& b = g_bike_state;
    s_lastNavMs = s.last_update_ms;

    if (s.status == NAV_STATUS_CONNECTED_IDLE) {
        // NAV_STOP: live session over — nav fields fall back to idle
        // (MockProvider re-seeds its demo route only if Demo Mode is ON).
        s_liveSession = false;
        b.navigating = false;
        b.navStale = false;
        b.speedKmh = 0.0f;   // no provider owns speed now — don't show stale
        return;
    }
    s_liveSession = true;
    b.navStale = false;
    b.navigating = (s.status == NAV_STATUS_NAVIGATING);
    b.offRoute = s.off_route;
    b.distToTurn_m = (float)s.dist_to_turn_m;
    b.turnDir = turn_dir_from_maneuver(s.maneuver);
    strncpy(b.streetName, s.road, sizeof(b.streetName) - 1);
    b.streetName[sizeof(b.streetName) - 1] = '\0';
    strncpy(b.instruction, s.instruction, sizeof(b.instruction) - 1);
    b.instruction[sizeof(b.instruction) - 1] = '\0';
    b.remainKm = s.rem_dist_m / 1000.0f;
    b.etaMin = (int)((s.rem_time_s + 59) / 60);
    b.progressPct = s.progress_pct;
    b.speedKmh = s.speed_mps * 3.6f;
    b.gpsAccuracy_m = s.gps_acc_m;
}

void bike_state_on_ble_connect(void) {
    g_bike_state.bleConnected = true;
}

void bike_state_on_ble_disconnect(void) {
    g_bike_state.bleConnected = false;
    // Live session dies with the link; nav fields idle again (or the mock
    // demo route, if Demo Mode is ON). The UI returns to the Connect screen.
    s_liveSession = false;
    g_bike_state.navStale = false;
}

void bike_state_check_stale(uint32_t nowMs) {
    if (s_liveSession && (nowMs - s_lastNavMs) > RPMLOPP_STALE_TIMEOUT_MS) {
        g_bike_state.navStale = true;
    }
}

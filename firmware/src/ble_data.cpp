// BLE data model implementation
// Maneuver parsing and display names

#include "ble_data.h"
#include <lvgl.h>
#include <cstring>

// Global device state
DeviceState deviceState = {};

struct ManeuverMapping {
    const char* name;
    Maneuver    value;
    const char* display;
    const char* symbol;
};

static const ManeuverMapping MANEUVERS[] = {
    {"straight",          Maneuver::STRAIGHT,          "Continue",         LV_SYMBOL_UP},
    {"turn_left",         Maneuver::TURN_LEFT,         "Turn left",        LV_SYMBOL_LEFT},
    {"turn_right",        Maneuver::TURN_RIGHT,        "Turn right",       LV_SYMBOL_RIGHT},
    {"slight_left",       Maneuver::SLIGHT_LEFT,       "Slight left",      LV_SYMBOL_LEFT},
    {"slight_right",      Maneuver::SLIGHT_RIGHT,      "Slight right",     LV_SYMBOL_RIGHT},
    {"sharp_left",        Maneuver::SHARP_LEFT,        "Sharp left",       LV_SYMBOL_LEFT},
    {"sharp_right",       Maneuver::SHARP_RIGHT,       "Sharp right",      LV_SYMBOL_RIGHT},
    {"u_turn_left",       Maneuver::U_TURN_LEFT,       "U-turn left",      LV_SYMBOL_LOOP},
    {"u_turn_right",      Maneuver::U_TURN_RIGHT,      "U-turn right",     LV_SYMBOL_LOOP},
    {"roundabout_left",   Maneuver::ROUNDABOUT_LEFT,   "Roundabout left",  LV_SYMBOL_REFRESH},
    {"roundabout_right",  Maneuver::ROUNDABOUT_RIGHT,  "Roundabout right", LV_SYMBOL_REFRESH},
    {"merge",             Maneuver::MERGE,             "Merge",            LV_SYMBOL_UP},
    {"fork_left",         Maneuver::FORK_LEFT,         "Fork left",        LV_SYMBOL_LEFT},
    {"fork_right",        Maneuver::FORK_RIGHT,        "Fork right",       LV_SYMBOL_RIGHT},
    {"ramp_left",         Maneuver::RAMP_LEFT,         "Ramp left",        LV_SYMBOL_LEFT},
    {"ramp_right",        Maneuver::RAMP_RIGHT,        "Ramp right",       LV_SYMBOL_RIGHT},
    {"destination",       Maneuver::DESTINATION,       "Destination",      LV_SYMBOL_GPS},
    {"destination_left",  Maneuver::DESTINATION_LEFT,  "Destination left", LV_SYMBOL_GPS},
    {"destination_right", Maneuver::DESTINATION_RIGHT, "Destination right",LV_SYMBOL_GPS},
};

static const int NUM_MANEUVERS = sizeof(MANEUVERS) / sizeof(MANEUVERS[0]);

Maneuver parseManeuver(const char* str) {
    if (!str) return Maneuver::NONE;
    for (int i = 0; i < NUM_MANEUVERS; i++) {
        if (strcmp(str, MANEUVERS[i].name) == 0) {
            return MANEUVERS[i].value;
        }
    }
    return Maneuver::NONE;
}

const char* maneuverName(Maneuver m) {
    for (int i = 0; i < NUM_MANEUVERS; i++) {
        if (MANEUVERS[i].value == m) return MANEUVERS[i].display;
    }
    return "Unknown";
}

const char* maneuverSymbol(Maneuver m) {
    for (int i = 0; i < NUM_MANEUVERS; i++) {
        if (MANEUVERS[i].value == m) return MANEUVERS[i].symbol;
    }
    return LV_SYMBOL_DUMMY;
}

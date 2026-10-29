#include "protocol_def.h"

// Maneuver helpers shared by the BLE packet handler. This is the same mapping
// the simulator bridge firmware uses (its copy lives in its main.cpp); it was
// missing from the production firmware, which called parseManeuver() without
// any definition — a hard build break fixed in 2.1.0.

ManeuverType parseManeuver(const char* m) {
    if (!m) return MANEUVER_NONE;
    if (strcmp(m, "STRAIGHT") == 0)     return MANEUVER_STRAIGHT;
    if (strcmp(m, "SLIGHT_RIGHT") == 0) return MANEUVER_SLIGHT_RIGHT;
    if (strcmp(m, "TURN_RIGHT") == 0)   return MANEUVER_TURN_RIGHT;
    if (strcmp(m, "SHARP_RIGHT") == 0)  return MANEUVER_SHARP_RIGHT;
    if (strcmp(m, "UTURN") == 0)        return MANEUVER_UTURN;
    if (strcmp(m, "SHARP_LEFT") == 0)   return MANEUVER_SHARP_LEFT;
    if (strcmp(m, "TURN_LEFT") == 0)    return MANEUVER_TURN_LEFT;
    if (strcmp(m, "SLIGHT_LEFT") == 0)  return MANEUVER_SLIGHT_LEFT;
    if (strcmp(m, "ROUNDABOUT") == 0)   return MANEUVER_ROUNDABOUT;
    if (strcmp(m, "DESTINATION") == 0)  return MANEUVER_DESTINATION;
    return MANEUVER_NONE;
}

const char* maneuverToString(ManeuverType m) {
    switch (m) {
        case MANEUVER_STRAIGHT:     return "STRAIGHT";
        case MANEUVER_SLIGHT_RIGHT: return "SLIGHT_RIGHT";
        case MANEUVER_TURN_RIGHT:   return "TURN_RIGHT";
        case MANEUVER_SHARP_RIGHT:  return "SHARP_RIGHT";
        case MANEUVER_UTURN:        return "UTURN";
        case MANEUVER_SHARP_LEFT:   return "SHARP_LEFT";
        case MANEUVER_TURN_LEFT:    return "TURN_LEFT";
        case MANEUVER_SLIGHT_LEFT:  return "SLIGHT_LEFT";
        case MANEUVER_ROUNDABOUT:   return "ROUNDABOUT";
        case MANEUVER_DESTINATION:  return "DESTINATION";
        default:                    return "NONE";
    }
}

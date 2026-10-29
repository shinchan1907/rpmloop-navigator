#ifndef VIRTUAL_DISPLAY_H
#define VIRTUAL_DISPLAY_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "protocol_def.h"
#include "usb_framed_serial.h"

class VirtualDisplay {
private:
    NavigationState _state;
    bool _isStale;
    uint16_t _width;
    uint16_t _height;

public:
    VirtualDisplay(uint16_t w = RPMLOPP_VIRTUAL_LCD_W, uint16_t h = RPMLOPP_VIRTUAL_LCD_H)
        : _isStale(false), _width(w), _height(h) {
        memset(&_state, 0, sizeof(NavigationState));
        _state.status = NAV_STATUS_DISCONNECTED;
    }

    void setConnectionStatus(NavStatus status) {
        _state.status = status;
        if (status == NAV_STATUS_DISCONNECTED) {
            _state.valid = false;
        }
        publishState();
    }

    void updateNavState(const NavigationState& newState) {
        _state = newState;
        _isStale = false;
        publishState();
    }

    void checkStaleTimeout(uint32_t now) {
        if (_state.status == NAV_STATUS_NAVIGATING) {
            if (!_isStale && (now - _state.last_update_ms > RPMLOPP_STALE_TIMEOUT_MS)) {
                _isStale = true;
                _state.status = NAV_STATUS_STALE;
                UsbFramedSerial::emitErr("ERR_STALE_TIMEOUT", "Navigation state stale (>5000ms)", _state.seq);
                publishState();
            }
        }
    }

    const char* maneuverToString(ManeuverType m) {
        switch (m) {
            case MANEUVER_STRAIGHT: return "STRAIGHT";
            case MANEUVER_SLIGHT_RIGHT: return "SLIGHT_RIGHT";
            case MANEUVER_TURN_RIGHT: return "TURN_RIGHT";
            case MANEUVER_SHARP_RIGHT: return "SHARP_RIGHT";
            case MANEUVER_UTURN: return "UTURN";
            case MANEUVER_SHARP_LEFT: return "SHARP_LEFT";
            case MANEUVER_TURN_LEFT: return "TURN_LEFT";
            case MANEUVER_SLIGHT_LEFT: return "SLIGHT_LEFT";
            case MANEUVER_ROUNDABOUT: return "ROUNDABOUT";
            case MANEUVER_DESTINATION: return "DESTINATION";
            default: return "NONE";
        }
    }

    const char* statusToString(NavStatus s) {
        switch (s) {
            case NAV_STATUS_DISCONNECTED: return "DISCONNECTED";
            case NAV_STATUS_CONNECTED_IDLE: return "CONNECTED_IDLE";
            case NAV_STATUS_NAVIGATING: return "NAVIGATING";
            case NAV_STATUS_PAUSED: return "PAUSED";
            case NAV_STATUS_STALE: return "STALE";
            default: return "UNKNOWN";
        }
    }

    void publishState() {
        JsonDocument doc;
        doc["status"] = statusToString(_state.status);
        doc["is_stale"] = _isStale;
        doc["seq"] = _state.seq;
        doc["is_sim"] = _state.is_sim;
        doc["maneuver"] = maneuverToString(_state.maneuver);
        doc["dist_m"] = _state.dist_to_turn_m;
        doc["road"] = _state.road;
        doc["instr"] = _state.instruction;
        doc["speed_mps"] = _state.speed_mps;
        doc["speed_kmh"] = (int)(_state.speed_mps * 3.6f);
        doc["heading"] = _state.heading_deg;
        doc["rem_dist_m"] = _state.rem_dist_m;
        doc["rem_time_s"] = _state.rem_time_s;
        doc["progress_pct"] = _state.progress_pct;
        doc["off_route"] = _state.off_route;
        doc["lat"] = _state.lat;
        doc["lon"] = _state.lon;
        doc["gps_acc_m"] = _state.gps_acc_m;
        doc["lcd_w"] = _width;
        doc["lcd_h"] = _height;

        UsbFramedSerial::emitEvent("ACCEPTED_STATE", doc);
    }
};

#endif // VIRTUAL_DISPLAY_H

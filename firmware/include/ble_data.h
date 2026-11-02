// BLE data models for RPMLOOP Navigator
// Mirrors the protocol spec — all state received from the companion app

#pragma once

#include <cstdint>
#include <cstring>

// Maximum lengths
static const int MAX_STREET_LEN   = 48;
static const int MAX_TITLE_LEN    = 48;
static const int MAX_ARTIST_LEN   = 48;
static const int MAX_ALBUM_LEN    = 48;
static const int MAX_POLYLINE_PTS = 256;

// ---- Connection state ----
enum class BleConnState : uint8_t {
    DISCONNECTED,
    ADVERTISING,
    CONNECTED,
    HANDSHAKE_OK
};

// ---- GPS Position ----
struct GpsPosition {
    double   lat        = 0.0;
    double   lon        = 0.0;
    float    speed_kmh  = 0.0f;
    float    heading    = 0.0f;
    float    accuracy_m = 0.0f;
    uint32_t timestamp  = 0;
    bool     valid      = false;
};

// ---- Navigation maneuver type ----
enum class Maneuver : uint8_t {
    NONE,
    STRAIGHT,
    TURN_LEFT,
    TURN_RIGHT,
    SLIGHT_LEFT,
    SLIGHT_RIGHT,
    SHARP_LEFT,
    SHARP_RIGHT,
    U_TURN_LEFT,
    U_TURN_RIGHT,
    ROUNDABOUT_LEFT,
    ROUNDABOUT_RIGHT,
    MERGE,
    FORK_LEFT,
    FORK_RIGHT,
    RAMP_LEFT,
    RAMP_RIGHT,
    DESTINATION,
    DESTINATION_LEFT,
    DESTINATION_RIGHT
};

// ---- Navigation state ----
struct NavState {
    bool     active       = false;
    Maneuver maneuver     = Maneuver::NONE;
    uint32_t distance_m   = 0;
    char     street[MAX_STREET_LEN]      = {0};
    char     next_street[MAX_STREET_LEN] = {0};
    uint16_t eta_min      = 0;
    float    remaining_km = 0.0f;
};

// ---- Route polyline ----
struct RoutePoint {
    float lat;
    float lon;
};

struct RoutePolyline {
    char       route_id[8] = {0};
    RoutePoint points[MAX_POLYLINE_PTS];
    uint16_t   count       = 0;
    bool       complete    = false;
};

// ---- Media state ----
struct MediaState {
    bool     playing     = false;
    char     title[MAX_TITLE_LEN]   = {0};
    char     artist[MAX_ARTIST_LEN] = {0};
    char     album[MAX_ALBUM_LEN]   = {0};
    uint16_t duration_s  = 0;
    uint16_t position_s  = 0;
    bool     has_art     = false;
};

// Album art buffer (max 120x120 JPEG, stored in PSRAM)
struct AlbumArt {
    uint8_t* jpeg_data   = nullptr;
    uint32_t jpeg_len    = 0;
    uint16_t* rgb565     = nullptr; // decoded pixels (120*120 = 14400 * 2 = 28.8KB)
    uint16_t width       = 0;
    uint16_t height      = 0;
    bool     decoded     = false;
};

// ---- Media command (device -> app) ----
enum class MediaCmd : uint8_t {
    NONE,
    PLAY,
    PAUSE,
    NEXT,
    PREVIOUS
};

// ---- Tracking state ----
struct TrackingState {
    bool     active          = false;
    float    distance_km     = 0.0f;
    uint32_t elapsed_s       = 0;
    float    avg_speed_kmh   = 0.0f;
    float    max_speed_kmh   = 0.0f;
    float    current_speed_kmh = 0.0f;
};

// ---- Brightness (software dim overlay) ----
struct BrightnessState {
    uint8_t level = 255;  // 0=fully dimmed, 255=full brightness
};

// ---- Aggregate device state (all BLE-received data) ----
struct DeviceState {
    BleConnState   ble;
    GpsPosition    gps;
    NavState       nav;
    RoutePolyline  route;
    MediaState     media;
    AlbumArt       art;
    TrackingState  tracking;
    BrightnessState brightness;
};

// Global device state
extern DeviceState deviceState;

// Parse maneuver string to enum
Maneuver parseManeuver(const char* str);
// Get maneuver display name
const char* maneuverName(Maneuver m);
// Get maneuver arrow symbol (UTF-8)
const char* maneuverSymbol(Maneuver m);

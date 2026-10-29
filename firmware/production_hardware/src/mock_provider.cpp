#include <Arduino.h>
#include <math.h>
#include <string.h>
#include "mock_provider.h"
#include "bike_state.h"

// ==============================================================================
// MockProvider — animates BikeState at 10 Hz.
//   * speed oscillates 38-52 km/h (sine)
//   * distance-to-turn counts down with speed; at 0 a new turn starts
//     (L/R alternating), cycling the street list
//   * remaining km drains with distance; demo route loops at ~0.2 km left
//   * track position advances only while "playing"; 2 demo tracks rotate
//   * ride timer / trip integrate speed only while tracking is active
//   * headset link toggles every ~20 s (demo of connect/disconnect UI)
//   * clock runs in real time from a 10:24 power-on base (no RTC on bench)
// While a live BLE nav session is fresh, nav fields are NOT touched — the
// phone owns them (bike_state_nav_live). Everything else keeps animating.
// ==============================================================================

static const char* kStreets[] = {"Grand Promenade", "Station Road", "Mall Road", "Canal Road"};
static const int   kStreetCount = 4;

struct DemoTrack { const char* title; const char* artist; int durSec; };
static const DemoTrack kTracks[] = {
    {"Midnight Ride (Instrumental)", "RPM Loop Sessions", 214},
    {"Highway Dawn",                 "RPM Loop Sessions", 187},
};

static uint32_t s_lastMs = 0;
static uint32_t s_bootMs = 0;
static int      s_streetIdx = 0;
static int      s_turnCount = 0;
static int      s_trackIdx = 0;
static uint32_t s_headsetToggleMs = 0;
static float    s_tripSpeedSum = 0.0f;   // for average speed
static uint32_t s_tripSamples = 0;

void mock_provider_init(void) {
    s_bootMs = millis();
    s_lastMs = s_bootMs;
    s_headsetToggleMs = s_bootMs;
    g_bike_state.headsetConnected = true;
}

void mock_provider_update(uint32_t nowMs) {
    if (nowMs - s_lastMs < 100) return;
    uint32_t dtMs = nowMs - s_lastMs;
    s_lastMs = nowMs;
    float dt = dtMs / 1000.0f;
    BikeState& b = g_bike_state;

    // ---- speed: smooth 38..52 km/h sine (period ~28 s) ----
    float phase = (nowMs - s_bootMs) / 1000.0f;
    b.speedKmh = 45.0f + 7.0f * sinf(phase * 2.0f * PI / 28.0f);
    float distStep_m = (b.speedKmh / 3.6f) * dt;

    // ---- navigation (skipped while the phone owns these fields) ----
    if (!bike_state_nav_live(nowMs)) {
        b.navigating = true;
        b.navStale = false;
        b.gpsAccuracy_m = 2.4f + 1.1f * (0.5f + 0.5f * sinf(phase * 0.7f));  // 2.4..3.5 m
        b.distToTurn_m -= distStep_m;
        b.remainKm -= distStep_m / 1000.0f;
        if (b.distToTurn_m <= 0.0f) {
            s_turnCount++;
            s_streetIdx = (s_streetIdx + 1) % kStreetCount;
            b.turnDir = (s_turnCount % 3 == 0) ? TURN_DIR_STRAIGHT
                        : (s_turnCount % 2 == 0) ? TURN_DIR_LEFT : TURN_DIR_RIGHT;
            strncpy(b.streetName, kStreets[s_streetIdx], sizeof(b.streetName) - 1);
            b.streetName[sizeof(b.streetName) - 1] = '\0';
            snprintf(b.instruction, sizeof(b.instruction), "%s onto %s",
                     b.turnDir == TURN_DIR_LEFT ? "Turn left" :
                     b.turnDir == TURN_DIR_RIGHT ? "Turn right" : "Continue straight on",
                     b.streetName);
            b.distToTurn_m = 140.0f + (float)((s_turnCount * 53) % 160);   // 140..300 m
        }
        if (b.remainKm < 0.2f) {           // loop the demo route
            b.remainKm = 2.4f;
            b.progressPct = 0;
        }
        b.progressPct = (uint8_t)(100.0f * (1.0f - b.remainKm / 2.4f));
        b.etaMin = (b.speedKmh > 1.0f) ? (int)(b.remainKm / b.speedKmh * 60.0f) + 1 : 0;
    }

    // ---- music ----
    if (b.playing) {
        b.trackPosSec += (int)dt;
        if (b.trackPosSec >= b.trackDurSec) {   // auto-advance demo track
            s_trackIdx = (s_trackIdx + 1) % 2;
            strncpy(b.trackTitle, kTracks[s_trackIdx].title, sizeof(b.trackTitle) - 1);
            strncpy(b.trackArtist, kTracks[s_trackIdx].artist, sizeof(b.trackArtist) - 1);
            b.trackDurSec = kTracks[s_trackIdx].durSec;
            b.trackPosSec = 0;
        }
    }

    // ---- headset demo toggle every ~20 s ----
    if (nowMs - s_headsetToggleMs > 20000) {
        s_headsetToggleMs = nowMs;
        b.headsetConnected = !b.headsetConnected;
    }

    // ---- ride tracking ----
    if (b.trackingActive) {
        b.rideTimeSec += (int)dt;
        b.tripKm += distStep_m / 1000.0f;
        if (b.speedKmh > b.maxKmh) b.maxKmh = b.speedKmh;
        s_tripSpeedSum += b.speedKmh;
        s_tripSamples++;
        b.avgKmh = s_tripSamples ? s_tripSpeedSum / (float)s_tripSamples : 0.0f;
    }

    // ---- clock: 10:24 base + elapsed real time ----
    uint32_t elapsedSec = (nowMs - s_bootMs) / 1000;
    uint32_t totalMin = 10 * 60 + 24 + elapsedSec / 60;
    b.clockHour = (int)((totalMin / 60) % 24);
    b.clockMin = (int)(totalMin % 60);
}

// ---- UI control seam (mock implementations) ----
void bike_control_toggle_play(void)     { g_bike_state.playing = !g_bike_state.playing; }
void bike_control_next_track(void) {
    s_trackIdx = (s_trackIdx + 1) % 2;
    strncpy(g_bike_state.trackTitle, kTracks[s_trackIdx].title, sizeof(g_bike_state.trackTitle) - 1);
    strncpy(g_bike_state.trackArtist, kTracks[s_trackIdx].artist, sizeof(g_bike_state.trackArtist) - 1);
    g_bike_state.trackDurSec = kTracks[s_trackIdx].durSec;
    g_bike_state.trackPosSec = 0;
}
void bike_control_prev_track(void) {
    if (g_bike_state.trackPosSec > 3) { g_bike_state.trackPosSec = 0; return; }
    s_trackIdx = (s_trackIdx + 1) % 2;
    strncpy(g_bike_state.trackTitle, kTracks[s_trackIdx].title, sizeof(g_bike_state.trackTitle) - 1);
    strncpy(g_bike_state.trackArtist, kTracks[s_trackIdx].artist, sizeof(g_bike_state.trackArtist) - 1);
    g_bike_state.trackDurSec = kTracks[s_trackIdx].durSec;
    g_bike_state.trackPosSec = 0;
}
void bike_control_toggle_tracking(void) { g_bike_state.trackingActive = !g_bike_state.trackingActive; }
void bike_control_reset_tracking(void) {
    g_bike_state.tripKm = 0; g_bike_state.rideTimeSec = 0;
    g_bike_state.maxKmh = 0; g_bike_state.avgKmh = 0;
    s_tripSpeedSum = 0; s_tripSamples = 0;
}
void bike_control_toggle_headset(void) {
    g_bike_state.headsetConnected = !g_bike_state.headsetConnected;
    s_headsetToggleMs = millis();
}
void bike_control_set_volume(int pct) {
    if (pct < 0) pct = 0; if (pct > 100) pct = 100;
    g_bike_state.volumePct = pct;
}
void bike_control_set_brightness(int pct) {
    if (pct < 40) pct = 40; if (pct > 100) pct = 100;
    g_bike_state.brightnessPct = pct;
}

#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include <Arduino.h>
#include <SPI.h>
#include "pinout_config.h"
#include "protocol_def.h"

// Color Palette (Google Maps Daylight & Night Themes in RGB565)
#define COLOR_GMAPS_GREEN   0x0BB0  // #137333 Google Navigation Banner Green
#define COLOR_GMAPS_BLUE    0x1B9D  // #1A73E8 Google Route Blue
#define COLOR_GMAPS_RED     0xE206  // #EA4335 Waypoint Destination Red
#define COLOR_ASPHALT_DAY   0xEF5D  // #E8EAED Google Light Grey Terrain
#define COLOR_ASPHALT_NIGHT 0x2124  // #202124 Google Dark Charcoal Terrain
#define COLOR_WHITE         0xFFFF
#define COLOR_BLACK         0x0000
#define COLOR_TEXT_MUTED    0x9CD3

class DisplayDriver {
private:
    uint16_t _width;
    uint16_t _height;
    bool _isNightMode;
    uint8_t _brightness;
    NavigationState _currentState;

public:
    DisplayDriver(uint16_t w = 320, uint16_t h = 240)
        : _width(w), _height(h), _isNightMode(false), _brightness(80) {
        memset(&_currentState, 0, sizeof(NavigationState));
        _currentState.status = NAV_STATUS_DISCONNECTED;
    }

    void init() {
        // Initialize Backlight PWM LEDC
        ledcSetup(LCD_BL_CHANNEL, LCD_BL_FREQ, LCD_BL_RES);
        ledcAttachPin(LCD_BL, LCD_BL_CHANNEL);
        setBrightness(_brightness);

        // Hardware reset sequence
        pinMode(LCD_RST, OUTPUT);
        digitalWrite(LCD_RST, LOW);
        delay(20);
        digitalWrite(LCD_RST, HIGH);
        delay(120);

        // Clear screen to standby boot screen
        drawStandbyScreen();
    }

    void setBrightness(uint8_t percent) {
        if (percent > 100) percent = 100;
        _brightness = percent;
        uint32_t duty = (percent * 255) / 100;
        ledcWrite(LCD_BL_CHANNEL, duty);
    }

    void setNightMode(bool night) {
        _isNightMode = night;
        renderNavScreen(_currentState);
    }

    void drawStandbyScreen() {
        // Standby screen when waiting for mobile app pairing
        // (Clean high-contrast screen showing RPMLOOP logo and BLE pairing status)
    }

    void renderNavScreen(const NavigationState& state) {
        _currentState = state;
        // 1. Draw Left Pane: Google Maps Style Maneuver HUD (96px width)
        // 2. Draw Right Pane: High-contrast 224px vector turn map
        // 3. Draw Vehicle Chevron at bottom center with accuracy halo
    }
};

#endif // DISPLAY_DRIVER_H

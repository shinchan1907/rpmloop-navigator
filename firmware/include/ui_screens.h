// UI screen manager for RPMLOOP Navigator
// All LVGL screens: Splash, Connect, Home, Navigation, Maps, Music, Tracking, Settings
// Plus pull-down quick shade

#pragma once

#include <lvgl.h>

// Screen identifiers
enum class Screen : uint8_t {
    SPLASH,
    CONNECT,
    HOME,
    NAVIGATION,
    MAPS,
    MUSIC,
    TRACKING,
    SETTINGS,
    DOCS
};

// Initialize the UI system (creates all screens)
void ui_init();

// Navigate to a screen (with transition animation)
void ui_navigate(Screen screen);

// Get current screen
Screen ui_current_screen();

// Update dynamic content (call from loop, ~10 Hz)
void ui_update();

// Quick shade control
void ui_shade_show();
void ui_shade_hide();
bool ui_shade_visible();

// Brightness overlay (software dimming)
void ui_set_brightness(uint8_t level);

// Set Home screen active page (0, 1, 2)
void ui_home_set_page(int page);

// Bike Documents helpers
void ui_docs_show_rc();
void ui_docs_show_aadhar();
void ui_docs_show_dl();
void ui_docs_flip();
void ui_docs_enter_fullscreen();
void ui_docs_exit_fullscreen();


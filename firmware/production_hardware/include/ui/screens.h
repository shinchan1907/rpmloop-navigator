#ifndef UI_SCREENS_H
#define UI_SCREENS_H

// Screen manager for the embedded LVGL UI. Screens are built lazily, kept
// alive, and only READ BikeState (refreshed on a 250 ms timer). Navigation
// is touch-only: Home tiles go out to each screen, and every sub-screen
// carries its own Home button to come back.

enum UiScreen {
    UI_BOOT = 0, UI_CONNECT, UI_HOME, UI_NAV, UI_MAPS,
    UI_MUSIC, UI_HEADSET, UI_TRACKING, UI_SETTINGS, UI_COUNT
};

void ui_init(void);
void ui_show(UiScreen s);
UiScreen ui_current(void);

#endif // UI_SCREENS_H

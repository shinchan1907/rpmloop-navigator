#ifndef UI_SCREENS_H
#define UI_SCREENS_H

// Screen manager for the embedded LVGL UI. Screens are built lazily, kept
// alive, and only READ BikeState (refreshed on a 250 ms timer). Navigation:
// HOME button (polled in main) -> Home from anywhere; BACK (ESC key) -> Home;
// PLAY button -> ui_on_play_button() (context action per screen).

enum UiScreen {
    UI_BOOT = 0, UI_HOME, UI_NAV, UI_MAPS,
    UI_MUSIC, UI_HEADSET, UI_TRACKING, UI_SETTINGS, UI_COUNT
};

void ui_init(void);
void ui_show(UiScreen s);
UiScreen ui_current(void);
void ui_on_play_button(void);

#endif // UI_SCREENS_H

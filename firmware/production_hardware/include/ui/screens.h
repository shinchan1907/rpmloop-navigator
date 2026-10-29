#ifndef UI_SCREENS_H
#define UI_SCREENS_H

// Screen manager for the embedded LVGL UI. Screens are built lazily, kept
// alive, and only READ BikeState (refreshed on a 250 ms timer). Navigation
// is touch-only: Home tiles go out to each screen, sub-screens carry a
// Home button, and a swipe-down quick shade (Home / brightness / BT
// status) is reachable from every screen — including Nav, where it is
// the way back Home.
// NOTE (2.4.0): there is deliberately NO Headset screen — the rider's
// headset pairs with the phone, never with this device. BikeState still
// carries headset fields for the Music screen's status line.

enum UiScreen {
    UI_BOOT = 0, UI_CONNECT, UI_HOME, UI_NAV, UI_MAPS,
    UI_MUSIC, UI_TRACKING, UI_SETTINGS, UI_COUNT
};

void ui_init(void);
void ui_show(UiScreen s);
UiScreen ui_current(void);

#endif // UI_SCREENS_H

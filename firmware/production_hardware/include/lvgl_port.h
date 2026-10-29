#ifndef LVGL_PORT_H
#define LVGL_PORT_H

#include <lvgl.h>
#include "lgfx_config.h"

// LVGL port layer: owns the LGFX display, draw buffers, the 5 ms tick timer
// and the two physical input devices (EC11 encoder + keypad from joystick and
// buttons). Screens register their focus group objects via the group handed
// out by lvgl_port_group().

// Custom keys delivered by the keypad indev for joystick directions.
// LVGL's own LV_KEY_LEFT/RIGHT are reserved for the encoder (focus movement
// + zoom), so joystick input uses private codes that screens interpret
// themselves (pan map, prev/next track, adjust slider...).
#define UI_KEY_JOY_LEFT   0x2001
#define UI_KEY_JOY_RIGHT  0x2002
#define UI_KEY_JOY_UP     0x2003
#define UI_KEY_JOY_DOWN   0x2004

extern LGFX tft;

void lvgl_port_init(void);
lv_group_t* lvgl_port_group(void);

// Polled by main loop (PLAY / HOME are momentary actions, not focus keys).
bool lvgl_port_play_pressed(void);
bool lvgl_port_home_pressed(void);

#endif // LVGL_PORT_H

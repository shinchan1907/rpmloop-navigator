#ifndef UI_COMMON_H
#define UI_COMMON_H

#include <lvgl.h>

// ==============================================================================
// Shared UI constants + tiny builders (palette per UI spec §5).
// Map colours are Google-Maps-like light; chrome is dark navy for sunlight
// readability on the small panel.
// ==============================================================================
#define COL_MAP_BG       0xE5E3DF
#define COL_ROAD         0xFFFFFF
#define COL_ROAD_EDGE    0xD9D9D9
#define COL_PARK         0xC6E4BA
#define COL_WATER        0xAAD3DF
#define COL_ROUTE        0x1A73E8
#define COL_MAP_LABEL    0x6E6E6E

#define COL_PANEL_BG     0x0A1120
#define COL_PANEL_CARD   0x16233C
#define COL_PANEL_DIV    0x22314D
#define COL_TEXT         0xFFFFFF
#define COL_TEXT_SECOND  0x9AA7BD
#define COL_TEXT_DIM     0xAEB9CC
#define COL_GREEN        0x22B573
#define COL_BLUE         0x1A73E8
#define COL_RED          0xE53935

// TurnDir (bike_state.h) -> LVGL symbol glyph for the maneuver arrow.
const char* ui_turn_symbol(int turnDir);

// "m:ss" formatting for track / ride times. buf must hold >= 8 chars.
void ui_format_mmss(char* buf, int totalSec);

// Label factory: creates a label with font + colour, left-aligned, no wrap
// config (caller sets width / long mode as needed).
lv_obj_t* ui_label(lv_obj_t* parent, const char* text, const lv_font_t* font, uint32_t colorHex);

// Button factory with the shared dark-card style + focus ring.
lv_obj_t* ui_button(lv_obj_t* parent, const char* text, const lv_font_t* font);

#endif // UI_COMMON_H

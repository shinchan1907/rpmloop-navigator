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
#define COL_BUILDING     0xD8D8D8   // pale-grey city blocks on the nav map
#define COL_BUILDING_2   0xCFCFCF   // second building shade (depth)
#define COL_CHEVRON      0x4FC3F7   // light-blue position chevron fill
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

// ---- Vibrant theme (2.3.0, owner reference: dark EV-cluster UI) ----
// Deep navy -> indigo vertical gradient behind neon cards; dark text sits
// on the bright cards, big white numerals on the dark background.
#define COL_BG_TOP       0x0A1030   // gradient start (top)
#define COL_BG_BOT       0x241257   // gradient end (bottom)
#define COL_VIB_LIME     0x8BE000
#define COL_VIB_CYAN     0x2ED3E6
#define COL_VIB_PINK     0xFF4D8D
#define COL_VIB_AMBER    0xFFB020
#define COL_CARD_TEXT    0x10142E   // dark text on vibrant cards
#define COL_CARD_TEXT2   0x2A3050   // softer dark text (card status lines)
#define VIB_CARD_RADIUS  16

// Wallpaper image (defined in src/ui/bg_wallpaper.c, generated asset).
#ifdef __cplusplus
extern "C" {
#endif
extern const lv_img_dsc_t bg_wallpaper;
#ifdef __cplusplus
}
#endif

// Paint a screen root with the 2.4.0 wallpaper (sky-blue/lime image) as
// its background: an lv_img pinned at (0,0) as the bottom child. The
// screen's own bg colour stays as the fallback behind the image.
void ui_apply_wallpaper(lv_obj_t* scr);

// Translucent dark-navy panel — keeps white text readable over the
// bright parts of the wallpaper (Music / Settings / Connect blocks).
lv_obj_t* ui_glass_panel(lv_obj_t* parent, int x, int y, int w, int h);

// Paint a screen root with the (legacy, pre-2.4.0) vibrant vertical
// gradient background. Kept for API stability; screens now use the
// wallpaper via ui_apply_wallpaper().
void ui_apply_gradient_bg(lv_obj_t* scr);

// Vibrant card factory: solid neon fill, radius 16, no border/padding.
// Content is NOT auto-laid-out — callers place labels at absolute offsets.
lv_obj_t* ui_vibrant_card(lv_obj_t* parent, int x, int y, int w, int h, uint32_t colorHex);

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

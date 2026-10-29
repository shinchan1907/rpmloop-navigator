#include "ui/ui_common.h"
#include "bike_state.h"

const char* ui_turn_symbol(int turnDir) {
    switch (turnDir) {
        case TURN_DIR_LEFT:        return LV_SYMBOL_LEFT;
        case TURN_DIR_RIGHT:       return LV_SYMBOL_RIGHT;
        case TURN_DIR_UTURN:       return LV_SYMBOL_LOOP;
        case TURN_DIR_ROUNDABOUT:  return LV_SYMBOL_REFRESH;
        case TURN_DIR_DESTINATION: return LV_SYMBOL_GPS;
        case TURN_DIR_STRAIGHT:
        default:                   return LV_SYMBOL_UP;
    }
}

void ui_format_mmss(char* buf, int totalSec) {
    if (totalSec < 0) totalSec = 0;
    // Manual formatting (no snprintf dependency surprises in hot UI code).
    int m = totalSec / 60;
    int s = totalSec % 60;
    char* p = buf;
    if (m >= 10) *p++ = (char)('0' + (m / 10) % 10);
    *p++ = (char)('0' + m % 10);
    *p++ = ':';
    *p++ = (char)('0' + s / 10);
    *p++ = (char)('0' + s % 10);
    *p = '\0';
}

lv_obj_t* ui_label(lv_obj_t* parent, const char* text, const lv_font_t* font, uint32_t colorHex) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(colorHex), 0);
    return l;
}

lv_obj_t* ui_button(lv_obj_t* parent, const char* text, const lv_font_t* font) {
    lv_obj_t* b = lv_btn_create(parent);
    lv_obj_set_style_bg_color(b, lv_color_hex(COL_PANEL_CARD), 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(COL_BLUE), LV_STATE_FOCUSED);
    lv_obj_set_style_radius(b, 10, 0);
    lv_obj_set_style_border_width(b, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(b, lv_color_hex(COL_TEXT), LV_STATE_FOCUSED);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(COL_TEXT), 0);
    lv_obj_center(l);
    return b;
}

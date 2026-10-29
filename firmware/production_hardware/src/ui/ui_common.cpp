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

void ui_apply_gradient_bg(lv_obj_t* scr) {
    lv_obj_set_style_bg_color(scr, lv_color_hex(COL_BG_TOP), 0);
    lv_obj_set_style_bg_grad_color(scr, lv_color_hex(COL_BG_BOT), 0);
    lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
}

void ui_apply_wallpaper(lv_obj_t* scr) {
    lv_obj_t* img = lv_img_create(scr);
    lv_img_set_src(img, &bg_wallpaper);
    lv_obj_set_pos(img, 0, 0);
    lv_obj_move_to_index(img, 0);   // bottom child: everything else draws over it
    lv_obj_clear_flag(img, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(img, LV_OBJ_FLAG_CLICKABLE);
}

lv_obj_t* ui_glass_panel(lv_obj_t* parent, int x, int y, int w, int h) {
    lv_obj_t* p = lv_obj_create(parent);
    lv_obj_set_pos(p, x, y);
    lv_obj_set_size(p, w, h);
    lv_obj_set_style_bg_color(p, lv_color_hex(COL_PANEL_BG), 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_60, 0);
    lv_obj_set_style_radius(p, 12, 0);
    lv_obj_set_style_border_width(p, 0, 0);
    lv_obj_set_style_pad_all(p, 0, 0);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_CLICKABLE);
    return p;
}

lv_obj_t* ui_vibrant_card(lv_obj_t* parent, int x, int y, int w, int h, uint32_t colorHex) {
    lv_obj_t* c = lv_obj_create(parent);
    lv_obj_set_pos(c, x, y);
    lv_obj_set_size(c, w, h);
    // Glossy vertical gradient (2.4.0): lightened neon at the top easing
    // into the base neon at the bottom, a 1 px darker shade border and a
    // soft drop shadow so the card pops off the bright wallpaper.
    lv_color_t base = lv_color_hex(colorHex);
    lv_obj_set_style_bg_color(c, lv_color_lighten(base, LV_OPA_20), 0);
    lv_obj_set_style_bg_grad_color(c, base, 0);
    lv_obj_set_style_bg_grad_dir(c, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(c, VIB_CARD_RADIUS, 0);
    lv_obj_set_style_border_width(c, 1, 0);
    lv_obj_set_style_border_color(c, lv_color_darken(base, LV_OPA_40), 0);
    lv_obj_set_style_border_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(c, 0, 0);
    lv_obj_set_style_shadow_width(c, 10, 0);
    lv_obj_set_style_shadow_color(c, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(c, LV_OPA_30, 0);
    lv_obj_set_style_shadow_ofs_x(c, 0, 0);
    lv_obj_set_style_shadow_ofs_y(c, 3, 0);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    return c;
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

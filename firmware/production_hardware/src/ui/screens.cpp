#include <stdio.h>
#include <string.h>
#include "ui/screens.h"
#include "ui/ui_common.h"
#include "ui/map_view.h"
#include "lvgl_port.h"
#include "bike_state.h"
#include "protocol_def.h"   // RPMLOPP_FIRMWARE_VERSION (About row)

// ==============================================================================
// All 8 screens. Conventions:
//  * A screen's widgets are created once (lazy build) and only refreshed.
//  * Refresh functions READ g_bike_state; user actions go through the
//    bike_control_*() seam — screens never write state fields directly.
//  * Input: encoder rotates focus (LVGL group), encoder push / joystick push
//    = ENTER, joystick directions arrive as UI_KEY_JOY_* custom keys and are
//    dispatched to the active screen's onKey handler, BACK = LV_KEY_ESC.
// ==============================================================================

struct ScrDef {
    lv_obj_t* root;
    lv_obj_t* focus[10];
    int focusN;
    void (*refresh)(void);
    void (*onKey)(uint32_t key);
};

static ScrDef S[UI_COUNT];
static UiScreen s_cur = UI_BOOT;
static lv_obj_t* s_dimOverlay = nullptr;

static void show_cb(lv_event_t* e) {
    ui_show((UiScreen)(intptr_t)lv_event_get_user_data(e));
}

static void key_dispatch_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_KEY) return;
    uint32_t key = lv_event_get_key(e);
    if (key == LV_KEY_ESC) {
        if (s_cur != UI_HOME && s_cur != UI_BOOT) ui_show(UI_HOME);
        return;
    }
    if (S[s_cur].onKey) S[s_cur].onKey(key);
}

static void add_focus(UiScreen id, lv_obj_t* obj) {
    if (S[id].focusN < 10) S[id].focus[S[id].focusN++] = obj;
    lv_obj_add_event_cb(obj, key_dispatch_cb, LV_EVENT_KEY, nullptr);
}

static lv_obj_t* make_root(UiScreen id, uint32_t bgHex) {
    lv_obj_t* r = lv_obj_create(nullptr);
    lv_obj_set_size(r, TFT_LCD_W, TFT_LCD_H);
    lv_obj_set_style_bg_color(r, lv_color_hex(bgHex), 0);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(r, 0, 0);
    lv_obj_set_style_radius(r, 0, 0);
    lv_obj_set_style_pad_all(r, 0, 0);
    lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);
    S[id].root = r;
    return r;
}

// Invisible key-catcher object used by the map screens: it holds group focus
// so encoder rotation (LV_KEY_LEFT/RIGHT) and joystick keys reach onKey.
static lv_obj_t* make_keys_obj(lv_obj_t* parent, int x, int y, int w, int h) {
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_border_width(o, 0, LV_STATE_FOCUSED);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
    return o;
}

static lv_obj_t* make_recentre_btn(lv_obj_t* parent) {
    lv_obj_t* b = lv_btn_create(parent);
    lv_obj_set_pos(b, 271, 181);                    // centre ~(288,198), r=17
    lv_obj_set_size(b, 34, 34);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_shadow_width(b, 8, 0);
    lv_obj_set_style_shadow_color(b, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(b, LV_OPA_50, 0);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, LV_SYMBOL_GPS);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(COL_BLUE), 0);
    lv_obj_center(l);
    return b;
}

static lv_obj_t* make_chip(lv_obj_t* parent, int x, int y, int w) {
    lv_obj_t* c = lv_obj_create(parent);
    lv_obj_set_pos(c, x, y);
    lv_obj_set_size(c, w, 20);
    lv_obj_set_style_bg_color(c, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_60, 0);
    lv_obj_set_style_radius(c, 10, 0);
    lv_obj_set_style_border_width(c, 0, 0);
    lv_obj_set_style_pad_all(c, 0, 0);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_CLICKABLE);
    return c;
}

// ============================================================ BOOT ==========
static lv_obj_t* bootBar;
static uint32_t  bootStartMs;

static void build_boot(void) {
    lv_obj_t* r = make_root(UI_BOOT, COL_PANEL_BG);
    lv_obj_t* logo = ui_label(r, "BIKE NAV", &lv_font_montserrat_28, COL_TEXT);
    lv_obj_set_pos(logo, 86, 74);
    lv_obj_t* sub = ui_label(r, "R P M L O O P   N A V I G A T O R", &lv_font_montserrat_10, COL_TEXT_SECOND);
    lv_obj_set_pos(sub, 74, 112);
    bootBar = lv_bar_create(r);
    lv_obj_set_pos(bootBar, 70, 146);
    lv_obj_set_size(bootBar, 180, 8);
    lv_bar_set_range(bootBar, 0, 100);
    lv_bar_set_value(bootBar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bootBar, lv_color_hex(COL_PANEL_DIV), 0);
    lv_obj_set_style_bg_color(bootBar, lv_color_hex(COL_BLUE), LV_PART_INDICATOR);
    lv_obj_set_style_radius(bootBar, 4, 0);
    lv_obj_set_style_radius(bootBar, 4, LV_PART_INDICATOR);
    bootStartMs = lv_tick_get();
}

static void refresh_boot(void) {
    uint32_t el = lv_tick_get() - bootStartMs;
    int pct = (int)(el * 100 / 1200);
    if (pct >= 100) { ui_show(UI_HOME); return; }
    lv_bar_set_value(bootBar, pct, LV_ANIM_OFF);
}

// ============================================================ HOME ==========
static lv_obj_t *homeClock, *homeGps, *homeBt;

static void build_home(void) {
    lv_obj_t* r = make_root(UI_HOME, COL_PANEL_BG);

    lv_obj_t* bar = lv_obj_create(r);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_size(bar, 320, 22);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x111C33), 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    homeGps = ui_label(bar, LV_SYMBOL_GPS " --", &lv_font_montserrat_10, COL_TEXT_SECOND);
    lv_obj_set_pos(homeGps, 6, 4);
    homeBt = ui_label(bar, LV_SYMBOL_CALL " --", &lv_font_montserrat_10, COL_TEXT_SECOND);
    lv_obj_set_pos(homeBt, 128, 4);
    lv_obj_t* usb = ui_label(bar, LV_SYMBOL_CHARGE, &lv_font_montserrat_10, COL_GREEN);
    lv_obj_set_pos(usb, 262, 4);
    homeClock = ui_label(bar, "10:24", &lv_font_montserrat_10, COL_TEXT);
    lv_obj_set_pos(homeClock, 282, 4);

    struct Tile { const char* text; UiScreen target; int x, y; };
    static const Tile kTiles[] = {
        {LV_SYMBOL_AUDIO "\nMUSIC",    UI_MUSIC,    10, 32},
        {LV_SYMBOL_GPS "\nMAPS",       UI_MAPS,     170, 32},
        {LV_SYMBOL_CALL "\nHEADSET",   UI_HEADSET,  10, 112},
        {LV_SYMBOL_BARS "\nTRACKING",  UI_TRACKING, 170, 112},
    };
    for (unsigned i = 0; i < sizeof(kTiles) / sizeof(kTiles[0]); i++) {
        lv_obj_t* b = ui_button(r, kTiles[i].text, &lv_font_montserrat_16);
        lv_obj_set_pos(b, kTiles[i].x, kTiles[i].y);
        lv_obj_set_size(b, 140, 70);
        lv_obj_add_event_cb(b, show_cb, LV_EVENT_CLICKED, (void*)(intptr_t)kTiles[i].target);
        add_focus(UI_HOME, b);
    }
    lv_obj_t* nav = ui_button(r, LV_SYMBOL_GPS "  NAVIGATE", &lv_font_montserrat_14);
    lv_obj_set_pos(nav, 10, 190);
    lv_obj_set_size(nav, 190, 42);
    lv_obj_add_event_cb(nav, show_cb, LV_EVENT_CLICKED, (void*)(intptr_t)UI_NAV);
    add_focus(UI_HOME, nav);
    lv_obj_t* set = ui_button(r, LV_SYMBOL_SETTINGS "  SETTINGS", &lv_font_montserrat_14);
    lv_obj_set_pos(set, 208, 190);
    lv_obj_set_size(set, 102, 42);
    lv_obj_add_event_cb(set, show_cb, LV_EVENT_CLICKED, (void*)(intptr_t)UI_SETTINGS);
    add_focus(UI_HOME, set);
}

static void refresh_home(void) {
    char buf[48];
    snprintf(buf, sizeof(buf), LV_SYMBOL_GPS " ±%.1fm", (double)g_bike_state.gpsAccuracy_m);
    lv_label_set_text(homeGps, buf);
    lv_label_set_text(homeBt, g_bike_state.headsetConnected ? LV_SYMBOL_CALL " Helmet" : LV_SYMBOL_CALL " --");
    snprintf(buf, sizeof(buf), "%02d:%02d", g_bike_state.clockHour, g_bike_state.clockMin);
    lv_label_set_text(homeClock, buf);
}

// ============================================================ NAV ===========
static MapView navMap;
static lv_obj_t *navPill, *navPillLbl, *navArrow, *navDist, *navUnit, *navStreet,
                *navInstr, *navSpeed, *navRem, *navEta, *navGpsLbl, *navZoomChip,
                *navZoomLbl, *navRecentre;
static uint32_t navZoomChipUntil = 0;
static int navLastDist = -1, navLastSpeed = -1;

static void nav_recentre(void) {
    navMap.setLocked(true);
    g_bike_state.mapLocked = true;
    g_bike_state.zoomLevel = navMap.zoom();
}

static void nav_onkey(uint32_t key) {
    switch (key) {
        case UI_KEY_JOY_LEFT:  navMap.setPan(-24, 0); g_bike_state.mapLocked = false; break;
        case UI_KEY_JOY_RIGHT: navMap.setPan(24, 0);  g_bike_state.mapLocked = false; break;
        case UI_KEY_JOY_UP:    navMap.setPan(0, -24); g_bike_state.mapLocked = false; break;
        case UI_KEY_JOY_DOWN:  navMap.setPan(0, 24);  g_bike_state.mapLocked = false; break;
        case LV_KEY_ENTER:     nav_recentre(); break;
        case LV_KEY_LEFT:      // encoder CCW = zoom out
            navMap.setZoom(navMap.zoom() - 1);
            g_bike_state.zoomLevel = navMap.zoom();
            navZoomChipUntil = lv_tick_get() + 1200;
            break;
        case LV_KEY_RIGHT:     // encoder CW = zoom in
            navMap.setZoom(navMap.zoom() + 1);
            g_bike_state.zoomLevel = navMap.zoom();
            navZoomChipUntil = lv_tick_get() + 1200;
            break;
        default: break;
    }
}

static void build_nav(void) {
    lv_obj_t* r = make_root(UI_NAV, COL_MAP_BG);
    navMap.create(r, 104, 0, 216, 240);

    lv_obj_t* keys = make_keys_obj(r, 104, 0, 216, 240);
    add_focus(UI_NAV, keys);

    // Left panel: 104 px, full height, dark navy (mandatory geometry).
    lv_obj_t* p = lv_obj_create(r);
    lv_obj_set_pos(p, 0, 0);
    lv_obj_set_size(p, 104, 240);
    lv_obj_set_style_bg_color(p, lv_color_hex(COL_PANEL_BG), 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(p, 0, 0);
    lv_obj_set_style_radius(p, 0, 0);
    lv_obj_set_style_pad_all(p, 0, 0);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);

    navPill = lv_obj_create(p);
    lv_obj_set_pos(navPill, 8, 8);
    lv_obj_set_size(navPill, 76, 16);
    lv_obj_set_style_bg_color(navPill, lv_color_hex(COL_GREEN), 0);
    lv_obj_set_style_radius(navPill, 8, 0);
    lv_obj_set_style_border_width(navPill, 0, 0);
    lv_obj_set_style_pad_all(navPill, 0, 0);
    lv_obj_clear_flag(navPill, LV_OBJ_FLAG_SCROLLABLE);
    navPillLbl = ui_label(navPill, "NAVIGATING", &lv_font_montserrat_8, COL_TEXT);
    lv_obj_center(navPillLbl);

    navArrow = ui_label(p, LV_SYMBOL_RIGHT, &lv_font_montserrat_28, COL_TEXT);
    lv_obj_set_pos(navArrow, 8, 30);
    navDist = ui_label(p, "150", &lv_font_montserrat_28, COL_TEXT);
    lv_obj_set_pos(navDist, 8, 62);
    navUnit = ui_label(p, "METERS", &lv_font_montserrat_8, COL_TEXT_SECOND);
    lv_obj_set_pos(navUnit, 8, 94);
    navStreet = ui_label(p, "Grand Promenade", &lv_font_montserrat_14, COL_TEXT);
    lv_obj_set_pos(navStreet, 8, 108);
    lv_obj_set_width(navStreet, 88);
    lv_label_set_long_mode(navStreet, LV_LABEL_LONG_WRAP);
    navInstr = ui_label(p, "", &lv_font_montserrat_10, COL_TEXT_DIM);
    lv_obj_set_pos(navInstr, 8, 146);
    lv_obj_set_width(navInstr, 88);
    lv_label_set_long_mode(navInstr, LV_LABEL_LONG_WRAP);

    lv_obj_t* div = lv_obj_create(p);
    lv_obj_set_pos(div, 8, 186);
    lv_obj_set_size(div, 88, 1);
    lv_obj_set_style_bg_color(div, lv_color_hex(COL_PANEL_DIV), 0);
    lv_obj_set_style_border_width(div, 0, 0);

    navSpeed = ui_label(p, "48", &lv_font_montserrat_24, COL_TEXT);
    lv_obj_set_pos(navSpeed, 8, 192);
    lv_obj_t* kmh = ui_label(p, "KM/H", &lv_font_montserrat_8, COL_TEXT_SECOND);
    lv_obj_set_pos(kmh, 52, 202);
    navRem = ui_label(p, "Rem: 2.4 km", &lv_font_montserrat_10, COL_TEXT);
    lv_obj_set_pos(navRem, 8, 218);
    navEta = ui_label(p, "ETA: 3 min", &lv_font_montserrat_8, COL_TEXT_SECOND);
    lv_obj_set_pos(navEta, 8, 230);

    // GPS chip (top-right over map) + zoom chip + recentre button.
    lv_obj_t* chip = make_chip(r, 220, 6, 94);
    navGpsLbl = ui_label(chip, "GPS: ±2.4m", &lv_font_montserrat_10, COL_TEXT);
    lv_obj_center(navGpsLbl);
    navZoomChip = make_chip(r, 110, 6, 44);
    navZoomLbl = ui_label(navZoomChip, "Z16", &lv_font_montserrat_10, COL_TEXT);
    lv_obj_center(navZoomLbl);
    lv_obj_add_flag(navZoomChip, LV_OBJ_FLAG_HIDDEN);
    navRecentre = make_recentre_btn(r);
    lv_obj_add_flag(navRecentre, LV_OBJ_FLAG_HIDDEN);
}

static void refresh_nav(void) {
    BikeState& b = g_bike_state;
    // Pill: status + colour.
    const char* pill = b.offRoute ? "OFF ROUTE" : b.navStale ? "STALE" :
                       b.navigating ? "NAVIGATING" : "IDLE";
    uint32_t pillCol = (b.offRoute || b.navStale) ? COL_RED : b.navigating ? COL_GREEN : COL_PANEL_DIV;
    lv_label_set_text(navPillLbl, pill);
    lv_obj_set_style_bg_color(navPill, lv_color_hex(pillCol), 0);

    lv_label_set_text(navArrow, ui_turn_symbol(b.turnDir));

    int dist = (int)(b.distToTurn_m + 0.5f);
    if (dist != navLastDist) {
        navLastDist = dist;
        char buf[24];
        if (dist >= 1000) {
            snprintf(buf, sizeof(buf), "%.1f", dist / 1000.0);
            lv_label_set_text(navUnit, "KM");
        } else {
            snprintf(buf, sizeof(buf), "%d", dist);
            lv_label_set_text(navUnit, "METERS");
        }
        lv_label_set_text(navDist, buf);
    }
    lv_label_set_text(navStreet, b.streetName);
    lv_label_set_text(navInstr, b.instruction);

    int spd = (int)(b.speedKmh + 0.5f);
    if (spd != navLastSpeed) {
        navLastSpeed = spd;
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", spd);
        lv_label_set_text(navSpeed, buf);
    }
    char buf[40];
    snprintf(buf, sizeof(buf), "Rem: %.1f km", (double)b.remainKm);
    lv_label_set_text(navRem, buf);
    snprintf(buf, sizeof(buf), "ETA: %d min", b.etaMin);
    lv_label_set_text(navEta, buf);
    snprintf(buf, sizeof(buf), "GPS: ±%.1fm", (double)b.gpsAccuracy_m);
    lv_label_set_text(navGpsLbl, buf);

    // Recentre button: visible ONLY while the map is unlocked (panned).
    if (b.mapLocked) lv_obj_add_flag(navRecentre, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(navRecentre, LV_OBJ_FLAG_HIDDEN);

    if ((int32_t)(navZoomChipUntil - lv_tick_get()) > 0) {
        char z[8];
        snprintf(z, sizeof(z), "Z%d", navMap.zoom());
        lv_label_set_text(navZoomLbl, z);
        lv_obj_clear_flag(navZoomChip, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(navZoomChip, LV_OBJ_FLAG_HIDDEN);
    }
}

// ============================================================ MAPS ==========
static MapView mapsMap;
static lv_obj_t *mapsRecentre, *mapsZoomChip, *mapsZoomLbl, *mapsFlash;
static uint32_t mapsZoomChipUntil = 0, mapsFlashUntil = 0;

static void maps_onkey(uint32_t key) {
    switch (key) {
        case UI_KEY_JOY_LEFT:  mapsMap.setPan(-24, 0); g_bike_state.mapLocked = false; break;
        case UI_KEY_JOY_RIGHT: mapsMap.setPan(24, 0);  g_bike_state.mapLocked = false; break;
        case UI_KEY_JOY_UP:    mapsMap.setPan(0, -24); g_bike_state.mapLocked = false; break;
        case UI_KEY_JOY_DOWN:  mapsMap.setPan(0, 24);  g_bike_state.mapLocked = false; break;
        case LV_KEY_ENTER:
            if (!mapsMap.locked()) { mapsMap.setLocked(true); g_bike_state.mapLocked = true; }
            else mapsFlashUntil = lv_tick_get() + 1000;   // "use this point" flash
            break;
        case LV_KEY_LEFT:
            mapsMap.setZoom(mapsMap.zoom() - 1);
            g_bike_state.zoomLevel = mapsMap.zoom();
            mapsZoomChipUntil = lv_tick_get() + 1200;
            break;
        case LV_KEY_RIGHT:
            mapsMap.setZoom(mapsMap.zoom() + 1);
            g_bike_state.zoomLevel = mapsMap.zoom();
            mapsZoomChipUntil = lv_tick_get() + 1200;
            break;
        default: break;
    }
}

static void build_maps(void) {
    lv_obj_t* r = make_root(UI_MAPS, COL_MAP_BG);
    mapsMap.create(r, 0, 0, 320, 240);
    lv_obj_t* keys = make_keys_obj(r, 0, 0, 320, 240);
    add_focus(UI_MAPS, keys);

    lv_obj_t* chip = make_chip(r, 6, 6, 56);
    lv_obj_t* l = ui_label(chip, "MAPS", &lv_font_montserrat_10, COL_TEXT);
    lv_obj_center(l);
    mapsZoomChip = make_chip(r, 262, 6, 44);
    mapsZoomLbl = ui_label(mapsZoomChip, "Z16", &lv_font_montserrat_10, COL_TEXT);
    lv_obj_center(mapsZoomLbl);
    lv_obj_add_flag(mapsZoomChip, LV_OBJ_FLAG_HIDDEN);
    mapsFlash = make_chip(r, 96, 108, 128);
    lv_obj_t* fl = ui_label(mapsFlash, "Point selected", &lv_font_montserrat_10, COL_TEXT);
    lv_obj_center(fl);
    lv_obj_add_flag(mapsFlash, LV_OBJ_FLAG_HIDDEN);
    mapsRecentre = make_recentre_btn(r);
    lv_obj_add_flag(mapsRecentre, LV_OBJ_FLAG_HIDDEN);
}

static void refresh_maps(void) {
    if (mapsMap.locked()) lv_obj_add_flag(mapsRecentre, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(mapsRecentre, LV_OBJ_FLAG_HIDDEN);
    if ((int32_t)(mapsZoomChipUntil - lv_tick_get()) > 0) {
        char z[8];
        snprintf(z, sizeof(z), "Z%d", mapsMap.zoom());
        lv_label_set_text(mapsZoomLbl, z);
        lv_obj_clear_flag(mapsZoomChip, LV_OBJ_FLAG_HIDDEN);
    } else lv_obj_add_flag(mapsZoomChip, LV_OBJ_FLAG_HIDDEN);
    if ((int32_t)(mapsFlashUntil - lv_tick_get()) > 0) lv_obj_clear_flag(mapsFlash, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(mapsFlash, LV_OBJ_FLAG_HIDDEN);
}

// ============================================================ MUSIC =========
static lv_obj_t *musTitle, *musArtist, *musBar, *musTime, *musPlayLbl, *musHeadset;
static char musLastTitle[48] = "";

static void music_play_cb(lv_event_t* e) { (void)e; bike_control_toggle_play(); }
static void music_next_cb(lv_event_t* e) { (void)e; bike_control_next_track(); }
static void music_prev_cb(lv_event_t* e) { (void)e; bike_control_prev_track(); }

static void music_onkey(uint32_t key) {
    if (key == UI_KEY_JOY_LEFT) bike_control_prev_track();
    else if (key == UI_KEY_JOY_RIGHT) bike_control_next_track();
}

static void build_music(void) {
    lv_obj_t* r = make_root(UI_MUSIC, COL_PANEL_BG);
    lv_obj_t* t = ui_label(r, "MUSIC", &lv_font_montserrat_14, COL_TEXT_SECOND);
    lv_obj_set_pos(t, 10, 8);

    lv_obj_t* art = lv_obj_create(r);
    lv_obj_set_pos(art, 118, 28);
    lv_obj_set_size(art, 84, 84);
    lv_obj_set_style_bg_color(art, lv_color_hex(COL_PANEL_CARD), 0);
    lv_obj_set_style_radius(art, 12, 0);
    lv_obj_set_style_border_width(art, 0, 0);
    lv_obj_clear_flag(art, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* note = ui_label(art, LV_SYMBOL_AUDIO, &lv_font_montserrat_32, COL_BLUE);
    lv_obj_center(note);

    musTitle = ui_label(r, "", &lv_font_montserrat_16, COL_TEXT);
    lv_obj_set_pos(musTitle, 10, 120);
    lv_obj_set_width(musTitle, 300);
    lv_label_set_long_mode(musTitle, LV_LABEL_LONG_DOT);
    musArtist = ui_label(r, "", &lv_font_montserrat_12, COL_TEXT_SECOND);
    lv_obj_set_pos(musArtist, 10, 144);
    lv_obj_set_width(musArtist, 300);
    lv_label_set_long_mode(musArtist, LV_LABEL_LONG_DOT);

    musBar = lv_bar_create(r);
    lv_obj_set_pos(musBar, 20, 170);
    lv_obj_set_size(musBar, 280, 6);
    lv_bar_set_range(musBar, 0, 100);
    lv_obj_set_style_bg_color(musBar, lv_color_hex(COL_PANEL_DIV), 0);
    lv_obj_set_style_bg_color(musBar, lv_color_hex(COL_BLUE), LV_PART_INDICATOR);
    lv_obj_set_style_radius(musBar, 3, 0);
    lv_obj_set_style_radius(musBar, 3, LV_PART_INDICATOR);
    musTime = ui_label(r, "0:00 / 0:00", &lv_font_montserrat_10, COL_TEXT_SECOND);
    lv_obj_set_pos(musTime, 20, 180);

    lv_obj_t* prev = ui_button(r, LV_SYMBOL_PREV, &lv_font_montserrat_16);
    lv_obj_set_pos(prev, 40, 198);
    lv_obj_set_size(prev, 64, 36);
    lv_obj_add_event_cb(prev, music_prev_cb, LV_EVENT_CLICKED, nullptr);
    add_focus(UI_MUSIC, prev);
    lv_obj_t* play = ui_button(r, LV_SYMBOL_PLAY, &lv_font_montserrat_16);
    lv_obj_set_pos(play, 118, 196);
    lv_obj_set_size(play, 84, 40);
    musPlayLbl = lv_obj_get_child(play, 0);
    lv_obj_add_event_cb(play, music_play_cb, LV_EVENT_CLICKED, nullptr);
    add_focus(UI_MUSIC, play);
    lv_obj_t* next = ui_button(r, LV_SYMBOL_NEXT, &lv_font_montserrat_16);
    lv_obj_set_pos(next, 216, 198);
    lv_obj_set_size(next, 64, 36);
    lv_obj_add_event_cb(next, music_next_cb, LV_EVENT_CLICKED, nullptr);
    add_focus(UI_MUSIC, next);

    musHeadset = ui_label(r, "", &lv_font_montserrat_10, COL_TEXT_DIM);
    lv_obj_set_pos(musHeadset, 196, 180);
}

static void refresh_music(void) {
    BikeState& b = g_bike_state;
    if (strcmp(musLastTitle, b.trackTitle) != 0) {
        strncpy(musLastTitle, b.trackTitle, sizeof(musLastTitle) - 1);
        lv_label_set_text(musTitle, b.trackTitle);
        lv_label_set_text(musArtist, b.trackArtist);
    }
    if (b.trackDurSec > 0) lv_bar_set_value(musBar, b.trackPosSec * 100 / b.trackDurSec, LV_ANIM_OFF);
    char tb[16], db[16], buf[40];
    ui_format_mmss(tb, b.trackPosSec);
    ui_format_mmss(db, b.trackDurSec);
    snprintf(buf, sizeof(buf), "%s / %s", tb, db);
    lv_label_set_text(musTime, buf);
    lv_label_set_text(musPlayLbl, b.playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
    lv_label_set_text(musHeadset, b.headsetConnected ? LV_SYMBOL_CALL " Rider Helmet" : LV_SYMBOL_CALL " No headset");
}

// ============================================================ HEADSET =======
static lv_obj_t *hsStatus, *hsDetail, *hsBtnLbl, *hsLog[3];
static int hsLastConn = -1;

static void headset_toggle_cb(lv_event_t* e) { (void)e; bike_control_toggle_headset(); }

static void build_headset(void) {
    lv_obj_t* r = make_root(UI_HEADSET, COL_PANEL_BG);
    lv_obj_t* t = ui_label(r, "HEADSET", &lv_font_montserrat_14, COL_TEXT_SECOND);
    lv_obj_set_pos(t, 10, 8);

    lv_obj_t* circ = lv_obj_create(r);
    lv_obj_set_pos(circ, 122, 26);
    lv_obj_set_size(circ, 76, 76);
    lv_obj_set_style_radius(circ, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(circ, lv_color_hex(COL_PANEL_CARD), 0);
    lv_obj_set_style_border_width(circ, 0, 0);
    lv_obj_clear_flag(circ, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* bt = ui_label(circ, LV_SYMBOL_BLUETOOTH, &lv_font_montserrat_28, COL_BLUE);
    lv_obj_center(bt);

    hsStatus = ui_label(r, "", &lv_font_montserrat_16, COL_TEXT);
    lv_obj_set_pos(hsStatus, 60, 110);
    lv_obj_set_width(hsStatus, 200);
    lv_obj_set_style_text_align(hsStatus, LV_TEXT_ALIGN_CENTER, 0);
    hsDetail = ui_label(r, "", &lv_font_montserrat_10, COL_TEXT_SECOND);
    lv_obj_set_pos(hsDetail, 60, 134);
    lv_obj_set_width(hsDetail, 200);
    lv_obj_set_style_text_align(hsDetail, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t* btn = ui_button(r, "CONNECT", &lv_font_montserrat_14);
    lv_obj_set_pos(btn, 95, 156);
    lv_obj_set_size(btn, 130, 38);
    hsBtnLbl = lv_obj_get_child(btn, 0);
    lv_obj_add_event_cb(btn, headset_toggle_cb, LV_EVENT_CLICKED, nullptr);
    add_focus(UI_HEADSET, btn);

    lv_obj_t* logBox = lv_obj_create(r);
    lv_obj_set_pos(logBox, 10, 202);
    lv_obj_set_size(logBox, 300, 32);
    lv_obj_set_style_bg_color(logBox, lv_color_hex(COL_PANEL_CARD), 0);
    lv_obj_set_style_radius(logBox, 6, 0);
    lv_obj_set_style_border_width(logBox, 0, 0);
    lv_obj_set_style_pad_all(logBox, 4, 0);
    lv_obj_clear_flag(logBox, LV_OBJ_FLAG_SCROLLABLE);
    for (int i = 0; i < 3; i++) {
        // Only two lines fit the slim log strip; third slot kept for parity
        // with the spec's 3-line log if the box grows.
        if (i == 2) { hsLog[i] = nullptr; continue; }
        hsLog[i] = ui_label(logBox, "", &lv_font_montserrat_8, COL_TEXT_DIM);
        lv_obj_set_pos(hsLog[i], 6, 3 + i * 13);
    }
}

static void refresh_headset(void) {
    BikeState& b = g_bike_state;
    int conn = b.headsetConnected ? 1 : 0;
    if (conn == hsLastConn) return;
    hsLastConn = conn;
    char buf[64];
    if (conn) {
        lv_label_set_text(hsStatus, "Connected");
        lv_obj_set_style_text_color(hsStatus, lv_color_hex(COL_GREEN), 0);
        lv_label_set_text(hsDetail, "Rider Helmet · A2DP (mock link)");
        lv_label_set_text(hsBtnLbl, "DISCONNECT");
        snprintf(buf, sizeof(buf), "%02d:%02d  BT radio ready", b.clockHour, b.clockMin);
        lv_label_set_text(hsLog[0], buf);
        lv_label_set_text(hsLog[1], "Link up: Rider Helmet · A2DP streaming ready");
    } else {
        lv_label_set_text(hsStatus, "Not connected");
        lv_obj_set_style_text_color(hsStatus, lv_color_hex(COL_RED), 0);
        lv_label_set_text(hsDetail, "Helmet headset not in range (mock)");
        lv_label_set_text(hsBtnLbl, "CONNECT");
        snprintf(buf, sizeof(buf), "%02d:%02d  BT radio ready", b.clockHour, b.clockMin);
        lv_label_set_text(hsLog[0], buf);
        lv_label_set_text(hsLog[1], "Scanning for headsets... last: Rider Helmet");
    }
}

// ============================================================ TRACKING ======
static lv_obj_t *trkSpeed, *trkTrip, *trkTime, *trkMaxAvg, *trkBtnLbl;

static void tracking_toggle_cb(lv_event_t* e) { (void)e; bike_control_toggle_tracking(); }
static void tracking_reset_cb(lv_event_t* e)  { (void)e; bike_control_reset_tracking(); }

static lv_obj_t* stat_card(lv_obj_t* parent, int x, int y, const char* caption, lv_obj_t** valueOut) {
    lv_obj_t* c = lv_obj_create(parent);
    lv_obj_set_pos(c, x, y);
    lv_obj_set_size(c, 145, 76);
    lv_obj_set_style_bg_color(c, lv_color_hex(COL_PANEL_CARD), 0);
    lv_obj_set_style_radius(c, 10, 0);
    lv_obj_set_style_border_width(c, 0, 0);
    lv_obj_set_style_pad_all(c, 0, 0);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* cap = ui_label(c, caption, &lv_font_montserrat_10, COL_TEXT_SECOND);
    lv_obj_set_pos(cap, 10, 8);
    *valueOut = ui_label(c, "--", &lv_font_montserrat_24, COL_TEXT);
    lv_obj_set_pos(*valueOut, 10, 28);
    return c;
}

static void build_tracking(void) {
    lv_obj_t* r = make_root(UI_TRACKING, COL_PANEL_BG);
    stat_card(r, 10, 10, "SPEED KM/H", &trkSpeed);
    stat_card(r, 165, 10, "TRIP KM", &trkTrip);
    stat_card(r, 10, 96, "RIDE TIME", &trkTime);
    stat_card(r, 165, 96, "MAX / AVG KM/H", &trkMaxAvg);

    lv_obj_t* toggle = ui_button(r, LV_SYMBOL_PLAY " START", &lv_font_montserrat_14);
    lv_obj_set_pos(toggle, 10, 186);
    lv_obj_set_size(toggle, 145, 42);
    trkBtnLbl = lv_obj_get_child(toggle, 0);
    lv_obj_add_event_cb(toggle, tracking_toggle_cb, LV_EVENT_CLICKED, nullptr);
    add_focus(UI_TRACKING, toggle);
    lv_obj_t* reset = ui_button(r, LV_SYMBOL_REFRESH " RESET", &lv_font_montserrat_14);
    lv_obj_set_pos(reset, 165, 186);
    lv_obj_set_size(reset, 145, 42);
    lv_obj_add_event_cb(reset, tracking_reset_cb, LV_EVENT_CLICKED, nullptr);
    add_focus(UI_TRACKING, reset);
}

static void refresh_tracking(void) {
    BikeState& b = g_bike_state;
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", (int)(b.speedKmh + 0.5f));
    lv_label_set_text(trkSpeed, buf);
    snprintf(buf, sizeof(buf), "%.2f", (double)b.tripKm);
    lv_label_set_text(trkTrip, buf);
    ui_format_mmss(buf, b.rideTimeSec);
    lv_label_set_text(trkTime, buf);
    snprintf(buf, sizeof(buf), "%d / %d", (int)(b.maxKmh + 0.5f), (int)(b.avgKmh + 0.5f));
    lv_label_set_text(trkMaxAvg, buf);
    lv_label_set_text(trkBtnLbl, b.trackingActive ? LV_SYMBOL_PAUSE " PAUSE" : LV_SYMBOL_PLAY " START");
}

// ============================================================ SETTINGS ======
static lv_obj_t *setBright, *setVol;

static void bright_changed_cb(lv_event_t* e) {
    bike_control_set_brightness((int)lv_slider_get_value(lv_event_get_target(e)));
}
static void vol_changed_cb(lv_event_t* e) {
    bike_control_set_volume((int)lv_slider_get_value(lv_event_get_target(e)));
}

static lv_obj_t* settings_slider(lv_obj_t* parent, int y, int vmin, int vmax, int val, lv_event_cb_t cb) {
    lv_obj_t* s = lv_slider_create(parent);
    lv_obj_set_pos(s, 118, y);
    lv_obj_set_size(s, 188, 10);
    lv_slider_set_range(s, vmin, vmax);
    lv_slider_set_value(s, val, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s, lv_color_hex(COL_PANEL_DIV), 0);
    lv_obj_set_style_bg_color(s, lv_color_hex(COL_BLUE), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s, lv_color_hex(COL_TEXT), LV_PART_KNOB);
    lv_obj_add_event_cb(s, cb, LV_EVENT_VALUE_CHANGED, nullptr);
    return s;
}

static void settings_onkey(uint32_t key) {
    if (key != UI_KEY_JOY_LEFT && key != UI_KEY_JOY_RIGHT) return;
    lv_obj_t* f = lv_group_get_focused(lvgl_port_group());
    int d = (key == UI_KEY_JOY_RIGHT) ? 5 : -5;
    if (f == setBright) {
        int v = (int)lv_slider_get_value(f) + d;
        if (v < 40) v = 40; if (v > 100) v = 100;
        lv_slider_set_value(f, v, LV_ANIM_OFF);
        bike_control_set_brightness(v);
    } else if (f == setVol) {
        int v = (int)lv_slider_get_value(f) + d;
        if (v < 0) v = 0; if (v > 100) v = 100;
        lv_slider_set_value(f, v, LV_ANIM_OFF);
        bike_control_set_volume(v);
    }
}

static void build_settings(void) {
    lv_obj_t* r = make_root(UI_SETTINGS, COL_PANEL_BG);
    lv_obj_t* t = ui_label(r, "SETTINGS", &lv_font_montserrat_14, COL_TEXT_SECOND);
    lv_obj_set_pos(t, 10, 8);

    lv_obj_t* l1 = ui_label(r, "Brightness", &lv_font_montserrat_14, COL_TEXT);
    lv_obj_set_pos(l1, 10, 44);
    setBright = settings_slider(r, 48, 40, 100, g_bike_state.brightnessPct, bright_changed_cb);
    add_focus(UI_SETTINGS, setBright);

    lv_obj_t* l2 = ui_label(r, "Volume", &lv_font_montserrat_14, COL_TEXT);
    lv_obj_set_pos(l2, 10, 84);
    setVol = settings_slider(r, 88, 0, 100, g_bike_state.volumePct, vol_changed_cb);
    add_focus(UI_SETTINGS, setVol);

    lv_obj_t* l3 = ui_label(r, "Units", &lv_font_montserrat_14, COL_TEXT);
    lv_obj_set_pos(l3, 10, 124);
    lv_obj_t* l3v = ui_label(r, "km/h (fixed)", &lv_font_montserrat_12, COL_TEXT_DIM);
    lv_obj_set_pos(l3v, 118, 126);

    lv_obj_t* l4 = ui_label(r, "About", &lv_font_montserrat_14, COL_TEXT);
    lv_obj_set_pos(l4, 10, 164);
    char buf[64];
    snprintf(buf, sizeof(buf), "RPMLOOP FW %s · LVGL 8.3 · ILI9341", RPMLOPP_FIRMWARE_VERSION);
    lv_obj_t* l4v = ui_label(r, buf, &lv_font_montserrat_10, COL_TEXT_DIM);
    lv_obj_set_pos(l4v, 10, 188);
    lv_obj_t* l5v = ui_label(r, "Mock data — GPS/BT drivers plug into BikeState", &lv_font_montserrat_10, COL_TEXT_DIM);
    lv_obj_set_pos(l5v, 10, 206);
}

// ============================================================ MANAGER =======
static void build_screen(UiScreen id) {
    if (S[id].root) return;
    switch (id) {
        case UI_BOOT:     build_boot();     S[id].refresh = refresh_boot; break;
        case UI_HOME:     build_home();     S[id].refresh = refresh_home; break;
        case UI_NAV:      build_nav();      S[id].refresh = refresh_nav; S[id].onKey = nav_onkey; break;
        case UI_MAPS:     build_maps();     S[id].refresh = refresh_maps; S[id].onKey = maps_onkey; break;
        case UI_MUSIC:    build_music();    S[id].refresh = refresh_music; S[id].onKey = music_onkey; break;
        case UI_HEADSET:  build_headset();  S[id].refresh = refresh_headset; break;
        case UI_TRACKING: build_tracking(); S[id].refresh = refresh_tracking; break;
        case UI_SETTINGS: build_settings(); S[id].refresh = nullptr; S[id].onKey = settings_onkey; break;
        default: break;
    }
}

static void apply_dim_overlay(void) {
    if (!s_dimOverlay) return;
    int dimPct = 100 - g_bike_state.brightnessPct;      // brightness 40..100 -> dim 60..0 %
    lv_obj_set_style_bg_opa(s_dimOverlay, (lv_opa_t)(dimPct * 255 / 100), 0);
}

static void refresh_timer_cb(lv_timer_t* t) {
    (void)t;
    if (S[s_cur].refresh) S[s_cur].refresh();
    apply_dim_overlay();
}

void ui_init(void) {
    memset(S, 0, sizeof(S));
    // Software brightness: translucent black overlay on the top layer
    // (the bench panel's backlight LED is wired straight to 3V3 — no PWM).
    s_dimOverlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_dimOverlay, TFT_LCD_W, TFT_LCD_H);
    lv_obj_set_style_bg_color(s_dimOverlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_dimOverlay, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_dimOverlay, 0, 0);
    lv_obj_set_style_radius(s_dimOverlay, 0, 0);
    lv_obj_set_style_pad_all(s_dimOverlay, 0, 0);
    lv_obj_clear_flag(s_dimOverlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_dimOverlay, LV_OBJ_FLAG_SCROLLABLE);

    build_screen(UI_BOOT);
    s_cur = UI_BOOT;
    lv_scr_load(S[UI_BOOT].root);
    lv_timer_create(refresh_timer_cb, 250, nullptr);
}

void ui_show(UiScreen id) {
    if (id == s_cur && S[id].root) return;
    build_screen(id);
    s_cur = id;
    lv_scr_load(S[id].root);
    lv_group_t* g = lvgl_port_group();
    lv_group_remove_all_objs(g);
    for (int i = 0; i < S[id].focusN; i++) lv_group_add_obj(g, S[id].focus[i]);
    if (S[id].focusN > 0) lv_group_focus_obj(S[id].focus[0]);
    if (S[id].refresh) S[id].refresh();
}

UiScreen ui_current(void) { return s_cur; }

void ui_on_play_button(void) {
    if (s_cur == UI_MUSIC) bike_control_toggle_play();
    else if (s_cur == UI_TRACKING) bike_control_toggle_tracking();
}

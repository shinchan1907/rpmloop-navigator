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
//    bike_control_*() seam — screens never write state fields directly
//    (the one exception: the Settings "Demo mode" switch is bound straight
//    to BikeState.demoMode, as specified).
//  * Input: touchscreen only (LVGL pointer indev from lvgl_port). Buttons,
//    tiles and sliders are tapped/dragged directly; the map pans by drag
//    (inside MapView) and zooms via on-screen +/- buttons; every sub-screen
//    carries a Home button (touch replacement for the old HOME key).
//  * Flow (2.3.0): Boot is an animated splash (~3.4 s) -> Connect (pair the
//    phone) -> Home. If BLE drops anywhere else, the manager sends the UI
//    back to Connect with a "Connection lost" note.
//  * 2.4.0: non-map screens sit on the sky/lime wallpaper image; layouts
//    are full-bleed (no dead bands); a swipe-down quick shade (Home,
//    brightness, BT status, Settings) hangs off the top edge of every
//    screen — a tappable pull zone + drag strip with manual drag
//    detection (2.4.2; LVGL gestures never fired on slow drags). There is NO Headset screen: the headset pairs with the
//    phone, never with this device.
// ==============================================================================

struct ScrDef {
    lv_obj_t* root;
    void (*refresh)(void);
};

static ScrDef S[UI_COUNT];
static UiScreen s_cur = UI_BOOT;
static lv_obj_t* s_dimOverlay = nullptr;

static void show_cb(lv_event_t* e) {
    ui_show((UiScreen)(intptr_t)lv_event_get_user_data(e));
}

// ---- quick shade (implemented below, just before the manager) ----------
// Every screen calls one of these at the END of its build: the pill is
// drawn last (topmost), and gesture bubbling must cover all children.
static void shade_add_handle(lv_obj_t* root);       // 28x5 pull pill, top-centre
static void shade_attach_swipe(lv_obj_t* root);     // LV_EVENT_GESTURE (plain screens)
static void shade_add_top_strip(lv_obj_t* root);    // drag strip (map screens)
static void shade_open(void);
static void shade_close(void);
static void shade_refresh(void);                    // BT row + slider sync (250 ms)
static void shade_init(void);                       // overlay on lv_layer_top()

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

// Home button carried by every sub-screen: dark translucent circle.
// (Touch replacement for the removed physical HOME button.)
static lv_obj_t* make_home_btn(lv_obj_t* parent, int x, int y) {
    lv_obj_t* b = lv_btn_create(parent);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, 30, 30);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_60, 0);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, LV_SYMBOL_HOME);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(COL_TEXT), 0);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, show_cb, LV_EVENT_CLICKED, (void*)(intptr_t)UI_HOME);
    return b;
}

// Round white zoom button for the map screens (+ / -), recentre-button style.
static lv_obj_t* make_zoom_btn(lv_obj_t* parent, const char* label, int x, int y, lv_event_cb_t cb) {
    lv_obj_t* b = lv_btn_create(parent);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, 30, 30);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_shadow_width(b, 6, 0);
    lv_obj_set_style_shadow_color(b, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(b, LV_OPA_50, 0);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, label);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(COL_BLUE), 0);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
    return b;
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

// ============================================================ BOOT ========
// Splash v3 (2.4.1): full-screen generated cluster artwork (owner-picked
// "Cluster Gauge" design) fading in from black, one light sweep across
// the arc, then the "RPM LOOP" wordmark + "RIDE CONNECTED" tagline rise
// into the artwork's clean dark lower third over a thin lime progress
// line. No gauge, no % counter (owner rejected v2 as boring). At ~3.2 s
// the manager advances: Home if the phone is already linked, otherwise
// the Connect screen.
static uint32_t  bootStartMs;

static void anim_text_opa_cb(void* obj, int32_t v) {
    lv_obj_set_style_text_opa((lv_obj_t*)obj, (lv_opa_t)v, 0);
}
static void anim_opa_cb(void* obj, int32_t v) {
    lv_obj_set_style_opa((lv_obj_t*)obj, (lv_opa_t)v, 0);
}
static void anim_bar_cb(void* obj, int32_t v) {
    lv_obj_set_width((lv_obj_t*)obj, (lv_coord_t)v);
}

static void build_boot(void) {
    lv_obj_t* r = make_root(UI_BOOT, COL_BG_TOP);

    // ---- full-screen splash artwork, fading in from black ----
    lv_obj_t* art = lv_img_create(r);
    lv_img_set_src(art, &splash_art);
    lv_obj_set_pos(art, 0, 0);
    lv_obj_set_style_opa(art, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(art, LV_OBJ_FLAG_CLICKABLE);
    lv_anim_t fa;
    lv_anim_init(&fa);
    lv_anim_set_var(&fa, art);
    lv_anim_set_exec_cb(&fa, (lv_anim_exec_xcb_t)anim_opa_cb);
    lv_anim_set_values(&fa, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_time(&fa, 700);
    lv_anim_set_path_cb(&fa, lv_anim_path_ease_out);
    lv_anim_start(&fa);

    // ---- one light sweep across the artwork ----
    lv_obj_t* sweep = lv_obj_create(r);
    lv_obj_set_size(sweep, 44, TFT_LCD_H);
    lv_obj_set_pos(sweep, -60, 0);
    lv_obj_set_style_bg_color(sweep, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(sweep, LV_OPA_20, 0);
    lv_obj_set_style_border_width(sweep, 0, 0);
    lv_obj_set_style_radius(sweep, 0, 0);
    lv_obj_set_style_pad_all(sweep, 0, 0);
    lv_obj_clear_flag(sweep, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(sweep, LV_OBJ_FLAG_CLICKABLE);
    lv_anim_t sa;
    lv_anim_init(&sa);
    lv_anim_set_var(&sa, sweep);
    lv_anim_set_exec_cb(&sa, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_values(&sa, -60, TFT_LCD_W + 20);
    lv_anim_set_time(&sa, 850);
    lv_anim_set_delay(&sa, 750);
    lv_anim_set_path_cb(&sa, lv_anim_path_ease_in_out);
    lv_anim_start(&sa);

    // ---- wordmark: rises + fades into the dark lower third ----
    lv_obj_t* logo = ui_label(r, "RPM LOOP", &lv_font_montserrat_32, COL_TEXT);
    lv_obj_set_width(logo, TFT_LCD_W);
    lv_obj_set_style_text_align(logo, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_letter_space(logo, 3, 0);
    lv_obj_set_pos(logo, 0, 176);
    lv_obj_set_style_text_opa(logo, LV_OPA_TRANSP, 0);
    lv_anim_t la;
    lv_anim_init(&la);
    lv_anim_set_var(&la, logo);
    lv_anim_set_time(&la, 500);
    lv_anim_set_delay(&la, 400);
    lv_anim_set_path_cb(&la, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&la, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_values(&la, 188, 176);
    lv_anim_start(&la);
    lv_anim_set_exec_cb(&la, (lv_anim_exec_xcb_t)anim_text_opa_cb);
    lv_anim_set_values(&la, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_start(&la);

    // ---- tagline ----
    lv_obj_t* tag = ui_label(r, "RIDE CONNECTED", &lv_font_montserrat_10, COL_TEXT_SECOND);
    lv_obj_set_pos(tag, 0, 216);
    lv_obj_set_width(tag, TFT_LCD_W);
    lv_obj_set_style_text_align(tag, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_letter_space(tag, 4, 0);
    lv_obj_set_style_text_opa(tag, LV_OPA_TRANSP, 0);
    lv_anim_t ta;
    lv_anim_init(&ta);
    lv_anim_set_var(&ta, tag);
    lv_anim_set_exec_cb(&ta, (lv_anim_exec_xcb_t)anim_text_opa_cb);
    lv_anim_set_values(&ta, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_time(&ta, 600);
    lv_anim_set_delay(&ta, 1100);
    lv_anim_start(&ta);

    // ---- progress line along the very bottom ----
    lv_obj_t* track = lv_obj_create(r);
    lv_obj_set_size(track, TFT_LCD_W, 3);
    lv_obj_set_pos(track, 0, TFT_LCD_H - 3);
    lv_obj_set_style_bg_color(track, lv_color_hex(0x10222E), 0);
    lv_obj_set_style_bg_opa(track, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(track, 0, 0);
    lv_obj_set_style_radius(track, 0, 0);
    lv_obj_set_style_pad_all(track, 0, 0);
    lv_obj_clear_flag(track, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(track, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* fill = lv_obj_create(r);
    lv_obj_set_size(fill, 0, 3);
    lv_obj_set_pos(fill, 0, TFT_LCD_H - 3);
    lv_obj_set_style_bg_color(fill, lv_color_hex(COL_VIB_LIME), 0);
    lv_obj_set_style_bg_opa(fill, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(fill, 0, 0);
    lv_obj_set_style_radius(fill, 0, 0);
    lv_obj_set_style_pad_all(fill, 0, 0);
    lv_obj_clear_flag(fill, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(fill, LV_OBJ_FLAG_CLICKABLE);
    lv_anim_t ba;
    lv_anim_init(&ba);
    lv_anim_set_var(&ba, fill);
    lv_anim_set_exec_cb(&ba, (lv_anim_exec_xcb_t)anim_bar_cb);
    lv_anim_set_values(&ba, 0, TFT_LCD_W);
    lv_anim_set_time(&ba, 2300);
    lv_anim_set_delay(&ba, 500);
    lv_anim_set_path_cb(&ba, lv_anim_path_ease_in_out);
    lv_anim_start(&ba);

    bootStartMs = lv_tick_get();
}

static void refresh_boot(void) {
    uint32_t el = lv_tick_get() - bootStartMs;
    if (el < 3200) return;
    // Splash done: a linked phone skips Connect entirely.
    ui_show(g_bike_state.bleConnected ? UI_HOME : UI_CONNECT);
}

// ============================================================ CONNECT =====
// Pairing screen. BLE advertising is ALWAYS on (started in setup()) — this
// screen is a pure UI state and never claims the device is "scanning".
// Idle: BT icon + "Connect your phone". Tap anywhere -> SEARCHING: three
// rings pulse outward around the icon while we wait. The moment
// BikeState.bleConnected flips true, a brief green-check "Connected"
// state (~600 ms) plays, then Home. Arriving here via a dropped link
// (manager sets s_connLost) shows a "Connection lost" note.
enum ConnState { CONN_IDLE, CONN_SEARCHING, CONN_SUCCESS };
static ConnState connState = CONN_IDLE;
static lv_obj_t *connRings[3], *connBtCircle, *connOkCircle,
                *connHead, *connSub, *connChip, *connLost;
static uint32_t connSuccessMs = 0;
static bool s_connLost = false;   // set by the manager's disconnect rule

static void ring_anim_cb(void* obj, int32_t v) {
    lv_coord_t sz = 64 + v;                       // 64 -> 164 px
    lv_obj_set_size((lv_obj_t*)obj, sz, sz);
    lv_obj_set_pos((lv_obj_t*)obj, 160 - sz / 2, 86 - sz / 2);
    lv_obj_set_style_border_opa((lv_obj_t*)obj, (lv_opa_t)((100 - v) * 2), 0);
}

static void conn_rings_start(void) {
    for (int i = 0; i < 3; i++) {
        lv_obj_clear_flag(connRings[i], LV_OBJ_FLAG_HIDDEN);
        ring_anim_cb(connRings[i], 0);
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, connRings[i]);
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)ring_anim_cb);
        lv_anim_set_values(&a, 0, 100);
        lv_anim_set_time(&a, 1600);
        lv_anim_set_delay(&a, i * 530);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&a);
    }
}

static void conn_rings_stop(void) {
    for (int i = 0; i < 3; i++) {
        lv_anim_del(connRings[i], (lv_anim_exec_xcb_t)ring_anim_cb);
        lv_obj_add_flag(connRings[i], LV_OBJ_FLAG_HIDDEN);
    }
}

// Idle visuals (also the reset used whenever the screen is entered).
static void conn_apply_idle(void) {
    connState = CONN_IDLE;
    conn_rings_stop();
    lv_obj_clear_flag(connBtCircle, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(connOkCircle, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(connChip, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(connHead, "Connect your phone");
    lv_label_set_text(connSub, "Pair with RPMLOOP from your phone's Bluetooth / companion app");
    if (s_connLost) lv_obj_clear_flag(connLost, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(connLost, LV_OBJ_FLAG_HIDDEN);
}

static void conn_tap_cb(lv_event_t* e) {
    (void)e;
    if (g_bike_state.bleConnected || connState != CONN_IDLE) return;
    connState = CONN_SEARCHING;
    lv_label_set_text(connHead, "Waiting for your phone\xE2\x80\xA6");
    lv_obj_clear_flag(connChip, LV_OBJ_FLAG_HIDDEN);
    conn_rings_start();
}

// Entry hook (called from ui_show): every visit starts from idle visuals;
// refresh_connect() immediately upgrades to the success state if the
// phone is somehow already linked.
static void connect_on_show(void) { conn_apply_idle(); }

static void build_connect(void) {
    lv_obj_t* r = make_root(UI_CONNECT, COL_BG_TOP);
    ui_apply_wallpaper(r);
    lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(r, conn_tap_cb, LV_EVENT_CLICKED, nullptr);

    connLost = ui_label(r, "Connection lost", &lv_font_montserrat_12, COL_VIB_AMBER);
    lv_obj_set_pos(connLost, 0, 16);
    lv_obj_set_width(connLost, TFT_LCD_W);
    lv_obj_set_style_text_align(connLost, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_flag(connLost, LV_OBJ_FLAG_HIDDEN);

    for (int i = 0; i < 3; i++) {
        lv_obj_t* ring = lv_obj_create(r);
        lv_obj_set_size(ring, 64, 64);
        lv_obj_set_pos(ring, 128, 54);
        lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(ring, lv_color_hex(COL_VIB_CYAN), 0);
        lv_obj_set_style_border_width(ring, 2, 0);
        lv_obj_set_style_pad_all(ring, 0, 0);
        lv_obj_clear_flag(ring, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(ring, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(ring, LV_OBJ_FLAG_HIDDEN);
        connRings[i] = ring;
    }

    connBtCircle = lv_obj_create(r);
    lv_obj_set_pos(connBtCircle, 124, 50);
    lv_obj_set_size(connBtCircle, 72, 72);
    lv_obj_set_style_radius(connBtCircle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(connBtCircle, lv_color_hex(0x1B2A52), 0);
    lv_obj_set_style_bg_opa(connBtCircle, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(connBtCircle, lv_color_hex(COL_VIB_CYAN), 0);
    lv_obj_set_style_border_width(connBtCircle, 2, 0);
    lv_obj_set_style_pad_all(connBtCircle, 0, 0);
    lv_obj_clear_flag(connBtCircle, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(connBtCircle, conn_tap_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* bt = ui_label(connBtCircle, LV_SYMBOL_BLUETOOTH, &lv_font_montserrat_28, COL_VIB_CYAN);
    lv_obj_center(bt);

    connOkCircle = lv_obj_create(r);
    lv_obj_set_pos(connOkCircle, 124, 50);
    lv_obj_set_size(connOkCircle, 72, 72);
    lv_obj_set_style_radius(connOkCircle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(connOkCircle, lv_color_hex(COL_VIB_LIME), 0);
    lv_obj_set_style_bg_opa(connOkCircle, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(connOkCircle, 0, 0);
    lv_obj_set_style_pad_all(connOkCircle, 0, 0);
    lv_obj_clear_flag(connOkCircle, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(connOkCircle, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t* ok = ui_label(connOkCircle, LV_SYMBOL_OK, &lv_font_montserrat_28, COL_CARD_TEXT);
    lv_obj_center(ok);
    lv_obj_add_flag(connOkCircle, LV_OBJ_FLAG_HIDDEN);

    // Glass panel behind the headline block: the wallpaper's midriff is
    // bright cyan, white text needs the dark backing to stay readable.
    ui_glass_panel(r, 16, 128, 288, 76);

    connHead = ui_label(r, "Connect your phone", &lv_font_montserrat_20, COL_TEXT);
    lv_obj_set_pos(connHead, 0, 138);
    lv_obj_set_width(connHead, TFT_LCD_W);
    lv_obj_set_style_text_align(connHead, LV_TEXT_ALIGN_CENTER, 0);
    connSub = ui_label(r, "Pair with RPMLOOP from your phone's Bluetooth / companion app",
                       &lv_font_montserrat_10, COL_TEXT_SECOND);
    lv_obj_set_pos(connSub, 28, 168);
    lv_obj_set_width(connSub, 264);
    lv_obj_set_style_text_align(connSub, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(connSub, LV_LABEL_LONG_WRAP);

    connChip = make_chip(r, 113, 212, 94);
    lv_obj_t* chipLbl = ui_label(connChip, LV_SYMBOL_BLUETOOTH " RPMLOOP", &lv_font_montserrat_10, COL_TEXT);
    lv_obj_center(chipLbl);
    lv_obj_add_flag(connChip, LV_OBJ_FLAG_HIDDEN);

    shade_add_handle(r);
    shade_attach_swipe(r);
}

static void refresh_connect(void) {
    BikeState& b = g_bike_state;
    if (b.bleConnected) {
        if (connState != CONN_SUCCESS) {
            connState = CONN_SUCCESS;
            connSuccessMs = lv_tick_get();
            conn_rings_stop();
            lv_obj_add_flag(connBtCircle, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(connOkCircle, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(connChip, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(connLost, LV_OBJ_FLAG_HIDDEN);
            s_connLost = false;
            lv_label_set_text(connHead, "Connected");
            lv_label_set_text(connSub, "Your phone is linked");
        } else if (lv_tick_get() - connSuccessMs >= 600) {
            ui_show(UI_HOME);
        }
        return;
    }
    if (connState == CONN_SUCCESS) {
        // Link dropped during the success beat — fall back to idle with
        // the lost-link note (the manager rule does not fire on this
        // screen, so handle it here).
        s_connLost = true;
        conn_apply_idle();
    }
}

// ============================================================ HOME ==========
// Full-bleed vibrant Home (2.4.0): wallpaper background, "RPM LOOP" header
// + connection pill, four big neon tiles (Navigate cyan / Music pink /
// Tracking lime / Settings amber, 146x78 in a 2x2 grid) whose status lines
// are REAL BikeState values only — and a bottom status strip (dark navy,
// translucent) that always fills the last band: street + live speed while
// navigating, "Phone connected" when linked, "Ready" otherwise.
static lv_obj_t *homePillDot, *homePillLbl, *homeNavStatus, *homeMusStatus,
                *homeTrkStatus, *homeStripDot, *homeStripLbl;

static lv_obj_t* home_tile(lv_obj_t* parent, int x, int y, uint32_t colorHex,
                           const char* icon, const char* title, UiScreen target,
                           lv_obj_t** statusOut) {
    lv_obj_t* c = ui_vibrant_card(parent, x, y, 146, 78, colorHex);
    lv_obj_add_event_cb(c, show_cb, LV_EVENT_CLICKED, (void*)(intptr_t)target);
    lv_obj_t* ic = ui_label(c, icon, &lv_font_montserrat_28, COL_CARD_TEXT);
    lv_obj_set_pos(ic, 12, 10);
    lv_obj_t* t = ui_label(c, title, &lv_font_montserrat_16, COL_CARD_TEXT);
    lv_obj_set_pos(t, 48, 14);
    *statusOut = ui_label(c, "", &lv_font_montserrat_10, COL_CARD_TEXT2);
    lv_obj_set_pos(*statusOut, 12, 56);
    lv_obj_set_width(*statusOut, 124);
    lv_label_set_long_mode(*statusOut, LV_LABEL_LONG_DOT);
    return c;
}

static void build_home(void) {
    lv_obj_t* r = make_root(UI_HOME, COL_BG_TOP);
    ui_apply_wallpaper(r);

    lv_obj_t* title = ui_label(r, "RPM LOOP", &lv_font_montserrat_16, COL_TEXT);
    lv_obj_set_pos(title, 8, 10);

    // Connection pill (top-right): dot + text, both refreshed live.
    lv_obj_t* pill = lv_obj_create(r);
    lv_obj_set_pos(pill, 200, 8);
    lv_obj_set_size(pill, 112, 20);
    lv_obj_set_style_bg_color(pill, lv_color_hex(COL_PANEL_CARD), 0);
    lv_obj_set_style_bg_opa(pill, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(pill, 10, 0);
    lv_obj_set_style_border_width(pill, 0, 0);
    lv_obj_set_style_pad_all(pill, 0, 0);
    lv_obj_clear_flag(pill, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(pill, LV_OBJ_FLAG_CLICKABLE);
    homePillDot = lv_obj_create(pill);
    lv_obj_set_pos(homePillDot, 8, 6);
    lv_obj_set_size(homePillDot, 8, 8);
    lv_obj_set_style_radius(homePillDot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(homePillDot, lv_color_hex(COL_TEXT_SECOND), 0);
    lv_obj_set_style_bg_opa(homePillDot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(homePillDot, 0, 0);
    lv_obj_set_style_pad_all(homePillDot, 0, 0);
    lv_obj_clear_flag(homePillDot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(homePillDot, LV_OBJ_FLAG_CLICKABLE);
    homePillLbl = ui_label(pill, "Not connected", &lv_font_montserrat_10, COL_TEXT_SECOND);
    lv_obj_set_pos(homePillLbl, 22, 4);

    // 2x2 tile grid: margin 8, tiles 146x78, gap 8 -> rows end at 204.
    home_tile(r, 8, 40, COL_VIB_CYAN, LV_SYMBOL_GPS, "NAVIGATE", UI_NAV, &homeNavStatus);
    home_tile(r, 162, 40, COL_VIB_PINK, LV_SYMBOL_AUDIO, "MUSIC", UI_MUSIC, &homeMusStatus);
    home_tile(r, 8, 126, COL_VIB_LIME, LV_SYMBOL_BARS, "TRACKING", UI_TRACKING, &homeTrkStatus);
    lv_obj_t* setStatus = nullptr;
    home_tile(r, 162, 126, COL_VIB_AMBER, LV_SYMBOL_SETTINGS, "SETTINGS", UI_SETTINGS, &setStatus);
    char fw[24];
    snprintf(fw, sizeof(fw), "Firmware %s", RPMLOPP_FIRMWARE_VERSION);
    lv_label_set_text(setStatus, fw);

    // Bottom status strip: dot + one line of REAL state, nothing invented.
    lv_obj_t* strip = lv_obj_create(r);
    lv_obj_set_pos(strip, 8, 210);
    lv_obj_set_size(strip, 304, 22);
    lv_obj_set_style_bg_color(strip, lv_color_hex(COL_PANEL_BG), 0);
    lv_obj_set_style_bg_opa(strip, LV_OPA_80, 0);
    lv_obj_set_style_radius(strip, 11, 0);
    lv_obj_set_style_border_width(strip, 0, 0);
    lv_obj_set_style_pad_all(strip, 0, 0);
    lv_obj_clear_flag(strip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(strip, LV_OBJ_FLAG_CLICKABLE);
    homeStripDot = lv_obj_create(strip);
    lv_obj_set_pos(homeStripDot, 10, 7);
    lv_obj_set_size(homeStripDot, 8, 8);
    lv_obj_set_style_radius(homeStripDot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(homeStripDot, lv_color_hex(COL_TEXT_SECOND), 0);
    lv_obj_set_style_bg_opa(homeStripDot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(homeStripDot, 0, 0);
    lv_obj_set_style_pad_all(homeStripDot, 0, 0);
    lv_obj_clear_flag(homeStripDot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(homeStripDot, LV_OBJ_FLAG_CLICKABLE);
    homeStripLbl = ui_label(strip, "Ready", &lv_font_montserrat_10, COL_TEXT);
    lv_obj_set_pos(homeStripLbl, 26, 5);
    lv_obj_set_width(homeStripLbl, 268);
    lv_label_set_long_mode(homeStripLbl, LV_LABEL_LONG_DOT);

    shade_add_handle(r);
    shade_attach_swipe(r);
}

static void refresh_home(void) {
    BikeState& b = g_bike_state;
    char buf[96];

    // Connection pill.
    lv_obj_set_style_bg_color(homePillDot,
        lv_color_hex(b.bleConnected ? COL_VIB_LIME : COL_TEXT_SECOND), 0);
    lv_label_set_text(homePillLbl, b.bleConnected ? "Phone connected" : "Not connected");
    lv_obj_set_style_text_color(homePillLbl,
        lv_color_hex(b.bleConnected ? COL_TEXT : COL_TEXT_SECOND), 0);

    // Tile status lines — real state or an honest empty message.
    if (b.navigating) {
        lv_label_set_text(homeNavStatus,
            b.instruction[0] ? b.instruction : b.streetName);
    } else {
        lv_label_set_text(homeNavStatus, "No active route");
    }
    lv_label_set_text(homeMusStatus,
        (b.playing && b.trackTitle[0]) ? b.trackTitle : "Nothing playing");
    if (b.trackingActive) {
        snprintf(buf, sizeof(buf), "%.2f km", (double)b.tripKm);
        lv_label_set_text(homeTrkStatus, buf);
    } else {
        lv_label_set_text(homeTrkStatus, "Not started");
    }

    // Bottom strip: live street + speed while navigating, else link state.
    if (b.navigating) {
        const char* road = b.streetName[0] ? b.streetName : "En route";
        snprintf(buf, sizeof(buf), "%s \xE2\x80\xA2 %d km/h", road, (int)(b.speedKmh + 0.5f));
        lv_label_set_text(homeStripLbl, buf);
        lv_obj_set_style_bg_color(homeStripDot, lv_color_hex(COL_VIB_LIME), 0);
    } else if (b.bleConnected) {
        lv_label_set_text(homeStripLbl, "Phone connected");
        lv_obj_set_style_bg_color(homeStripDot, lv_color_hex(COL_VIB_LIME), 0);
    } else {
        lv_label_set_text(homeStripLbl, "Ready");
        lv_obj_set_style_bg_color(homeStripDot, lv_color_hex(COL_TEXT_SECOND), 0);
    }
}

// White circular map-control button (36 px) for the NAV screen's left
// rail — recentre / zoom + / zoom -, Google-Maps control style.
static lv_obj_t* make_map_ctrl_btn(lv_obj_t* parent, const char* symbol, int x, int y,
                                   lv_event_cb_t cb, lv_obj_t** labelOut) {
    lv_obj_t* b = lv_btn_create(parent);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, 36, 36);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_shadow_width(b, 6, 0);
    lv_obj_set_style_shadow_color(b, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(b, LV_OPA_40, 0);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, symbol);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(COL_BLUE), 0);
    lv_obj_center(l);
    if (labelOut) *labelOut = l;
    if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
    return b;
}

// ============================================================ NAV ===========
// TomTom/Google-style full-bleed 2D nav (owner reference, 2026-10-29):
// the map fills all 320x240; over it sit a two-segment instruction banner
// (main maneuver/distance/street + a "Then" preview segment), a left rail
// of round map controls, a speed cluster at bottom-centre and a
// remaining/ETA pill bottom-right. Everything reads BikeState.
static MapView navMap;
static lv_obj_t *navArrow, *navDist, *navUnit, *navStreet, *navThenArrow,
                *navSpdLimit, *navSpdNow, *navRemLbl, *navStalePill,
                *navRecentre, *navRecentreLbl,
                *navMainSeg, *navThenSeg, *navSpdDisc, *navKmhLbl,
                *navRemPill, *navIdlePill;

static void nav_set_hidden(lv_obj_t* o, bool hidden) {
    if (hidden) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
}
static int navLastDist = -1, navLastSpeed = -1, navLastLimit = -1;

static void nav_recentre(void) {
    navMap.setLocked(true);
    g_bike_state.mapLocked = true;
    g_bike_state.zoomLevel = navMap.zoom();
}

static void nav_recentre_cb(lv_event_t* e) { (void)e; nav_recentre(); }
static void nav_zoom_in_cb(lv_event_t* e)  { (void)e; navMap.setZoom(navMap.zoom() + 1); }
static void nav_zoom_out_cb(lv_event_t* e) { (void)e; navMap.setZoom(navMap.zoom() - 1); }

static lv_obj_t* nav_banner_seg(lv_obj_t* parent, int x, int w) {
    lv_obj_t* s = lv_obj_create(parent);
    lv_obj_set_pos(s, x, 0);
    lv_obj_set_size(s, w, 46);
    lv_obj_set_style_bg_color(s, lv_color_hex(COL_ROUTE), 0);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s, 0, 0);
    lv_obj_set_style_radius(s, 0, 0);
    lv_obj_set_style_pad_all(s, 0, 0);
    lv_obj_clear_flag(s, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(s, LV_OBJ_FLAG_CLICKABLE);
    return s;
}

static void build_nav(void) {
    lv_obj_t* r = make_root(UI_NAV, COL_MAP_BG);
    navMap.create(r, 0, 0, 320, 240);           // full-bleed map

    // ---- two-segment banner: main (0..226) + 2 px gap + Then (228..320) --
    lv_obj_t* mainSeg = nav_banner_seg(r, 0, 226);
    navMainSeg = mainSeg;
    navArrow = ui_label(mainSeg, LV_SYMBOL_UP, &lv_font_montserrat_28, COL_TEXT);
    lv_obj_set_pos(navArrow, 10, 9);
    navDist = ui_label(mainSeg, "150", &lv_font_montserrat_32, COL_TEXT);
    lv_obj_set_pos(navDist, 46, 6);
    navUnit = ui_label(mainSeg, "m", &lv_font_montserrat_12, COL_TEXT);
    lv_obj_set_pos(navUnit, 100, 26);           // nudged next to number in refresh
    navStreet = ui_label(mainSeg, "", &lv_font_montserrat_12, COL_TEXT);
    lv_obj_set_pos(navStreet, 128, 8);          // nudged in refresh (dist width)
    lv_obj_set_width(navStreet, 94);
    lv_label_set_long_mode(navStreet, LV_LABEL_LONG_DOT);

    lv_obj_t* thenSeg = nav_banner_seg(r, 228, 92);
    navThenSeg = thenSeg;
    lv_obj_t* thenCap = ui_label(thenSeg, "Then", &lv_font_montserrat_10, COL_TEXT);
    lv_obj_set_pos(thenCap, 10, 3);
    navThenArrow = ui_label(thenSeg, LV_SYMBOL_RIGHT, &lv_font_montserrat_24, COL_TEXT);
    lv_obj_set_pos(navThenArrow, 12, 17);

    // ---- STALE pill (top-right under the banner; only while feed stale) --
    navStalePill = lv_obj_create(r);
    lv_obj_set_pos(navStalePill, 250, 52);
    lv_obj_set_size(navStalePill, 64, 20);
    lv_obj_set_style_bg_color(navStalePill, lv_color_hex(COL_RED), 0);
    lv_obj_set_style_bg_opa(navStalePill, LV_OPA_80, 0);
    lv_obj_set_style_radius(navStalePill, 10, 0);
    lv_obj_set_style_border_width(navStalePill, 0, 0);
    lv_obj_set_style_pad_all(navStalePill, 0, 0);
    lv_obj_clear_flag(navStalePill, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(navStalePill, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t* staleLbl = ui_label(navStalePill, "STALE", &lv_font_montserrat_10, COL_TEXT);
    lv_obj_center(staleLbl);
    lv_obj_add_flag(navStalePill, LV_OBJ_FLAG_HIDDEN);

    // ---- left rail: recentre / + / - (vertically centred) ----
    navRecentre = make_map_ctrl_btn(r, LV_SYMBOL_GPS,   6, 66,  nav_recentre_cb, &navRecentreLbl);
    make_map_ctrl_btn(r, LV_SYMBOL_PLUS,  6, 108, nav_zoom_in_cb, nullptr);
    make_map_ctrl_btn(r, LV_SYMBOL_MINUS, 6, 150, nav_zoom_out_cb, nullptr);

    // ---- speed cluster (bottom-centre): limit disc + current speed ----
    lv_obj_t* disc = lv_obj_create(r);
    lv_obj_set_pos(disc, 118, 190);
    lv_obj_set_size(disc, 44, 44);
    lv_obj_set_style_radius(disc, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(disc, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(disc, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(disc, lv_color_hex(COL_RED), 0);
    lv_obj_set_style_border_width(disc, 5, 0);
    lv_obj_set_style_pad_all(disc, 0, 0);
    lv_obj_clear_flag(disc, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(disc, LV_OBJ_FLAG_CLICKABLE);
    navSpdLimit = ui_label(disc, "50", &lv_font_montserrat_20, 0x111111);
    lv_obj_center(navSpdLimit);
    navSpdDisc = disc;
    navSpdNow = ui_label(r, "48", &lv_font_montserrat_32, 0x111111);
    lv_obj_set_pos(navSpdNow, 168, 194);
    navKmhLbl = ui_label(r, "km/h", &lv_font_montserrat_10, COL_MAP_LABEL);
    lv_obj_set_pos(navKmhLbl, 170, 224);

    // ---- remaining pill (bottom-right, dark translucent) ----
    lv_obj_t* pill = lv_obj_create(r);
    lv_obj_set_pos(pill, 220, 212);
    lv_obj_set_size(pill, 94, 21);
    lv_obj_set_style_bg_color(pill, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(pill, LV_OPA_60, 0);
    lv_obj_set_style_radius(pill, 10, 0);
    lv_obj_set_style_border_width(pill, 0, 0);
    lv_obj_set_style_pad_all(pill, 0, 0);
    lv_obj_clear_flag(pill, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(pill, LV_OBJ_FLAG_CLICKABLE);
    navRemLbl = ui_label(pill, "", &lv_font_montserrat_10, COL_TEXT);
    lv_obj_center(navRemLbl);
    navRemPill = pill;

    // ---- idle overlay (2.3.0): centred pill shown instead of all route
    // chrome while no route is active. The map underneath still draws.
    navIdlePill = lv_obj_create(r);
    lv_obj_set_pos(navIdlePill, 10, 104);
    lv_obj_set_size(navIdlePill, 300, 32);
    lv_obj_set_style_bg_color(navIdlePill, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(navIdlePill, LV_OPA_70, 0);
    lv_obj_set_style_radius(navIdlePill, 16, 0);
    lv_obj_set_style_border_width(navIdlePill, 0, 0);
    lv_obj_set_style_pad_all(navIdlePill, 0, 0);
    lv_obj_clear_flag(navIdlePill, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(navIdlePill, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t* idleLbl = ui_label(navIdlePill,
        "No active route \xE2\x80\x94 start navigation on your phone",
        &lv_font_montserrat_12, COL_TEXT);
    lv_obj_center(idleLbl);
    lv_obj_add_flag(navIdlePill, LV_OBJ_FLAG_HIDDEN);

    // Quick shade access (2.4.0): transparent drag strip over the top
    // edge (created last = topmost) + the visible pull pill. A downward
    // drag starting here opens the shade; map panning elsewhere is
    // untouched because the strip only ever sees presses inside y<22.
    shade_add_top_strip(r);
    shade_add_handle(r);
}

static void refresh_nav(void) {
    BikeState& b = g_bike_state;
    // Touch drags pan inside MapView; mirror its lock/zoom into BikeState
    // (the recentre button's dimming keys off mapLocked).
    b.mapLocked = navMap.locked();
    b.zoomLevel = navMap.zoom();

    // Idle (2.3.0): no route -> hide ALL route chrome (banner, speed
    // cluster, remaining pill, stale pill) and show the centred hint.
    // The map itself still draws beneath.
    bool idle = !b.navigating;
    nav_set_hidden(navIdlePill, !idle);
    nav_set_hidden(navMainSeg, idle);
    nav_set_hidden(navThenSeg, idle);
    nav_set_hidden(navSpdDisc, idle);
    nav_set_hidden(navSpdNow, idle);
    nav_set_hidden(navKmhLbl, idle);
    nav_set_hidden(navRemPill, idle);
    if (idle) {
        lv_obj_add_flag(navStalePill, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_opa(navRecentre, b.mapLocked ? LV_OPA_60 : LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(navRecentreLbl,
            lv_color_hex(b.mapLocked ? COL_TEXT_SECOND : COL_BLUE), 0);
        return;
    }

    // Banner main segment: maneuver arrow + distance + street.
    lv_label_set_text(navArrow, ui_turn_symbol(b.turnDir));
    int dist = (int)(b.distToTurn_m + 0.5f);
    if (dist != navLastDist) {
        navLastDist = dist;
        char buf[16];
        if (dist >= 1000) {
            snprintf(buf, sizeof(buf), "%.1f", dist / 1000.0);
            lv_label_set_text(navUnit, "km");
        } else {
            snprintf(buf, sizeof(buf), "%d", dist);
            lv_label_set_text(navUnit, "m");
        }
        lv_label_set_text(navDist, buf);
        // Sit the unit right after the number, the street right after that.
        lv_point_t sz;
        lv_txt_get_size(&sz, buf, &lv_font_montserrat_32, 0, 0, LV_COORD_MAX, 0);
        lv_obj_set_pos(navUnit, 48 + sz.x + 3, 26);
        int streetX = 48 + sz.x + 24;
        lv_obj_set_pos(navStreet, streetX, 8);
        lv_obj_set_width(navStreet, 226 - streetX - 4);
    }
    lv_label_set_text(navStreet, b.streetName);

    // "Then" segment: next maneuver preview (BikeState.nextTurnDir).
    lv_label_set_text(navThenArrow, ui_turn_symbol(b.nextTurnDir));

    // STALE pill: visible only while the live feed is stale.
    if (b.navStale) lv_obj_clear_flag(navStalePill, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(navStalePill, LV_OBJ_FLAG_HIDDEN);

    // Speed cluster: limit disc + current speed.
    if ((int)b.speedLimitKmh != navLastLimit) {
        navLastLimit = (int)b.speedLimitKmh;
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", (int)b.speedLimitKmh);
        lv_label_set_text(navSpdLimit, buf);
    }
    int spd = (int)(b.speedKmh + 0.5f);
    if (spd != navLastSpeed) {
        navLastSpeed = spd;
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", spd);
        lv_label_set_text(navSpdNow, buf);
    }

    // Remaining pill: "2.4 km • 3 min".
    char rbuf[32];
    snprintf(rbuf, sizeof(rbuf), "%.1f km \xE2\x80\xA2 %d min",
             (double)b.remainKm, b.etaMin);
    lv_label_set_text(navRemLbl, rbuf);

    // Recentre: dimmed while locked (following position), full while panned.
    lv_obj_set_style_bg_opa(navRecentre, b.mapLocked ? LV_OPA_60 : LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(navRecentreLbl,
        lv_color_hex(b.mapLocked ? COL_TEXT_SECOND : COL_BLUE), 0);
}

// ============================================================ MAPS ==========
static MapView mapsMap;
static lv_obj_t *mapsRecentre, *mapsZoomChip, *mapsZoomLbl;
static uint32_t mapsZoomChipUntil = 0;

static void maps_recentre_cb(lv_event_t* e) {
    (void)e;
    mapsMap.setLocked(true);
    g_bike_state.mapLocked = true;
    g_bike_state.zoomLevel = mapsMap.zoom();
}
static void maps_zoom_in_cb(lv_event_t* e) {
    (void)e;
    mapsMap.setZoom(mapsMap.zoom() + 1);
    mapsZoomChipUntil = lv_tick_get() + 1200;
}
static void maps_zoom_out_cb(lv_event_t* e) {
    (void)e;
    mapsMap.setZoom(mapsMap.zoom() - 1);
    mapsZoomChipUntil = lv_tick_get() + 1200;
}

static void build_maps(void) {
    lv_obj_t* r = make_root(UI_MAPS, COL_MAP_BG);
    mapsMap.create(r, 0, 0, 320, 240);

    lv_obj_t* chip = make_chip(r, 6, 6, 56);
    lv_obj_t* l = ui_label(chip, "MAPS", &lv_font_montserrat_10, COL_TEXT);
    lv_obj_center(l);
    mapsZoomChip = make_chip(r, 262, 6, 44);
    mapsZoomLbl = ui_label(mapsZoomChip, "Z16", &lv_font_montserrat_10, COL_TEXT);
    lv_obj_center(mapsZoomLbl);
    lv_obj_add_flag(mapsZoomChip, LV_OBJ_FLAG_HIDDEN);
    mapsRecentre = make_recentre_btn(r);
    lv_obj_add_flag(mapsRecentre, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(mapsRecentre, maps_recentre_cb, LV_EVENT_CLICKED, nullptr);
    make_zoom_btn(r, "+", 275, 96, maps_zoom_in_cb);
    make_zoom_btn(r, "-", 275, 134, maps_zoom_out_cb);
    make_home_btn(r, 6, 204);

    // Quick shade access (2.4.0): same top drag strip + pull pill as Nav.
    shade_add_top_strip(r);
    shade_add_handle(r);
}

static void refresh_maps(void) {
    g_bike_state.mapLocked = mapsMap.locked();
    g_bike_state.zoomLevel = mapsMap.zoom();
    if (mapsMap.locked()) lv_obj_add_flag(mapsRecentre, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(mapsRecentre, LV_OBJ_FLAG_HIDDEN);
    if ((int32_t)(mapsZoomChipUntil - lv_tick_get()) > 0) {
        char z[8];
        snprintf(z, sizeof(z), "Z%d", mapsMap.zoom());
        lv_label_set_text(mapsZoomLbl, z);
        lv_obj_clear_flag(mapsZoomChip, LV_OBJ_FLAG_HIDDEN);
    } else lv_obj_add_flag(mapsZoomChip, LV_OBJ_FLAG_HIDDEN);
}

// ============================================================ MUSIC =========
static lv_obj_t *musTitle, *musArtist, *musBar, *musTime, *musPlayLbl, *musHeadset;
static char musLastTitle[48] = "";

static void music_play_cb(lv_event_t* e) { (void)e; bike_control_toggle_play(); }
static void music_next_cb(lv_event_t* e) { (void)e; bike_control_next_track(); }
static void music_prev_cb(lv_event_t* e) { (void)e; bike_control_prev_track(); }

static void build_music(void) {
    lv_obj_t* r = make_root(UI_MUSIC, COL_BG_TOP);
    ui_apply_wallpaper(r);
    lv_obj_t* t = ui_label(r, "MUSIC", &lv_font_montserrat_14, COL_TEXT_SECOND);
    lv_obj_set_pos(t, 10, 10);

    // Full-bleed layout (2.4.0): big art block top-left; title / artist /
    // headset status to its right (that corner of the wallpaper is deep
    // blue, so white text reads cleanly); progress mid-screen; big
    // transport buttons filling the bottom band.
    lv_obj_t* art = lv_obj_create(r);
    lv_obj_set_pos(art, 12, 22);
    lv_obj_set_size(art, 88, 88);
    lv_obj_set_style_bg_color(art, lv_color_hex(COL_PANEL_CARD), 0);
    lv_obj_set_style_radius(art, 14, 0);
    lv_obj_set_style_border_width(art, 0, 0);
    lv_obj_set_style_pad_all(art, 0, 0);
    lv_obj_clear_flag(art, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* note = ui_label(art, LV_SYMBOL_AUDIO, &lv_font_montserrat_32, COL_VIB_PINK);
    lv_obj_center(note);

    musTitle = ui_label(r, "", &lv_font_montserrat_16, COL_TEXT);
    lv_obj_set_pos(musTitle, 112, 32);
    lv_obj_set_width(musTitle, 196);
    lv_label_set_long_mode(musTitle, LV_LABEL_LONG_DOT);
    musArtist = ui_label(r, "", &lv_font_montserrat_12, COL_TEXT_SECOND);
    lv_obj_set_pos(musArtist, 112, 60);
    lv_obj_set_width(musArtist, 196);
    lv_label_set_long_mode(musArtist, LV_LABEL_LONG_DOT);
    musHeadset = ui_label(r, "", &lv_font_montserrat_10, COL_TEXT_SECOND);
    lv_obj_set_pos(musHeadset, 112, 88);
    lv_obj_set_width(musHeadset, 196);

    musBar = lv_bar_create(r);
    lv_obj_set_pos(musBar, 16, 132);
    lv_obj_set_size(musBar, 288, 8);
    lv_bar_set_range(musBar, 0, 100);
    lv_obj_set_style_bg_color(musBar, lv_color_hex(COL_PANEL_DIV), 0);
    lv_obj_set_style_bg_color(musBar, lv_color_hex(COL_BLUE), LV_PART_INDICATOR);
    lv_obj_set_style_radius(musBar, 4, 0);
    lv_obj_set_style_radius(musBar, 4, LV_PART_INDICATOR);
    musTime = ui_label(r, "0:00 / 0:00", &lv_font_montserrat_10, COL_TEXT_SECOND);
    lv_obj_set_pos(musTime, 16, 148);

    lv_obj_t* prev = ui_button(r, LV_SYMBOL_PREV, &lv_font_montserrat_20);
    lv_obj_set_pos(prev, 12, 180);
    lv_obj_set_size(prev, 88, 48);
    lv_obj_add_event_cb(prev, music_prev_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* play = ui_button(r, LV_SYMBOL_PLAY, &lv_font_montserrat_24);
    lv_obj_set_pos(play, 112, 174);
    lv_obj_set_size(play, 96, 58);
    musPlayLbl = lv_obj_get_child(play, 0);
    lv_obj_add_event_cb(play, music_play_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* next = ui_button(r, LV_SYMBOL_NEXT, &lv_font_montserrat_20);
    lv_obj_set_pos(next, 220, 180);
    lv_obj_set_size(next, 88, 48);
    lv_obj_add_event_cb(next, music_next_cb, LV_EVENT_CLICKED, nullptr);

    make_home_btn(r, 282, 6);
    shade_add_handle(r);
    shade_attach_swipe(r);
}

static void refresh_music(void) {
    BikeState& b = g_bike_state;
    // Empty state: no track loaded (Demo off, nothing from the phone yet)
    // shows "Nothing playing" instead of a blank title and a frozen bar.
    const char* dispTitle = b.trackTitle[0] ? b.trackTitle : "Nothing playing";
    if (strcmp(musLastTitle, dispTitle) != 0) {
        strncpy(musLastTitle, dispTitle, sizeof(musLastTitle) - 1);
        musLastTitle[sizeof(musLastTitle) - 1] = '\0';
        lv_label_set_text(musTitle, dispTitle);
        lv_label_set_text(musArtist, b.trackTitle[0] ? b.trackArtist : "");
    }
    if (b.trackDurSec > 0) lv_bar_set_value(musBar, b.trackPosSec * 100 / b.trackDurSec, LV_ANIM_OFF);
    else lv_bar_set_value(musBar, 0, LV_ANIM_OFF);
    char tb[16], db[16], buf[40];
    ui_format_mmss(tb, b.trackPosSec);
    ui_format_mmss(db, b.trackDurSec);
    snprintf(buf, sizeof(buf), "%s / %s", tb, db);
    lv_label_set_text(musTime, buf);
    lv_label_set_text(musPlayLbl, b.playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
    lv_label_set_text(musHeadset, b.headsetConnected ? LV_SYMBOL_CALL " Rider Helmet" : LV_SYMBOL_CALL " No headset");
}

// NOTE (2.4.0): the Headset screen was REMOVED. Headphones pair with the
// phone, never with this device, so there is no headset UI — BikeState
// keeps its headset fields only for the Music screen's status line, and
// the bike_control_toggle_headset() seam stays for the provider layer.

// ============================================================ TRACKING ======
static lv_obj_t *trkSpeed, *trkTrip, *trkTime, *trkMaxAvg, *trkBtnLbl;

static void tracking_toggle_cb(lv_event_t* e) { (void)e; bike_control_toggle_tracking(); }
static void tracking_reset_cb(lv_event_t* e)  { (void)e; bike_control_reset_tracking(); }

static lv_obj_t* stat_card(lv_obj_t* parent, int x, int y, const char* caption, lv_obj_t** valueOut) {
    lv_obj_t* c = lv_obj_create(parent);
    lv_obj_set_pos(c, x, y);
    lv_obj_set_size(c, 146, 82);
    lv_obj_set_style_bg_color(c, lv_color_hex(COL_PANEL_CARD), 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(c, 10, 0);
    lv_obj_set_style_border_color(c, lv_color_hex(COL_PANEL_DIV), 0);
    lv_obj_set_style_border_width(c, 1, 0);
    lv_obj_set_style_pad_all(c, 0, 0);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* cap = ui_label(c, caption, &lv_font_montserrat_10, COL_TEXT_SECOND);
    lv_obj_set_pos(cap, 10, 8);
    *valueOut = ui_label(c, "--", &lv_font_montserrat_28, COL_TEXT);
    lv_obj_set_pos(*valueOut, 10, 32);
    return c;
}

static void build_tracking(void) {
    lv_obj_t* r = make_root(UI_TRACKING, COL_BG_TOP);
    ui_apply_wallpaper(r);
    // Full-bleed (2.4.0): 146x82 cards at margin 8, buttons fill the
    // bottom band with the Home button centred between them.
    stat_card(r, 8, 8, "SPEED KM/H", &trkSpeed);
    stat_card(r, 166, 8, "TRIP KM", &trkTrip);
    stat_card(r, 8, 98, "RIDE TIME", &trkTime);
    stat_card(r, 166, 98, "MAX / AVG KM/H", &trkMaxAvg);

    lv_obj_t* toggle = ui_button(r, LV_SYMBOL_PLAY " START", &lv_font_montserrat_14);
    lv_obj_set_pos(toggle, 8, 190);
    lv_obj_set_size(toggle, 132, 42);
    trkBtnLbl = lv_obj_get_child(toggle, 0);
    lv_obj_add_event_cb(toggle, tracking_toggle_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* reset = ui_button(r, LV_SYMBOL_REFRESH " RESET", &lv_font_montserrat_14);
    lv_obj_set_pos(reset, 180, 190);
    lv_obj_set_size(reset, 132, 42);
    lv_obj_add_event_cb(reset, tracking_reset_cb, LV_EVENT_CLICKED, nullptr);
    make_home_btn(r, 145, 196);
    shade_add_handle(r);
    shade_attach_swipe(r);
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
static lv_obj_t *setBright, *setVol, *setDemoSwitch, *setDemoNote;

static void bright_changed_cb(lv_event_t* e) {
    bike_control_set_brightness((int)lv_slider_get_value(lv_event_get_target(e)));
}
static void vol_changed_cb(lv_event_t* e) {
    bike_control_set_volume((int)lv_slider_get_value(lv_event_get_target(e)));
}
// The Demo mode switch is bound straight to BikeState.demoMode (spec'd
// exception to the bike_control_* seam — there is no driver action here,
// just the provider gate flag).
static void demo_changed_cb(lv_event_t* e) {
    g_bike_state.demoMode = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
}

static lv_obj_t* settings_slider(lv_obj_t* parent, int x, int y, int w, int vmin, int vmax, int val, lv_event_cb_t cb) {
    lv_obj_t* s = lv_slider_create(parent);
    lv_obj_set_pos(s, x, y);
    lv_obj_set_size(s, w, 10);
    lv_slider_set_range(s, vmin, vmax);
    lv_slider_set_value(s, val, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s, lv_color_hex(COL_PANEL_DIV), 0);
    lv_obj_set_style_bg_color(s, lv_color_hex(COL_BLUE), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s, lv_color_hex(COL_TEXT), LV_PART_KNOB);
    lv_obj_add_event_cb(s, cb, LV_EVENT_VALUE_CHANGED, nullptr);
    return s;
}

static void build_settings(void) {
    lv_obj_t* r = make_root(UI_SETTINGS, COL_BG_TOP);
    ui_apply_wallpaper(r);
    lv_obj_t* t = ui_label(r, "SETTINGS", &lv_font_montserrat_14, COL_TEXT_SECOND);
    lv_obj_set_pos(t, 10, 10);

    // Full-bleed (2.4.0): all rows live on one glass panel stretched over
    // the body of the screen, rows spaced to fill it edge to edge.
    ui_glass_panel(r, 8, 34, 304, 198);

    lv_obj_t* l1 = ui_label(r, "Brightness", &lv_font_montserrat_14, COL_TEXT);
    lv_obj_set_pos(l1, 18, 52);
    setBright = settings_slider(r, 126, 56, 178, 40, 100, g_bike_state.brightnessPct, bright_changed_cb);

    lv_obj_t* l2 = ui_label(r, "Volume", &lv_font_montserrat_14, COL_TEXT);
    lv_obj_set_pos(l2, 18, 96);
    setVol = settings_slider(r, 126, 100, 178, 0, 100, g_bike_state.volumePct, vol_changed_cb);

    lv_obj_t* l3 = ui_label(r, "Demo mode", &lv_font_montserrat_14, COL_TEXT);
    lv_obj_set_pos(l3, 18, 140);
    setDemoSwitch = lv_switch_create(r);
    lv_obj_set_pos(setDemoSwitch, 126, 138);
    lv_obj_set_size(setDemoSwitch, 46, 24);
    lv_obj_set_style_bg_color(setDemoSwitch, lv_color_hex(COL_PANEL_DIV), 0);
    lv_obj_set_style_bg_color(setDemoSwitch, lv_color_hex(COL_VIB_LIME),
                              (lv_style_selector_t)(LV_PART_INDICATOR | LV_STATE_CHECKED));
    lv_obj_set_style_bg_color(setDemoSwitch, lv_color_hex(COL_TEXT), LV_PART_KNOB);
    if (g_bike_state.demoMode) lv_obj_add_state(setDemoSwitch, LV_STATE_CHECKED);
    lv_obj_add_event_cb(setDemoSwitch, demo_changed_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    setDemoNote = ui_label(r, "", &lv_font_montserrat_10, COL_TEXT_DIM);
    lv_obj_set_pos(setDemoNote, 184, 147);

    lv_obj_t* l4 = ui_label(r, "Units", &lv_font_montserrat_14, COL_TEXT);
    lv_obj_set_pos(l4, 18, 178);
    lv_obj_t* l4v = ui_label(r, "km/h (fixed)", &lv_font_montserrat_12, COL_TEXT_DIM);
    lv_obj_set_pos(l4v, 126, 181);

    lv_obj_t* l5 = ui_label(r, "About", &lv_font_montserrat_14, COL_TEXT);
    lv_obj_set_pos(l5, 18, 206);
    char buf[64];
    snprintf(buf, sizeof(buf), "RPMLOOP FW %s \xC2\xB7 LVGL 8.3 \xC2\xB7 ST7789V", RPMLOPP_FIRMWARE_VERSION);
    lv_obj_t* l5v = ui_label(r, buf, &lv_font_montserrat_10, COL_TEXT_DIM);
    lv_obj_set_pos(l5v, 126, 209);
    make_home_btn(r, 282, 6);
    shade_add_handle(r);
    shade_attach_swipe(r);
}

static void refresh_settings(void) {
    // Keep the switch + its caption honest if the flag changed elsewhere.
    bool on = g_bike_state.demoMode;
    if (lv_obj_has_state(setDemoSwitch, LV_STATE_CHECKED) != on) {
        if (on) lv_obj_add_state(setDemoSwitch, LV_STATE_CHECKED);
        else lv_obj_clear_state(setDemoSwitch, LV_STATE_CHECKED);
    }
    lv_label_set_text(setDemoNote, on ? "Simulated data" : "Off \xE2\x80\x94 live data only");
}

// ============================================================ SHADE =======
// Swipe-down quick panel (2.4.0) — the universal way Home, reachable even
// mid-navigation (the owner had NO way back from the Nav screen). The
// panel lives on lv_layer_top() ABOVE the brightness dim overlay (both
// are created in ui_init, shade second), so it stays fully readable while
// the screen behind it is dimmed. Contents: big HOME button, close (x),
// a brightness slider bound to the SAME control as Settings
// (bike_control_set_brightness via bright_changed_cb), a Bluetooth
// status row from BikeState.bleConnected, and a Settings shortcut.
// Open: LV_DIR_BOTTOM gesture on plain screens; a transparent 320x22
// drag strip on Nav/Maps (their canvas owns drag = map pan). Close:
// swipe-up gesture on the panel, the (x) button, or any shortcut.
static lv_obj_t* s_shade = nullptr;
static lv_obj_t* s_shadeSlider = nullptr;
static lv_obj_t* s_shadeBtLbl = nullptr;
static bool      s_shadeOpen = false;
static int       s_shadeLastBle = -1;

static const int SHADE_H = 192;

static void shade_home_cb(lv_event_t* e)    { (void)e; shade_close(); ui_show(UI_HOME); }
static void shade_close_cb(lv_event_t* e)   { (void)e; shade_close(); }
static void shade_settings_cb(lv_event_t* e){ (void)e; shade_close(); ui_show(UI_SETTINGS); }

static void shade_anim_to(int32_t target, uint32_t timeMs, lv_anim_path_cb_t path) {
    if (!s_shade) return;
    lv_anim_del(s_shade, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_shade);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_values(&a, lv_obj_get_y(s_shade), target);
    lv_anim_set_time(&a, timeMs);
    lv_anim_set_path_cb(&a, path);
    lv_anim_start(&a);
}

static void shade_open(void) {
    if (s_shadeOpen || !s_shade) return;
    s_shadeOpen = true;
    shade_refresh();
    shade_anim_to(0, 260, lv_anim_path_ease_out);
}

static void shade_close(void) {
    if (!s_shadeOpen || !s_shade) return;
    s_shadeOpen = false;
    shade_anim_to(-SHADE_H, 220, lv_anim_path_ease_in);
}

static void shade_refresh(void) {
    if (!s_shade) return;
    int ble = g_bike_state.bleConnected ? 1 : 0;
    if (ble != s_shadeLastBle) {
        s_shadeLastBle = ble;
        lv_label_set_text(s_shadeBtLbl, ble ? LV_SYMBOL_BLUETOOTH "  Connected"
                                            : LV_SYMBOL_BLUETOOTH "  Not connected");
        lv_obj_set_style_text_color(s_shadeBtLbl,
            lv_color_hex(ble ? COL_VIB_LIME : COL_TEXT_SECOND), 0);
    }
    // Keep the shade slider in step with Settings' slider (same control);
    // never fight the user's finger mid-drag.
    if (s_shadeSlider && !lv_obj_has_state(s_shadeSlider, LV_STATE_PRESSED))
        lv_slider_set_value(s_shadeSlider, g_bike_state.brightnessPct, LV_ANIM_OFF);
}

static void shade_panel_gesture_cb(lv_event_t* e) {
    (void)e;
    if (lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_TOP) shade_close();
}

static void shade_init(void) {
    s_shade = lv_obj_create(lv_layer_top());
    lv_obj_set_pos(s_shade, 0, -SHADE_H);
    lv_obj_set_size(s_shade, TFT_LCD_W, SHADE_H);
    lv_obj_set_style_bg_color(s_shade, lv_color_hex(COL_PANEL_BG), 0);
    lv_obj_set_style_bg_opa(s_shade, LV_OPA_90, 0);
    lv_obj_set_style_radius(s_shade, 16, 0);
    lv_obj_set_style_border_width(s_shade, 0, 0);
    lv_obj_set_style_pad_all(s_shade, 0, 0);
    lv_obj_clear_flag(s_shade, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_shade, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_shade, shade_panel_gesture_cb, LV_EVENT_GESTURE, nullptr);

    lv_obj_t* home = lv_btn_create(s_shade);
    lv_obj_set_pos(home, 12, 12);
    lv_obj_set_size(home, 196, 52);
    lv_obj_set_style_bg_color(home, lv_color_hex(COL_VIB_LIME), 0);
    lv_obj_set_style_bg_opa(home, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(home, 12, 0);
    lv_obj_set_style_shadow_width(home, 0, 0);
    lv_obj_t* homeLbl = ui_label(home, LV_SYMBOL_HOME "  HOME", &lv_font_montserrat_20, COL_CARD_TEXT);
    lv_obj_center(homeLbl);
    lv_obj_add_event_cb(home, shade_home_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* x = lv_btn_create(s_shade);
    lv_obj_set_pos(x, 220, 12);
    lv_obj_set_size(x, 88, 52);
    lv_obj_set_style_bg_color(x, lv_color_hex(COL_PANEL_CARD), 0);
    lv_obj_set_style_bg_opa(x, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(x, 12, 0);
    lv_obj_set_style_shadow_width(x, 0, 0);
    lv_obj_t* xLbl = ui_label(x, LV_SYMBOL_CLOSE, &lv_font_montserrat_20, COL_TEXT);
    lv_obj_center(xLbl);
    lv_obj_add_event_cb(x, shade_close_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* bl = ui_label(s_shade, "Brightness", &lv_font_montserrat_14, COL_TEXT);
    lv_obj_set_pos(bl, 12, 82);
    s_shadeSlider = lv_slider_create(s_shade);
    lv_obj_set_pos(s_shadeSlider, 112, 86);
    lv_obj_set_size(s_shadeSlider, 196, 10);
    lv_slider_set_range(s_shadeSlider, 40, 100);
    lv_slider_set_value(s_shadeSlider, g_bike_state.brightnessPct, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_shadeSlider, lv_color_hex(COL_PANEL_DIV), 0);
    lv_obj_set_style_bg_color(s_shadeSlider, lv_color_hex(COL_BLUE), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_shadeSlider, lv_color_hex(COL_TEXT), LV_PART_KNOB);
    lv_obj_add_event_cb(s_shadeSlider, bright_changed_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    s_shadeBtLbl = ui_label(s_shade, LV_SYMBOL_BLUETOOTH "  Not connected",
                            &lv_font_montserrat_14, COL_TEXT_SECOND);
    lv_obj_set_pos(s_shadeBtLbl, 12, 118);

    lv_obj_t* set = lv_btn_create(s_shade);
    lv_obj_set_pos(set, 12, 146);
    lv_obj_set_size(set, 296, 36);
    lv_obj_set_style_bg_color(set, lv_color_hex(COL_PANEL_CARD), 0);
    lv_obj_set_style_bg_opa(set, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(set, 10, 0);
    lv_obj_set_style_shadow_width(set, 0, 0);
    lv_obj_t* setLbl = ui_label(set, LV_SYMBOL_SETTINGS "  SETTINGS", &lv_font_montserrat_14, COL_TEXT);
    lv_obj_center(setLbl);
    lv_obj_add_event_cb(set, shade_settings_cb, LV_EVENT_CLICKED, nullptr);

    // Swipe-up must reach the panel handler even when the drag starts on
    // a child (button / slider), so every child bubbles gestures up.
    uint32_t n = lv_obj_get_child_cnt(s_shade);
    for (uint32_t i = 0; i < n; i++)
        lv_obj_add_flag(lv_obj_get_child(s_shade, i), LV_OBJ_FLAG_GESTURE_BUBBLE);
}

// --- per-screen attach points -------------------------------------------
static int  s_pullY0 = 0;
static bool s_pullActive = false;

static void shade_pull_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) { shade_open(); return; }
    lv_indev_t* indev = lv_event_get_indev(e);
    if (!indev) return;
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    if (code == LV_EVENT_PRESSED) {
        s_pullY0 = p.y; s_pullActive = true;
    } else if (code == LV_EVENT_PRESSING && s_pullActive) {
        if (p.y - s_pullY0 > 28) { s_pullActive = false; shade_open(); }
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        if (s_pullActive && p.y - s_pullY0 > 28) shade_open();
        s_pullActive = false;
    }
}

static void shade_add_handle(lv_obj_t* root) {
    // Pull target (2.4.2): a real clickable zone (64x20) centred on the
    // top edge — a plain TAP on it opens the shade, and so does a short
    // downward drag starting there. The visible pill sits inside it.
    // (2.4.0's tiny non-clickable pill + LV_EVENT_GESTURE never fired:
    // lv_indev zeroes the gesture sum whenever the per-read velocity
    // dips below the minimum, so a slow resistive-touch drag never
    // accumulates the 50 px a gesture needs.)
    lv_obj_t* zone = lv_obj_create(root);
    lv_obj_set_pos(zone, (TFT_LCD_W - 64) / 2, 0);
    lv_obj_set_size(zone, 64, 20);
    lv_obj_set_style_bg_opa(zone, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(zone, 0, 0);
    lv_obj_set_style_radius(zone, 0, 0);
    lv_obj_set_style_pad_all(zone, 0, 0);
    lv_obj_clear_flag(zone, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(zone, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(zone, shade_pull_cb, LV_EVENT_ALL, nullptr);

    lv_obj_t* h = lv_obj_create(zone);
    lv_obj_set_pos(h, (64 - 28) / 2, 4);
    lv_obj_set_size(h, 28, 5);
    lv_obj_set_style_radius(h, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(h, lv_color_hex(COL_TEXT), 0);
    lv_obj_set_style_bg_opa(h, LV_OPA_80, 0);
    lv_obj_set_style_border_width(h, 0, 0);
    lv_obj_set_style_pad_all(h, 0, 0);
    lv_obj_clear_flag(h, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(h, LV_OBJ_FLAG_CLICKABLE);
}

static void shade_root_gesture_cb(lv_event_t* e) {
    (void)e;
    if (lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_BOTTOM) shade_open();
}

static void shade_attach_swipe(lv_obj_t* root) {
    lv_obj_add_event_cb(root, shade_root_gesture_cb, LV_EVENT_GESTURE, nullptr);
    // Gestures start on the pressed child; bubble them up to the root.
    uint32_t n = lv_obj_get_child_cnt(root);
    for (uint32_t i = 0; i < n; i++)
        lv_obj_add_flag(lv_obj_get_child(root, i), LV_OBJ_FLAG_GESTURE_BUBBLE);
}

static int  s_stripY0 = 0;
static bool s_stripActive = false, s_stripFired = false;

static void shade_strip_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t* indev = lv_event_get_indev(e);
    if (!indev) return;
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    if (code == LV_EVENT_PRESSED) {
        s_stripY0 = p.y; s_stripActive = true; s_stripFired = false;
    } else if (code == LV_EVENT_PRESSING && s_stripActive && !s_stripFired) {
        if (p.y - s_stripY0 > 28) { s_stripFired = true; shade_open(); }
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        // 2.4.2: also fire on release — if PRESSING delivery is flaky,
        // the release point still proves the downward drag happened.
        if (s_stripActive && !s_stripFired && p.y - s_stripY0 > 28) shade_open();
        s_stripActive = false;
    }
}

static void shade_add_top_strip(lv_obj_t* root) {
    lv_obj_t* strip = lv_obj_create(root);
    lv_obj_set_pos(strip, 0, 0);
    lv_obj_set_size(strip, TFT_LCD_W, 22);
    lv_obj_set_style_bg_opa(strip, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(strip, 0, 0);
    lv_obj_set_style_radius(strip, 0, 0);
    lv_obj_set_style_pad_all(strip, 0, 0);
    lv_obj_clear_flag(strip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(strip, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(strip, shade_strip_cb, LV_EVENT_ALL, nullptr);
}

// ============================================================ MANAGER =======
static void build_screen(UiScreen id) {
    if (S[id].root) return;
    switch (id) {
        case UI_BOOT:     build_boot();     S[id].refresh = refresh_boot; break;
        case UI_CONNECT:  build_connect();  S[id].refresh = refresh_connect; break;
        case UI_HOME:     build_home();     S[id].refresh = refresh_home; break;
        case UI_NAV:      build_nav();      S[id].refresh = refresh_nav; break;
        case UI_MAPS:     build_maps();     S[id].refresh = refresh_maps; break;
        case UI_MUSIC:    build_music();    S[id].refresh = refresh_music; break;
        case UI_TRACKING: build_tracking(); S[id].refresh = refresh_tracking; break;
        case UI_SETTINGS: build_settings(); S[id].refresh = refresh_settings; break;
        default: break;
    }
}

static void apply_dim_overlay(void) {
    if (!s_dimOverlay) return;
    int dimPct = 100 - g_bike_state.brightnessPct;      // brightness 40..100 -> dim 60..0 %
    lv_obj_set_style_bg_opa(s_dimOverlay, (lv_opa_t)(dimPct * 255 / 100), 0);
}

static bool s_prevBle = false;   // disconnect-rule edge detector

static void refresh_timer_cb(lv_timer_t* t) {
    (void)t;
    // Disconnect rule (2.3.0): if the phone link drops anywhere other
    // than Splash/Connect, go back to Connect with the lost-link note.
    bool ble = g_bike_state.bleConnected;
    if (s_prevBle && !ble && s_cur != UI_BOOT && s_cur != UI_CONNECT) {
        s_connLost = true;
        ui_show(UI_CONNECT);
    }
    s_prevBle = ble;
    if (S[s_cur].refresh) S[s_cur].refresh();
    apply_dim_overlay();
    shade_refresh();
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

    // Quick shade (2.4.0): created AFTER the dim overlay so it sits on
    // top of it on lv_layer_top() and stays readable at any brightness.
    shade_init();

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
    if (id == UI_CONNECT) connect_on_show();
    if (S[id].refresh) S[id].refresh();
}

UiScreen ui_current(void) { return s_cur; }

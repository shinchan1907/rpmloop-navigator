// RPMLOOP Navigator UI — All Screens Implementation (v3.0 Flagship Redesign)
// Modern Light Slate & Vibrant Accents UI for 320x240 ST7789V Landscape Display
// Features: Animated Instagram Splash (Bold Montserrat 34), Paged Home Dashboard (2 cards/page, button navigation),
// Navigation, Maps, Music, Tracking, Settings, Bike Docs (Fullscreen 1:1 view)

#include "ui_screens.h"
#include "ble_data.h"
#include "ble_server.h"
#include "tile_reader.h"
#include "map_renderer.h"
#include "lvgl_port.h"
#include <Arduino.h>
#include <cstdio>
#include <cmath>

// Custom high-impact bold font for wordmark and titles
extern const lv_font_t font_montserrat_bold_34;

// ================================================================
// 1. VISUAL THEME — MODERN LIGHT DASHBOARD PALETTE
// ================================================================
#define COL_BG_TOP       lv_color_hex(0xF1F5F9)  // Slate 100 - Clean sleek off-white top
#define COL_BG_BOT       lv_color_hex(0xE2E8F0)  // Slate 200 - Rich soft vertical gradient bottom
#define COL_CARD_BG      lv_color_hex(0xFFFFFF)  // Pure White Card Fill
#define COL_CARD_BORDER  lv_color_hex(0xCBD5E1)  // Slate 300 - Crisp 1px Outline
#define COL_CARD_PRESSED lv_color_hex(0xF8FAFC)  // Slate 50 - Pressed State Fill
#define COL_CARD_INNER   lv_color_hex(0xF1F5F9)  // Sub-element Fill

// Typography Colors (High contrast, razor sharp readability in outdoor daylight)
#define COL_TEXT_PRI     lv_color_hex(0x0F172A)  // Slate 900 - Deep Charcoal Black
#define COL_TEXT_SEC     lv_color_hex(0x475569)  // Slate 600 - Medium Slate Gray
#define COL_TEXT_MUTED   lv_color_hex(0x94A3B8)  // Slate 400 - Clean Muted Slate

// ONE Accent Color per Feature (Used for icon badge + accent bar)
#define ACCENT_NAV       lv_color_hex(0x0284C7)  // Electric Sky Blue (Navigate)
#define ACCENT_MAPS      lv_color_hex(0x059669)  // Vivid Emerald Green (Offline Map)
#define ACCENT_MUSIC     lv_color_hex(0xE11D48)  // Vibrant Raspberry Rose (Music)
#define ACCENT_TRACK     lv_color_hex(0x65A30D)  // Energetic Bright Lime (Tracking)
#define ACCENT_DOCS      lv_color_hex(0xD97706)  // Warm Golden Amber (Bike Docs)
#define ACCENT_SETTINGS  lv_color_hex(0x7C3AED)  // Royal Purple (Settings)

// Helper: Apply modern clean gradient to a screen
static void apply_screen_theme(lv_obj_t* scr) {
    lv_obj_set_style_bg_color(scr, COL_BG_TOP, 0);
    lv_obj_set_style_bg_grad_color(scr, COL_BG_BOT, 0);
    lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
}

// ---- Forward declarations ----
static void create_splash_screen();
static void create_connect_screen();
static void create_home_screen();
static void create_nav_screen();
static void create_maps_screen();
static void create_music_screen();
static void create_tracking_screen();
static void create_settings_screen();
static void create_docs_screen();
static void create_shade();
static void create_brightness_overlay();

static void update_connect_screen();
static void update_home_screen();
static void update_nav_screen();
static void update_maps_screen();
static void update_music_screen();
static void update_tracking_screen();
static void update_docs_screen();

// ---- Screen objects ----
static lv_obj_t* scr_splash    = nullptr;
static lv_obj_t* scr_connect   = nullptr;
static lv_obj_t* scr_home      = nullptr;
static lv_obj_t* scr_nav       = nullptr;
static lv_obj_t* scr_maps      = nullptr;
static lv_obj_t* scr_music     = nullptr;
static lv_obj_t* scr_tracking  = nullptr;
static lv_obj_t* scr_settings  = nullptr;
static lv_obj_t* scr_docs      = nullptr;

// ---- Quick shade ----
static lv_obj_t* shade_panel   = nullptr;
static bool shade_is_visible   = false;

// ---- Brightness overlay ----
static lv_obj_t* brightness_overlay = nullptr;
static uint8_t   brightness_level   = 255;

// ---- Current screen ----
static Screen current_screen = Screen::SPLASH;

// ---- Home Paged Navigation State ----
static int home_cur_page = 0;
static lv_obj_t* home_pages_cont = nullptr;
static lv_obj_t* home_dot_objs[3] = {nullptr};
static lv_obj_t* home_prev_btn = nullptr;
static lv_obj_t* home_next_btn = nullptr;
static lv_obj_t* home_bt_status = nullptr;
static lv_obj_t* home_speed_label = nullptr;
static lv_obj_t* home_sd_label = nullptr;

// Dynamic labels for sub-screens
static lv_obj_t* connect_status_label = nullptr;
static lv_obj_t* connect_prompt_label = nullptr;
static lv_obj_t* connect_spinner = nullptr;

// Nav screen
static lv_obj_t* nav_map_container = nullptr;
static lv_obj_t* nav_maneuver_icon = nullptr;
static lv_obj_t* nav_distance_label = nullptr;
static lv_obj_t* nav_street_label = nullptr;
static lv_obj_t* nav_speed_label = nullptr;
static lv_obj_t* nav_eta_label = nullptr;
static lv_obj_t* nav_remaining_label = nullptr;
static lv_obj_t* nav_no_data_label = nullptr;

// Maps screen
static lv_obj_t* maps_container = nullptr;
static lv_obj_t* maps_zoom_label = nullptr;
static lv_obj_t* maps_attribution_label = nullptr;
static lv_obj_t* maps_no_sd_label = nullptr;

// Music screen
static lv_obj_t* music_title_label = nullptr;
static lv_obj_t* music_artist_label = nullptr;
static lv_obj_t* music_album_label = nullptr;
static lv_obj_t* music_play_btn = nullptr;
static lv_obj_t* music_progress_bar = nullptr;
static lv_obj_t* music_time_label = nullptr;

// Tracking screen
static lv_obj_t* track_distance_label = nullptr;
static lv_obj_t* track_time_label = nullptr;
static lv_obj_t* track_avg_speed_label = nullptr;
static lv_obj_t* track_max_speed_label = nullptr;
static lv_obj_t* track_cur_speed_label = nullptr;
static lv_obj_t* track_no_data_label = nullptr;

// Settings
static lv_obj_t* settings_brightness_slider = nullptr;
static lv_obj_t* settings_mem_label = nullptr;
static lv_obj_t* settings_sd_label = nullptr;

// Bike Documents screen
enum class DocType : uint8_t { RC, DL, AADHAAR };
static DocType current_doc = DocType::RC;
static void show_doc(DocType doc);
static void docs_exit_fullscreen();
static void docs_enter_fullscreen();

static lv_obj_t* docs_tab_rc = nullptr;
static lv_obj_t* docs_tab_dl = nullptr;
static lv_obj_t* docs_tab_aadhar = nullptr;
static lv_obj_t* docs_card = nullptr;
static lv_obj_t* docs_title_lbl = nullptr;
static lv_obj_t* docs_sub_lbl = nullptr;
static lv_obj_t* docs_file_lbl = nullptr;
static lv_obj_t* docs_info_lbl = nullptr;
static lv_obj_t* docs_img_obj = nullptr;
static lv_obj_t* docs_hint_lbl = nullptr;
static lv_obj_t* docs_flip_btn = nullptr;
static lv_obj_t* docs_flip_lbl = nullptr;
static bool rc_show_back = false;
static bool aadhar_show_back = false;
static bool docs_is_fullscreen = false;

static const int DOC_IMG_W = 320;
static const int DOC_IMG_H = 240;
static uint16_t* docs_img_buf = nullptr;
static lv_img_dsc_t docs_img_dsc;

// Fullscreen overlay objects (Native 1:1 ST7789 display)
static lv_obj_t* docs_fs_overlay = nullptr;
static lv_obj_t* docs_fs_img = nullptr;
static lv_obj_t* docs_fs_title_lbl = nullptr;
static lv_obj_t* docs_fs_flip_btn = nullptr;
static lv_obj_t* docs_fs_flip_lbl = nullptr;
static lv_obj_t* docs_fs_back_btn = nullptr;

// Splash resources
static lv_color_t* splash_canvas_buf = nullptr;
static lv_obj_t* splash_canvas = nullptr;
static lv_timer_t* splash_anim_timer = nullptr;
static lv_timer_t* splash_timer = nullptr;
static uint16_t splash_offset = 0;
static uint16_t splash_lut[768];

// Shade elements
static lv_obj_t* shade_bt_label = nullptr;
static lv_obj_t* shade_brightness_slider = nullptr;

// ================================================================
// HELPER: FORMATTERS
// ================================================================
static void formatDistance(char* buf, size_t len, uint32_t meters) {
    if (meters >= 1000) {
        snprintf(buf, len, "%.1f km", meters / 1000.0f);
    } else {
        snprintf(buf, len, "%d m", (int)meters);
    }
}

static void formatDuration(char* buf, size_t len, uint32_t seconds) {
    uint32_t h = seconds / 3600;
    uint32_t m = (seconds % 3600) / 60;
    uint32_t s = seconds % 60;
    if (h > 0) {
        snprintf(buf, len, "%dh %02dm", (int)h, (int)m);
    } else {
        snprintf(buf, len, "%02d:%02d", (int)m, (int)s);
    }
}

// Convert 24-bit RGB to 16-bit RGB565 (with SPI byte-swap for ST7789)
static inline uint16_t make_swap565(uint8_t r, uint8_t g, uint8_t b) {
    uint16_t c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    return (uint16_t)((c >> 8) | (c << 8));
}

// ================================================================
// 2. SPLASH SCREEN (Instagram 45-degree Gradient + Staggered Rise)
// ================================================================
static void init_splash_lut() {
    // 5 Instagram stops: #FEDA75, #FA7E1E, #D62976, #962FBF, #4F5BD5
    const uint8_t stops[5][3] = {
        { 0xFE, 0xDA, 0x75 }, // Stop 0: Warm Yellow
        { 0xFA, 0x7E, 0x1E }, // Stop 1: Vivid Orange
        { 0xD6, 0x29, 0x76 }, // Stop 2: Deep Pink
        { 0x96, 0x2F, 0xBF }, // Stop 3: Purple
        { 0x4F, 0x5B, 0xD5 }  // Stop 4: Royal Blue
    };

    const int SEG_LEN = 768 / 5; // ~153 steps per segment
    int lut_idx = 0;

    for (int s = 0; s < 5; s++) {
        int next_s = (s + 1) % 5;
        int steps = (s == 4) ? (768 - lut_idx) : SEG_LEN;
        for (int i = 0; i < steps; i++) {
            float t = (float)i / (float)steps;
            uint8_t r = (uint8_t)(stops[s][0] + (stops[next_s][0] - stops[s][0]) * t);
            uint8_t g = (uint8_t)(stops[s][1] + (stops[next_s][1] - stops[s][1]) * t);
            uint8_t b = (uint8_t)(stops[s][2] + (stops[next_s][2] - stops[s][2]) * t);
            splash_lut[lut_idx++] = make_swap565(r, g, b);
        }
    }
}

static void splash_render_frame(uint16_t offset) {
    if (!splash_canvas_buf) return;
    for (int y = 0; y < 240; y++) {
        uint16_t* row = (uint16_t*)&splash_canvas_buf[y * 320];
        int base = y + offset;
        for (int x = 0; x < 320; x++) {
            row[x] = splash_lut[(base + x) % 768];
        }
    }
    if (splash_canvas) lv_obj_invalidate(splash_canvas);
}

static void splash_anim_timer_cb(lv_timer_t* t) {
    splash_offset = (splash_offset + 3) % 768;
    splash_render_frame(splash_offset);
}

static void splash_timer_cb(lv_timer_t* timer) {
    if (splash_anim_timer) {
        lv_timer_del(splash_anim_timer);
        splash_anim_timer = nullptr;
    }
    lv_timer_del(timer);
    splash_timer = nullptr;

    // Smooth crossfade directly to Home
    ui_navigate(Screen::HOME);
}

static void create_splash_screen() {
    scr_splash = lv_obj_create(nullptr);
    lv_obj_remove_style_all(scr_splash);
    lv_obj_set_size(scr_splash, 320, 240);

    init_splash_lut();

    // Allocate 320x240 canvas buffer in PSRAM
    splash_canvas_buf = (lv_color_t*)ps_malloc(320 * 240 * sizeof(lv_color_t));
    if (!splash_canvas_buf) {
        splash_canvas_buf = (lv_color_t*)malloc(320 * 240 * sizeof(lv_color_t));
    }

    if (splash_canvas_buf) {
        splash_render_frame(0);
        splash_canvas = lv_canvas_create(scr_splash);
        lv_canvas_set_buffer(splash_canvas, splash_canvas_buf, 320, 240, LV_IMG_CF_TRUE_COLOR);
        lv_obj_set_size(splash_canvas, 320, 240);
        lv_obj_set_pos(splash_canvas, 0, 0);

        // Low-overhead 50ms timer for slow smooth pan
        splash_anim_timer = lv_timer_create(splash_anim_timer_cb, 50, nullptr);
    } else {
        // Fallback to static background gradient
        apply_screen_theme(scr_splash);
    }

    // Wordmark Container (Holds 7 staggered rising letters: R-P-M-L-O-O-P)
    lv_obj_t* wm_cont = lv_obj_create(scr_splash);
    lv_obj_remove_style_all(wm_cont);
    lv_obj_set_size(wm_cont, 280, 52);
    lv_obj_align(wm_cont, LV_ALIGN_CENTER, 0, -22);
    lv_obj_set_flex_flow(wm_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(wm_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(wm_cont, 2, 0);

    const char letters[] = "RPMLOOP";
    for (int i = 0; i < 7; i++) {
        lv_obj_t* l = lv_label_create(wm_cont);
        char s[2] = { letters[i], '\0' };
        lv_label_set_text(l, s);
        lv_obj_set_style_text_font(l, &font_montserrat_bold_34, 0);
        lv_obj_set_style_text_color(l, lv_color_white(), 0);

        // Initial hidden state
        lv_obj_set_style_opa(l, LV_OPA_TRANSP, 0);
        lv_obj_set_style_translate_y(l, 18, 0);

        // Staggered rise animation (60ms apart)
        lv_anim_t a_y, a_opa;
        lv_anim_init(&a_y);
        lv_anim_set_var(&a_y, l);
        lv_anim_set_values(&a_y, 18, 0);
        lv_anim_set_exec_cb(&a_y, [](void* obj, int32_t v) {
            lv_obj_set_style_translate_y((lv_obj_t*)obj, v, 0);
        });
        lv_anim_set_time(&a_y, 320);
        lv_anim_set_delay(&a_y, i * 60);
        lv_anim_set_path_cb(&a_y, lv_anim_path_ease_out);
        lv_anim_start(&a_y);

        // Staggered fade-in animation
        lv_anim_init(&a_opa);
        lv_anim_set_var(&a_opa, l);
        lv_anim_set_values(&a_opa, LV_OPA_TRANSP, LV_OPA_COVER);
        lv_anim_set_exec_cb(&a_opa, [](void* obj, int32_t v) {
            lv_obj_set_style_opa((lv_obj_t*)obj, v, 0);
        });
        lv_anim_set_time(&a_opa, 320);
        lv_anim_set_delay(&a_opa, i * 60);
        lv_anim_set_path_cb(&a_opa, lv_anim_path_ease_out);
        lv_anim_start(&a_opa);
    }

    // Subtitle: NAVIGATOR in letterspaced small caps (fades in last)
    lv_obj_t* nav_sub = lv_label_create(scr_splash);
    lv_label_set_text(nav_sub, "N A V I G A T O R");
    lv_obj_set_style_text_font(nav_sub, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(nav_sub, lv_color_white(), 0);
    lv_obj_align(nav_sub, LV_ALIGN_CENTER, 0, 18);
    lv_obj_set_style_opa(nav_sub, LV_OPA_TRANSP, 0);

    lv_anim_t a_sub;
    lv_anim_init(&a_sub);
    lv_anim_set_var(&a_sub, nav_sub);
    lv_anim_set_values(&a_sub, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_exec_cb(&a_sub, [](void* obj, int32_t v) {
        lv_obj_set_style_opa((lv_obj_t*)obj, v, 0);
    });
    lv_anim_set_time(&a_sub, 450);
    lv_anim_set_delay(&a_sub, 560);
    lv_anim_set_path_cb(&a_sub, lv_anim_path_ease_out);
    lv_anim_start(&a_sub);

    // Bottom Progress Shimmer Line
    lv_obj_t* track = lv_obj_create(scr_splash);
    lv_obj_remove_style_all(track);
    lv_obj_set_size(track, 150, 2);
    lv_obj_align(track, LV_ALIGN_BOTTOM_MID, 0, -22);
    lv_obj_set_style_bg_color(track, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(track, LV_OPA_30, 0);
    lv_obj_set_style_radius(track, 1, 0);

    lv_obj_t* shimmer = lv_obj_create(track);
    lv_obj_remove_style_all(shimmer);
    lv_obj_set_size(shimmer, 0, 2);
    lv_obj_set_pos(shimmer, 0, 0);
    lv_obj_set_style_bg_color(shimmer, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(shimmer, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(shimmer, 1, 0);

    lv_anim_t a_shim;
    lv_anim_init(&a_shim);
    lv_anim_set_var(&a_shim, shimmer);
    lv_anim_set_values(&a_shim, 0, 150);
    lv_anim_set_exec_cb(&a_shim, [](void* obj, int32_t v) {
        lv_obj_set_width((lv_obj_t*)obj, v);
    });
    lv_anim_set_time(&a_shim, 2200);
    lv_anim_set_path_cb(&a_shim, lv_anim_path_ease_out);
    lv_anim_start(&a_shim);

    // Total splash ~2.4s, then transition into Home
    splash_timer = lv_timer_create(splash_timer_cb, 2400, nullptr);
    lv_timer_set_repeat_count(splash_timer, 1);
}

// ================================================================
// 3. HOME SCREEN (Paged 2 Large Cards/Page, Buttons + Dots Navigation)
// ================================================================
static void home_nav_cb(lv_event_t* e) {
    if (rpmBle.isReady()) {
        ui_navigate(Screen::NAVIGATION);
    } else {
        lv_label_set_text(connect_prompt_label, "Pair phone for live navigation");
        ui_navigate(Screen::CONNECT);
    }
}

static void home_maps_cb(lv_event_t* e) {
    ui_navigate(Screen::MAPS);
}

static void home_music_cb(lv_event_t* e) {
    if (rpmBle.isReady()) {
        ui_navigate(Screen::MUSIC);
    } else {
        lv_label_set_text(connect_prompt_label, "Pair phone to play music");
        ui_navigate(Screen::CONNECT);
    }
}

static void home_track_cb(lv_event_t* e) {
    ui_navigate(Screen::TRACKING);
}

static void home_docs_cb(lv_event_t* e) {
    docs_exit_fullscreen();
    show_doc(DocType::RC);
    ui_navigate(Screen::DOCS);
}

static void home_settings_cb(lv_event_t* e) {
    ui_navigate(Screen::SETTINGS);
}

static void home_bt_click_cb(lv_event_t* e) {
    ui_navigate(Screen::CONNECT);
}

// Helper: Create a Large 296x80 Card with 3px Accent Bar, 44px Icon Circle & Chevrons
static lv_obj_t* create_home_card(lv_obj_t* parent, int x, int y,
                                  const char* icon, const char* title, const char* sub,
                                  lv_color_t accent, lv_event_cb_t cb) {
    lv_obj_t* card = lv_btn_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, 296, 80);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_style_bg_color(card, COL_CARD_BG, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(card, COL_CARD_PRESSED, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, COL_CARD_BORDER, 0);
    lv_obj_set_style_radius(card, 14, 0);
    lv_obj_set_style_shadow_width(card, 6, 0);
    lv_obj_set_style_shadow_color(card, lv_color_hex(0x94A3B8), 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_30, 0);
    lv_obj_set_style_shadow_ofs_y(card, 2, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_add_event_cb(card, cb, LV_EVENT_CLICKED, nullptr);

    // 1. 4px Accent Bar on Left Edge
    lv_obj_t* bar = lv_obj_create(card);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, 4, 52);
    lv_obj_align(bar, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(bar, accent, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(bar, 2, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_CLICKABLE);

    // 2. 44px Soft-Tinted Icon Circle
    lv_obj_t* circle = lv_obj_create(card);
    lv_obj_remove_style_all(circle);
    lv_obj_set_size(circle, 44, 44);
    lv_obj_align(circle, LV_ALIGN_LEFT_MID, 14, 0);
    lv_obj_set_style_bg_color(circle, accent, 0);
    lv_obj_set_style_bg_opa(circle, LV_OPA_20, 0);
    lv_obj_set_style_border_width(circle, 1, 0);
    lv_obj_set_style_border_color(circle, accent, 0);
    lv_obj_set_style_border_opa(circle, LV_OPA_40, 0);
    lv_obj_set_style_radius(circle, 22, 0);
    lv_obj_clear_flag(circle, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* ico = lv_label_create(circle);
    lv_label_set_text(ico, icon);
    lv_obj_set_style_text_color(ico, accent, 0);
    lv_obj_set_style_text_font(ico, &lv_font_montserrat_20, 0);
    lv_obj_center(ico);
    lv_obj_clear_flag(ico, LV_OBJ_FLAG_CLICKABLE);

    // 3. Title (Montserrat 20, Dark Slate)
    lv_obj_t* t = lv_label_create(card);
    lv_label_set_text(t, title);
    lv_obj_set_style_text_color(t, COL_TEXT_PRI, 0);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_20, 0);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 68, 14);
    lv_obj_clear_flag(t, LV_OBJ_FLAG_CLICKABLE);

    // 4. Subtitle (Montserrat 12, Medium Slate)
    lv_obj_t* s = lv_label_create(card);
    lv_label_set_text(s, sub);
    lv_obj_set_style_text_color(s, COL_TEXT_SEC, 0);
    lv_obj_set_style_text_font(s, &lv_font_montserrat_12, 0);
    lv_obj_set_width(s, 196);
    lv_label_set_long_mode(s, LV_LABEL_LONG_DOT);
    lv_obj_align(s, LV_ALIGN_TOP_LEFT, 68, 44);
    lv_obj_clear_flag(s, LV_OBJ_FLAG_CLICKABLE);

    // 5. Subtle Chevron ">" on Right Edge
    lv_obj_t* chv = lv_label_create(card);
    lv_label_set_text(chv, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(chv, COL_TEXT_MUTED, 0);
    lv_obj_set_style_text_font(chv, &lv_font_montserrat_14, 0);
    lv_obj_align(chv, LV_ALIGN_RIGHT_MID, -14, 0);
    lv_obj_clear_flag(chv, LV_OBJ_FLAG_CLICKABLE);

    return card;
}

// Switch Home Screen active page with 250ms ease-out animation
void ui_home_set_page(int page) {
    if (page < 0) page = 0;
    if (page > 2) page = 2;
    if (!home_pages_cont) return;

    int start_x = lv_obj_get_x(home_pages_cont);
    int target_x = -page * 320;
    home_cur_page = page;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, home_pages_cont);
    lv_anim_set_values(&a, start_x, target_x);
    lv_anim_set_exec_cb(&a, [](void* obj, int32_t v) {
        if (obj) lv_obj_set_x((lv_obj_t*)obj, v);
    });
    lv_anim_set_time(&a, 250);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);

    // Update bottom page dots: active dot is elongated pill, inactive are small circles
    for (int i = 0; i < 3; i++) {
        if (home_dot_objs[i]) {
            if (i == page) {
                lv_obj_set_size(home_dot_objs[i], 18, 6);
                lv_obj_set_style_bg_color(home_dot_objs[i], ACCENT_NAV, 0);
                lv_obj_set_style_bg_opa(home_dot_objs[i], LV_OPA_COVER, 0);
            } else {
                lv_obj_set_size(home_dot_objs[i], 6, 6);
                lv_obj_set_style_bg_color(home_dot_objs[i], COL_CARD_BORDER, 0);
                lv_obj_set_style_bg_opa(home_dot_objs[i], LV_OPA_COVER, 0);
            }
        }
    }

    // Update < and > arrow states (dimmed text when at boundaries)
    if (home_prev_btn) {
        lv_obj_t* lbl = lv_obj_get_child(home_prev_btn, 0);
        if (lbl) lv_obj_set_style_text_color(lbl, (page > 0) ? COL_TEXT_PRI : COL_TEXT_MUTED, 0);
    }
    if (home_next_btn) {
        lv_obj_t* lbl = lv_obj_get_child(home_next_btn, 0);
        if (lbl) lv_obj_set_style_text_color(lbl, (page < 2) ? COL_TEXT_PRI : COL_TEXT_MUTED, 0);
    }
}

static void dot0_click_cb(lv_event_t* e) { ui_home_set_page(0); }
static void dot1_click_cb(lv_event_t* e) { ui_home_set_page(1); }
static void dot2_click_cb(lv_event_t* e) { ui_home_set_page(2); }
static void prev_click_cb(lv_event_t* e) { if (home_cur_page > 0) ui_home_set_page(home_cur_page - 1); }
static void next_click_cb(lv_event_t* e) { if (home_cur_page < 2) ui_home_set_page(home_cur_page + 1); }

static void create_home_screen() {
    scr_home = lv_obj_create(nullptr);
    apply_screen_theme(scr_home);
    lv_obj_clear_flag(scr_home, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(scr_home, LV_SCROLLBAR_MODE_OFF);

    // ================================================================
    // TOP STATUS BAR (Slim 32px height, touch targets >= 44px)
    // ================================================================
    lv_obj_t* status_bar = lv_obj_create(scr_home);
    lv_obj_remove_style_all(status_bar);
    lv_obj_set_size(status_bar, 320, 32);
    lv_obj_set_pos(status_bar, 0, 0);

    // 1. BLE Pill (x = 10, y = 3, size = 68x26, touch target >= 44px)
    lv_obj_t* bt_pill = lv_obj_create(status_bar);
    lv_obj_remove_style_all(bt_pill);
    lv_obj_set_size(bt_pill, 68, 26);
    lv_obj_set_pos(bt_pill, 10, 3);
    lv_obj_set_style_bg_color(bt_pill, COL_CARD_BG, 0);
    lv_obj_set_style_bg_opa(bt_pill, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(bt_pill, 12, 0);
    lv_obj_set_style_border_width(bt_pill, 1, 0);
    lv_obj_set_style_border_color(bt_pill, COL_CARD_BORDER, 0);
    lv_obj_add_flag(bt_pill, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(bt_pill, home_bt_click_cb, LV_EVENT_CLICKED, nullptr);

    home_bt_status = lv_label_create(bt_pill);
    lv_label_set_text(home_bt_status, LV_SYMBOL_BLUETOOTH " Ready");
    lv_obj_set_style_text_color(home_bt_status, ACCENT_DOCS, 0);
    lv_obj_set_style_text_font(home_bt_status, &lv_font_montserrat_12, 0);
    lv_obj_center(home_bt_status);
    lv_obj_clear_flag(home_bt_status, LV_OBJ_FLAG_CLICKABLE);

    // 2. Speed Pill (x = 84, y = 3, size = 80x26)
    lv_obj_t* speed_pill = lv_obj_create(status_bar);
    lv_obj_remove_style_all(speed_pill);
    lv_obj_set_size(speed_pill, 80, 26);
    lv_obj_set_pos(speed_pill, 84, 3);
    lv_obj_set_style_bg_color(speed_pill, COL_CARD_BG, 0);
    lv_obj_set_style_bg_opa(speed_pill, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(speed_pill, 12, 0);
    lv_obj_set_style_border_width(speed_pill, 1, 0);
    lv_obj_set_style_border_color(speed_pill, ACCENT_TRACK, 0);
    lv_obj_add_flag(speed_pill, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(speed_pill, home_track_cb, LV_EVENT_CLICKED, nullptr);

    home_speed_label = lv_label_create(speed_pill);
    lv_label_set_text(home_speed_label, LV_SYMBOL_CHARGE " 0 KM/H");
    lv_obj_set_style_text_color(home_speed_label, ACCENT_TRACK, 0);
    lv_obj_set_style_text_font(home_speed_label, &lv_font_montserrat_12, 0);
    lv_obj_center(home_speed_label);
    lv_obj_clear_flag(home_speed_label, LV_OBJ_FLAG_CLICKABLE);

    // 3. SD Storage Pill (x = 170, y = 3, size = 86x26)
    lv_obj_t* sd_pill = lv_obj_create(status_bar);
    lv_obj_remove_style_all(sd_pill);
    lv_obj_set_size(sd_pill, 86, 26);
    lv_obj_set_pos(sd_pill, 170, 3);
    lv_obj_set_style_bg_color(sd_pill, COL_CARD_BG, 0);
    lv_obj_set_style_bg_opa(sd_pill, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(sd_pill, 12, 0);
    lv_obj_set_style_border_width(sd_pill, 1, 0);
    lv_obj_set_style_border_color(sd_pill, COL_CARD_BORDER, 0);
    lv_obj_add_flag(sd_pill, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(sd_pill, home_maps_cb, LV_EVENT_CLICKED, nullptr);

    home_sd_label = lv_label_create(sd_pill);
    lv_label_set_text(home_sd_label, LV_SYMBOL_SD_CARD " Standby");
    lv_obj_set_style_text_color(home_sd_label, COL_TEXT_SEC, 0);
    lv_obj_set_style_text_font(home_sd_label, &lv_font_montserrat_12, 0);
    lv_obj_center(home_sd_label);
    lv_obj_clear_flag(home_sd_label, LV_OBJ_FLAG_CLICKABLE);

    // 4. Setup Gear Button (x = 262, y = 3, size = 48x26, touch target >= 44px)
    lv_obj_t* set_pill = lv_obj_create(status_bar);
    lv_obj_remove_style_all(set_pill);
    lv_obj_set_size(set_pill, 48, 26);
    lv_obj_set_pos(set_pill, 262, 3);
    lv_obj_set_style_bg_color(set_pill, COL_CARD_BG, 0);
    lv_obj_set_style_bg_opa(set_pill, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(set_pill, 12, 0);
    lv_obj_set_style_border_width(set_pill, 1, 0);
    lv_obj_set_style_border_color(set_pill, COL_CARD_BORDER, 0);
    lv_obj_add_flag(set_pill, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(set_pill, home_settings_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* set_ico = lv_label_create(set_pill);
    lv_label_set_text(set_ico, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_color(set_ico, ACCENT_SETTINGS, 0);
    lv_obj_set_style_text_font(set_ico, &lv_font_montserrat_14, 0);
    lv_obj_center(set_ico);
    lv_obj_clear_flag(set_ico, LV_OBJ_FLAG_CLICKABLE);

    // ================================================================
    // PAGED HORIZONTAL CONTAINER (3 Pages, 2 Cards per Page)
    // ================================================================
    home_pages_cont = lv_obj_create(scr_home);
    lv_obj_remove_style_all(home_pages_cont);
    lv_obj_set_size(home_pages_cont, 960, 174);
    lv_obj_set_pos(home_pages_cont, 0, 34);
    lv_obj_clear_flag(home_pages_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(home_pages_cont, LV_SCROLLBAR_MODE_OFF);

    // --- PAGE 1: Navigate, Offline Map (x = 0) ---
    lv_obj_t* page1 = lv_obj_create(home_pages_cont);
    lv_obj_remove_style_all(page1);
    lv_obj_set_size(page1, 320, 174);
    lv_obj_set_pos(page1, 0, 0);

    create_home_card(page1, 12, 2,  LV_SYMBOL_GPS,   "Navigate",    "Turn-by-turn routing",    ACCENT_NAV,  home_nav_cb);
    create_home_card(page1, 12, 88, LV_SYMBOL_IMAGE, "Offline Map", "Preloaded SD route pack", ACCENT_MAPS, home_maps_cb);

    // --- PAGE 2: Music, Tracking (x = 320) ---
    lv_obj_t* page2 = lv_obj_create(home_pages_cont);
    lv_obj_remove_style_all(page2);
    lv_obj_set_size(page2, 320, 174);
    lv_obj_set_pos(page2, 320, 0);

    create_home_card(page2, 12, 2,  LV_SYMBOL_AUDIO,  "Music",    "Now playing media",     ACCENT_MUSIC, home_music_cb);
    create_home_card(page2, 12, 88, LV_SYMBOL_CHARGE, "Tracking", "Trip & ride telemetry", ACCENT_TRACK, home_track_cb);

    // --- PAGE 3: Bike Docs, Settings (x = 640) ---
    lv_obj_t* page3 = lv_obj_create(home_pages_cont);
    lv_obj_remove_style_all(page3);
    lv_obj_set_size(page3, 320, 174);
    lv_obj_set_pos(page3, 640, 0);

    create_home_card(page3, 12, 2,  LV_SYMBOL_FILE,     "Bike Docs", "RC, DL & Aadhaar", ACCENT_DOCS,     home_docs_cb);
    create_home_card(page3, 12, 88, LV_SYMBOL_SETTINGS, "Settings",  "Display & system", ACCENT_SETTINGS, home_settings_cb);

    // ================================================================
    // BOTTOM NAVIGATION ROW (Tappable Dots + Corner < > Arrows)
    // ================================================================
    lv_obj_t* bottom_nav = lv_obj_create(scr_home);
    lv_obj_remove_style_all(bottom_nav);
    lv_obj_set_size(bottom_nav, 320, 28);
    lv_obj_set_pos(bottom_nav, 0, 210);

    // Left Corner "<" Arrow Button (48x26, rounded tactile pill)
    home_prev_btn = lv_btn_create(bottom_nav);
    lv_obj_remove_style_all(home_prev_btn);
    lv_obj_set_size(home_prev_btn, 48, 26);
    lv_obj_set_pos(home_prev_btn, 12, 1);
    lv_obj_set_style_bg_color(home_prev_btn, COL_CARD_BG, 0);
    lv_obj_set_style_bg_opa(home_prev_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(home_prev_btn, 1, 0);
    lv_obj_set_style_border_color(home_prev_btn, COL_CARD_BORDER, 0);
    lv_obj_set_style_radius(home_prev_btn, 13, 0);
    lv_obj_set_style_bg_color(home_prev_btn, COL_CARD_PRESSED, LV_STATE_PRESSED);
    lv_obj_add_event_cb(home_prev_btn, prev_click_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* prev_lbl = lv_label_create(home_prev_btn);
    lv_label_set_text(prev_lbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(prev_lbl, COL_TEXT_MUTED, 0); // Initially disabled on page 0
    lv_obj_set_style_text_font(prev_lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(prev_lbl);

    // Right Corner ">" Arrow Button (48x26, rounded tactile pill)
    home_next_btn = lv_btn_create(bottom_nav);
    lv_obj_remove_style_all(home_next_btn);
    lv_obj_set_size(home_next_btn, 48, 26);
    lv_obj_set_pos(home_next_btn, 260, 1);
    lv_obj_set_style_bg_color(home_next_btn, COL_CARD_BG, 0);
    lv_obj_set_style_bg_opa(home_next_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(home_next_btn, 1, 0);
    lv_obj_set_style_border_color(home_next_btn, COL_CARD_BORDER, 0);
    lv_obj_set_style_radius(home_next_btn, 13, 0);
    lv_obj_set_style_bg_color(home_next_btn, COL_CARD_PRESSED, LV_STATE_PRESSED);
    lv_obj_add_event_cb(home_next_btn, next_click_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* next_lbl = lv_label_create(home_next_btn);
    lv_label_set_text(next_lbl, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(next_lbl, COL_TEXT_PRI, 0);
    lv_obj_set_style_text_font(next_lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(next_lbl);

    // Center Tappable Page Dots Container (Touch targets >= 44px each)
    lv_obj_t* dots_cont = lv_obj_create(bottom_nav);
    lv_obj_remove_style_all(dots_cont);
    lv_obj_set_size(dots_cont, 132, 28);
    lv_obj_align(dots_cont, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_flex_flow(dots_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dots_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Dot 0 Touch Area (44x28)
    lv_obj_t* d0_hit = lv_btn_create(dots_cont);
    lv_obj_remove_style_all(d0_hit);
    lv_obj_set_size(d0_hit, 44, 28);
    lv_obj_add_event_cb(d0_hit, dot0_click_cb, LV_EVENT_CLICKED, nullptr);
    home_dot_objs[0] = lv_obj_create(d0_hit);
    lv_obj_remove_style_all(home_dot_objs[0]);
    lv_obj_set_size(home_dot_objs[0], 18, 6); // Active page 0 initially
    lv_obj_center(home_dot_objs[0]);
    lv_obj_set_style_bg_color(home_dot_objs[0], ACCENT_NAV, 0);
    lv_obj_set_style_radius(home_dot_objs[0], 3, 0);
    lv_obj_clear_flag(home_dot_objs[0], LV_OBJ_FLAG_CLICKABLE);

    // Dot 1 Touch Area (44x28)
    lv_obj_t* d1_hit = lv_btn_create(dots_cont);
    lv_obj_remove_style_all(d1_hit);
    lv_obj_set_size(d1_hit, 44, 28);
    lv_obj_add_event_cb(d1_hit, dot1_click_cb, LV_EVENT_CLICKED, nullptr);
    home_dot_objs[1] = lv_obj_create(d1_hit);
    lv_obj_remove_style_all(home_dot_objs[1]);
    lv_obj_set_size(home_dot_objs[1], 6, 6);
    lv_obj_center(home_dot_objs[1]);
    lv_obj_set_style_bg_color(home_dot_objs[1], COL_CARD_BORDER, 0);
    lv_obj_set_style_radius(home_dot_objs[1], 3, 0);
    lv_obj_clear_flag(home_dot_objs[1], LV_OBJ_FLAG_CLICKABLE);

    // Dot 2 Touch Area (44x28)
    lv_obj_t* d2_hit = lv_btn_create(dots_cont);
    lv_obj_remove_style_all(d2_hit);
    lv_obj_set_size(d2_hit, 44, 28);
    lv_obj_add_event_cb(d2_hit, dot2_click_cb, LV_EVENT_CLICKED, nullptr);
    home_dot_objs[2] = lv_obj_create(d2_hit);
    lv_obj_remove_style_all(home_dot_objs[2]);
    lv_obj_set_size(home_dot_objs[2], 6, 6);
    lv_obj_center(home_dot_objs[2]);
    lv_obj_set_style_bg_color(home_dot_objs[2], COL_CARD_BORDER, 0);
    lv_obj_set_style_radius(home_dot_objs[2], 3, 0);
    lv_obj_clear_flag(home_dot_objs[2], LV_OBJ_FLAG_CLICKABLE);
}

static void update_home_screen() {
    if (current_screen != Screen::HOME) return;

    if (rpmBle.isReady()) {
        lv_label_set_text(home_bt_status, LV_SYMBOL_BLUETOOTH " Paired");
        lv_obj_set_style_text_color(home_bt_status, ACCENT_MAPS, 0);
    } else {
        lv_label_set_text(home_bt_status, LV_SYMBOL_BLUETOOTH " Ready");
        lv_obj_set_style_text_color(home_bt_status, ACCENT_DOCS, 0);
    }

    if (tileReader.isReady()) {
        lv_label_set_text(home_sd_label, LV_SYMBOL_SD_CARD " SD OK");
        lv_obj_set_style_text_color(home_sd_label, ACCENT_MAPS, 0);
    } else {
        lv_label_set_text(home_sd_label, LV_SYMBOL_SD_CARD " Standby");
        lv_obj_set_style_text_color(home_sd_label, COL_TEXT_SEC, 0);
    }

    char spd_str[24];
    uint16_t spd = 0;
    if (deviceState.gps.valid && deviceState.gps.speed_kmh > 0) {
        spd = (uint16_t)deviceState.gps.speed_kmh;
    }
    snprintf(spd_str, sizeof(spd_str), LV_SYMBOL_CHARGE " %u KM/H", spd);
    lv_label_set_text(home_speed_label, spd_str);
}

// ================================================================
// 4. CONNECT SCREEN (Mobile Pairing)
// ================================================================
static void connect_back_cb(lv_event_t* e) { ui_navigate(Screen::HOME); }

static void create_connect_screen() {
    scr_connect = lv_obj_create(nullptr);
    apply_screen_theme(scr_connect);

    // Standard Sub-Screen Header
    lv_obj_t* back = lv_btn_create(scr_connect);
    lv_obj_remove_style_all(back);
    lv_obj_set_size(back, 52, 34);
    lv_obj_set_pos(back, 12, 10);
    lv_obj_set_style_bg_color(back, COL_CARD_BG, 0);
    lv_obj_set_style_border_width(back, 1, 0);
    lv_obj_set_style_border_color(back, COL_CARD_BORDER, 0);
    lv_obj_set_style_radius(back, 10, 0);
    lv_obj_add_event_cb(back, connect_back_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* bl = lv_label_create(back);
    lv_label_set_text(bl, LV_SYMBOL_LEFT " Home");
    lv_obj_set_style_text_color(bl, COL_TEXT_PRI, 0);
    lv_obj_set_style_text_font(bl, &lv_font_montserrat_12, 0);
    lv_obj_center(bl);

    // Centered Glass Card
    lv_obj_t* card = lv_obj_create(scr_connect);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, 296, 172);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, 16);
    lv_obj_set_style_bg_color(card, COL_CARD_BG, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, COL_CARD_BORDER, 0);
    lv_obj_set_style_radius(card, 14, 0);

    connect_spinner = lv_spinner_create(card, 1000, 60);
    lv_obj_set_size(connect_spinner, 44, 44);
    lv_obj_align(connect_spinner, LV_ALIGN_TOP_MID, 0, 14);
    lv_obj_set_style_arc_color(connect_spinner, ACCENT_NAV, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(connect_spinner, COL_CARD_INNER, LV_PART_MAIN);
    lv_obj_set_style_arc_width(connect_spinner, 4, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(connect_spinner, 4, LV_PART_MAIN);

    lv_obj_t* title = lv_label_create(card);
    lv_label_set_text(title, "Pair Mobile Device");
    lv_obj_set_style_text_color(title, COL_TEXT_PRI, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 66);

    connect_prompt_label = lv_label_create(card);
    lv_label_set_text(connect_prompt_label, "For live navigation & music");
    lv_obj_set_style_text_color(connect_prompt_label, COL_TEXT_SEC, 0);
    lv_obj_set_style_text_font(connect_prompt_label, &lv_font_montserrat_12, 0);
    lv_obj_align(connect_prompt_label, LV_ALIGN_TOP_MID, 0, 92);

    lv_obj_t* pill = lv_obj_create(card);
    lv_obj_remove_style_all(pill);
    lv_obj_set_size(pill, 250, 30);
    lv_obj_align(pill, LV_ALIGN_BOTTOM_MID, 0, -12);
    lv_obj_set_style_bg_color(pill, COL_CARD_INNER, 0);
    lv_obj_set_style_bg_opa(pill, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(pill, 15, 0);
    lv_obj_set_style_border_width(pill, 1, 0);
    lv_obj_set_style_border_color(pill, COL_CARD_BORDER, 0);

    connect_status_label = lv_label_create(pill);
    lv_label_set_text(connect_status_label, LV_SYMBOL_BLUETOOTH " Advertising as RPMLOOP");
    lv_obj_set_style_text_color(connect_status_label, ACCENT_NAV, 0);
    lv_obj_set_style_text_font(connect_status_label, &lv_font_montserrat_12, 0);
    lv_obj_center(connect_status_label);
}

static void update_connect_screen() {
    if (current_screen != Screen::CONNECT) return;

    switch (deviceState.ble) {
        case BleConnState::ADVERTISING:
            lv_label_set_text(connect_status_label, LV_SYMBOL_BLUETOOTH " Advertising as RPMLOOP");
            lv_obj_set_style_text_color(connect_status_label, ACCENT_NAV, 0);
            break;
        case BleConnState::CONNECTED:
            lv_label_set_text(connect_status_label, LV_SYMBOL_OK " Connected! Handshake...");
            lv_obj_set_style_text_color(connect_status_label, ACCENT_MAPS, 0);
            break;
        case BleConnState::HANDSHAKE_OK:
            lv_label_set_text(connect_status_label, LV_SYMBOL_OK " Handshake OK! Starting...");
            lv_obj_set_style_text_color(connect_status_label, ACCENT_MAPS, 0);
            ui_navigate(Screen::HOME);
            break;
        default:
            break;
    }
}

// ================================================================
// 5. NAVIGATION SCREEN
// ================================================================
static void nav_back_cb(lv_event_t* e) { ui_navigate(Screen::HOME); }

static void create_nav_screen() {
    scr_nav = lv_obj_create(nullptr);
    apply_screen_theme(scr_nav);

    nav_map_container = lv_obj_create(scr_nav);
    lv_obj_remove_style_all(nav_map_container);
    lv_obj_set_size(nav_map_container, 320, 240);
    lv_obj_set_pos(nav_map_container, 0, 0);

    nav_no_data_label = lv_label_create(nav_map_container);
    lv_label_set_text(nav_no_data_label, LV_SYMBOL_GPS " Live GPS Mode Active\nStart route in phone app");
    lv_obj_set_style_text_color(nav_no_data_label, COL_TEXT_SEC, 0);
    lv_obj_set_style_text_font(nav_no_data_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(nav_no_data_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(nav_no_data_label);

    // Floating Top Maneuver Banner
    lv_obj_t* banner = lv_obj_create(scr_nav);
    lv_obj_remove_style_all(banner);
    lv_obj_set_size(banner, 296, 60);
    lv_obj_align(banner, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_set_style_bg_color(banner, COL_CARD_BG, 0);
    lv_obj_set_style_bg_opa(banner, LV_OPA_90, 0);
    lv_obj_set_style_border_width(banner, 1, 0);
    lv_obj_set_style_border_color(banner, COL_CARD_BORDER, 0);
    lv_obj_set_style_radius(banner, 14, 0);

    lv_obj_t* back = lv_btn_create(banner);
    lv_obj_remove_style_all(back);
    lv_obj_set_size(back, 44, 44);
    lv_obj_align(back, LV_ALIGN_LEFT_MID, 4, 0);
    lv_obj_add_event_cb(back, nav_back_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* bl = lv_label_create(back);
    lv_label_set_text(bl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(bl, COL_TEXT_PRI, 0);
    lv_obj_set_style_text_font(bl, &lv_font_montserrat_16, 0);
    lv_obj_center(bl);

    nav_maneuver_icon = lv_label_create(banner);
    lv_label_set_text(nav_maneuver_icon, LV_SYMBOL_UP);
    lv_obj_set_style_text_color(nav_maneuver_icon, ACCENT_NAV, 0);
    lv_obj_set_style_text_font(nav_maneuver_icon, &lv_font_montserrat_28, 0);
    lv_obj_align(nav_maneuver_icon, LV_ALIGN_LEFT_MID, 48, 0);

    nav_distance_label = lv_label_create(banner);
    lv_label_set_text(nav_distance_label, "--");
    lv_obj_set_style_text_color(nav_distance_label, COL_TEXT_PRI, 0);
    lv_obj_set_style_text_font(nav_distance_label, &lv_font_montserrat_20, 0);
    lv_obj_align(nav_distance_label, LV_ALIGN_LEFT_MID, 84, -10);

    nav_street_label = lv_label_create(banner);
    lv_label_set_text(nav_street_label, "Ready for navigation");
    lv_obj_set_style_text_color(nav_street_label, COL_TEXT_SEC, 0);
    lv_obj_set_style_text_font(nav_street_label, &lv_font_montserrat_14, 0);
    lv_obj_set_width(nav_street_label, 190);
    lv_label_set_long_mode(nav_street_label, LV_LABEL_LONG_DOT);
    lv_obj_align(nav_street_label, LV_ALIGN_LEFT_MID, 84, 14);

    // Floating Bottom HUD
    lv_obj_t* bottom = lv_obj_create(scr_nav);
    lv_obj_remove_style_all(bottom);
    lv_obj_set_size(bottom, 296, 42);
    lv_obj_align(bottom, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_style_bg_color(bottom, COL_CARD_BG, 0);
    lv_obj_set_style_bg_opa(bottom, LV_OPA_90, 0);
    lv_obj_set_style_border_width(bottom, 1, 0);
    lv_obj_set_style_border_color(bottom, COL_CARD_BORDER, 0);
    lv_obj_set_style_radius(bottom, 12, 0);

    lv_obj_t* speed_bg = lv_obj_create(bottom);
    lv_obj_remove_style_all(speed_bg);
    lv_obj_set_size(speed_bg, 76, 28);
    lv_obj_align(speed_bg, LV_ALIGN_LEFT_MID, 6, 0);
    lv_obj_set_style_bg_color(speed_bg, COL_CARD_INNER, 0);
    lv_obj_set_style_border_width(speed_bg, 1, 0);
    lv_obj_set_style_border_color(speed_bg, ACCENT_TRACK, 0);
    lv_obj_set_style_radius(speed_bg, 8, 0);

    nav_speed_label = lv_label_create(speed_bg);
    lv_label_set_text(nav_speed_label, "0 km/h");
    lv_obj_set_style_text_color(nav_speed_label, ACCENT_TRACK, 0);
    lv_obj_set_style_text_font(nav_speed_label, &lv_font_montserrat_14, 0);
    lv_obj_center(nav_speed_label);

    nav_eta_label = lv_label_create(bottom);
    lv_label_set_text(nav_eta_label, "ETA --");
    lv_obj_set_style_text_color(nav_eta_label, COL_TEXT_PRI, 0);
    lv_obj_set_style_text_font(nav_eta_label, &lv_font_montserrat_14, 0);
    lv_obj_align(nav_eta_label, LV_ALIGN_CENTER, 8, 0);

    nav_remaining_label = lv_label_create(bottom);
    lv_label_set_text(nav_remaining_label, "-- km");
    lv_obj_set_style_text_color(nav_remaining_label, ACCENT_NAV, 0);
    lv_obj_set_style_text_font(nav_remaining_label, &lv_font_montserrat_14, 0);
    lv_obj_align(nav_remaining_label, LV_ALIGN_RIGHT_MID, -10, 0);
}

static void update_nav_screen() {
    if (current_screen != Screen::NAVIGATION) return;

    if (!tileReader.isReady()) {
        lv_obj_clear_flag(nav_no_data_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(nav_no_data_label, LV_OBJ_FLAG_HIDDEN);
    }

    if (deviceState.gps.valid) {
        if (tileReader.isReady()) {
            mapRenderer.setCenter(deviceState.gps.lat, deviceState.gps.lon);
            mapRenderer.setHeading(deviceState.gps.heading);
            if (deviceState.route.count > 0) {
                mapRenderer.setRoute(&deviceState.route);
            }
            mapRenderer.render();
        }

        char buf[16];
        snprintf(buf, sizeof(buf), "%.0f km/h", deviceState.gps.speed_kmh);
        lv_label_set_text(nav_speed_label, buf);
    }

    if (deviceState.nav.active) {
        lv_label_set_text(nav_maneuver_icon, maneuverSymbol(deviceState.nav.maneuver));

        char dist[16];
        formatDistance(dist, sizeof(dist), deviceState.nav.distance_m);
        lv_label_set_text(nav_distance_label, dist);

        lv_label_set_text(nav_street_label, deviceState.nav.street);
        lv_obj_set_style_text_color(nav_street_label, COL_TEXT_PRI, 0);

        char eta[16];
        snprintf(eta, sizeof(eta), "ETA %d min", deviceState.nav.eta_min);
        lv_label_set_text(nav_eta_label, eta);

        char rem[16];
        snprintf(rem, sizeof(rem), "%.1f km", deviceState.nav.remaining_km);
        lv_label_set_text(nav_remaining_label, rem);
    } else {
        lv_label_set_text(nav_street_label, "Start route in phone app");
        lv_obj_set_style_text_color(nav_street_label, COL_TEXT_SEC, 0);
        lv_label_set_text(nav_distance_label, "--");
        lv_label_set_text(nav_eta_label, "ETA --");
        lv_label_set_text(nav_remaining_label, "-- km");
    }
}

// ================================================================
// 6. OFFLINE MAPS SCREEN
// ================================================================
static void maps_zoom_in_cb(lv_event_t* e) {
    mapRenderer.zoomIn();
    char buf[8];
    snprintf(buf, sizeof(buf), "Z%d", mapRenderer.getZoom());
    lv_label_set_text(maps_zoom_label, buf);
    mapRenderer.render();
}

static void maps_zoom_out_cb(lv_event_t* e) {
    mapRenderer.zoomOut();
    char buf[8];
    snprintf(buf, sizeof(buf), "Z%d", mapRenderer.getZoom());
    lv_label_set_text(maps_zoom_label, buf);
    mapRenderer.render();
}

static void maps_back_cb(lv_event_t* e) { ui_navigate(Screen::HOME); }

static bool maps_touch_active = false;
static int16_t maps_last_x = 0, maps_last_y = 0;

static void maps_touch_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t* indev = lv_indev_get_act();
    if (!indev) return;

    lv_point_t p;
    lv_indev_get_point(indev, &p);

    if (code == LV_EVENT_PRESSED) {
        maps_touch_active = true;
        maps_last_x = p.x;
        maps_last_y = p.y;
    } else if (code == LV_EVENT_PRESSING && maps_touch_active) {
        int16_t dx = p.x - maps_last_x;
        int16_t dy = p.y - maps_last_y;
        if (abs(dx) > 2 || abs(dy) > 2) {
            mapRenderer.panBy(-dx, -dy);
            mapRenderer.render();
            maps_last_x = p.x;
            maps_last_y = p.y;
        }
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        maps_touch_active = false;
    }
}

static void create_maps_screen() {
    scr_maps = lv_obj_create(nullptr);
    apply_screen_theme(scr_maps);

    maps_container = lv_obj_create(scr_maps);
    lv_obj_remove_style_all(maps_container);
    lv_obj_set_size(maps_container, 320, 240);
    lv_obj_set_pos(maps_container, 0, 0);
    lv_obj_add_flag(maps_container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(maps_container, maps_touch_cb, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(maps_container, maps_touch_cb, LV_EVENT_PRESSING, nullptr);
    lv_obj_add_event_cb(maps_container, maps_touch_cb, LV_EVENT_RELEASED, nullptr);
    lv_obj_add_event_cb(maps_container, maps_touch_cb, LV_EVENT_PRESS_LOST, nullptr);

    maps_no_sd_label = lv_label_create(maps_container);
    lv_label_set_text(maps_no_sd_label, "Offline Map Storage\nInsert SD card with /tiles/");
    lv_obj_set_style_text_color(maps_no_sd_label, COL_TEXT_SEC, 0);
    lv_obj_set_style_text_font(maps_no_sd_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(maps_no_sd_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(maps_no_sd_label);

    // Zoom Controls (Touch targets >= 44px)
    lv_obj_t* zoom_in = lv_btn_create(scr_maps);
    lv_obj_remove_style_all(zoom_in);
    lv_obj_set_size(zoom_in, 44, 44);
    lv_obj_align(zoom_in, LV_ALIGN_BOTTOM_LEFT, 12, -10);
    lv_obj_set_style_bg_color(zoom_in, COL_CARD_BG, 0);
    lv_obj_set_style_border_width(zoom_in, 1, 0);
    lv_obj_set_style_border_color(zoom_in, COL_CARD_BORDER, 0);
    lv_obj_set_style_radius(zoom_in, 12, 0);
    lv_obj_add_event_cb(zoom_in, maps_zoom_in_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* zi_lbl = lv_label_create(zoom_in);
    lv_label_set_text(zi_lbl, LV_SYMBOL_PLUS);
    lv_obj_set_style_text_color(zi_lbl, COL_TEXT_PRI, 0);
    lv_obj_set_style_text_font(zi_lbl, &lv_font_montserrat_18, 0);
    lv_obj_center(zi_lbl);

    lv_obj_t* zoom_out = lv_btn_create(scr_maps);
    lv_obj_remove_style_all(zoom_out);
    lv_obj_set_size(zoom_out, 44, 44);
    lv_obj_align(zoom_out, LV_ALIGN_BOTTOM_LEFT, 62, -10);
    lv_obj_set_style_bg_color(zoom_out, COL_CARD_BG, 0);
    lv_obj_set_style_border_width(zoom_out, 1, 0);
    lv_obj_set_style_border_color(zoom_out, COL_CARD_BORDER, 0);
    lv_obj_set_style_radius(zoom_out, 12, 0);
    lv_obj_add_event_cb(zoom_out, maps_zoom_out_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* zo_lbl = lv_label_create(zoom_out);
    lv_label_set_text(zo_lbl, LV_SYMBOL_MINUS);
    lv_obj_set_style_text_color(zo_lbl, COL_TEXT_PRI, 0);
    lv_obj_set_style_text_font(zo_lbl, &lv_font_montserrat_18, 0);
    lv_obj_center(zo_lbl);

    maps_zoom_label = lv_label_create(scr_maps);
    lv_label_set_text(maps_zoom_label, "Z15");
    lv_obj_set_style_text_color(maps_zoom_label, ACCENT_MAPS, 0);
    lv_obj_set_style_text_font(maps_zoom_label, &lv_font_montserrat_14, 0);
    lv_obj_align(maps_zoom_label, LV_ALIGN_BOTTOM_LEFT, 116, -22);

    // Header Back button (Touch target 48x36)
    lv_obj_t* back_btn = lv_btn_create(scr_maps);
    lv_obj_remove_style_all(back_btn);
    lv_obj_set_size(back_btn, 48, 36);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 12, 10);
    lv_obj_set_style_bg_color(back_btn, COL_CARD_BG, 0);
    lv_obj_set_style_border_width(back_btn, 1, 0);
    lv_obj_set_style_border_color(back_btn, COL_CARD_BORDER, 0);
    lv_obj_set_style_radius(back_btn, 10, 0);
    lv_obj_add_event_cb(back_btn, maps_back_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* back_lbl = lv_label_create(back_btn);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_lbl, COL_TEXT_PRI, 0);
    lv_obj_set_style_text_font(back_lbl, &lv_font_montserrat_16, 0);
    lv_obj_center(back_lbl);
}

static void update_maps_screen() {
    if (current_screen != Screen::MAPS) return;

    if (!tileReader.isReady()) {
        lv_obj_clear_flag(maps_no_sd_label, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_add_flag(maps_no_sd_label, LV_OBJ_FLAG_HIDDEN);
}

// ================================================================
// 7. MUSIC SCREEN (Now Playing)
// ================================================================
static void music_back_cb(lv_event_t* e) { ui_navigate(Screen::HOME); }
static void music_prev_cb(lv_event_t* e) { rpmBle.sendMediaCommand(MediaCmd::PREVIOUS); }
static void music_play_cb(lv_event_t* e) {
    if (deviceState.media.playing) {
        rpmBle.sendMediaCommand(MediaCmd::PAUSE);
    } else {
        rpmBle.sendMediaCommand(MediaCmd::PLAY);
    }
}
static void music_next_cb(lv_event_t* e) { rpmBle.sendMediaCommand(MediaCmd::NEXT); }

static void create_music_screen() {
    scr_music = lv_obj_create(nullptr);
    apply_screen_theme(scr_music);

    // Header Back button (Touch target 48x36)
    lv_obj_t* back = lv_btn_create(scr_music);
    lv_obj_remove_style_all(back);
    lv_obj_set_size(back, 48, 36);
    lv_obj_set_pos(back, 12, 10);
    lv_obj_set_style_bg_color(back, COL_CARD_BG, 0);
    lv_obj_set_style_border_width(back, 1, 0);
    lv_obj_set_style_border_color(back, COL_CARD_BORDER, 0);
    lv_obj_set_style_radius(back, 10, 0);
    lv_obj_add_event_cb(back, music_back_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* bl = lv_label_create(back);
    lv_label_set_text(bl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(bl, COL_TEXT_PRI, 0);
    lv_obj_set_style_text_font(bl, &lv_font_montserrat_16, 0);
    lv_obj_center(bl);

    lv_obj_t* scr_title = lv_label_create(scr_music);
    lv_label_set_text(scr_title, "Now Playing");
    lv_obj_set_style_text_color(scr_title, COL_TEXT_PRI, 0);
    lv_obj_set_style_text_font(scr_title, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(scr_title, 66, 16);

    // Track Details Glass Card (x = 12, width = 296, height = 98)
    lv_obj_t* card = lv_obj_create(scr_music);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, 296, 98);
    lv_obj_set_pos(card, 12, 52);
    lv_obj_set_style_bg_color(card, COL_CARD_BG, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, COL_CARD_BORDER, 0);
    lv_obj_set_style_radius(card, 14, 0);

    // Album Art Square (72x72)
    lv_obj_t* art = lv_obj_create(card);
    lv_obj_remove_style_all(art);
    lv_obj_set_size(art, 72, 72);
    lv_obj_set_pos(art, 12, 13);
    lv_obj_set_style_bg_color(art, COL_CARD_INNER, 0);
    lv_obj_set_style_border_width(art, 1, 0);
    lv_obj_set_style_border_color(art, COL_CARD_BORDER, 0);
    lv_obj_set_style_radius(art, 12, 0);

    lv_obj_t* art_ico = lv_label_create(art);
    lv_label_set_text(art_ico, LV_SYMBOL_AUDIO);
    lv_obj_set_style_text_color(art_ico, ACCENT_MUSIC, 0);
    lv_obj_set_style_text_font(art_ico, &lv_font_montserrat_28, 0);
    lv_obj_center(art_ico);

    // Track metadata (Shortened/fitted to avoid clipping)
    music_title_label = lv_label_create(card);
    lv_label_set_text(music_title_label, "No Track Playing");
    lv_obj_set_style_text_color(music_title_label, COL_TEXT_PRI, 0);
    lv_obj_set_style_text_font(music_title_label, &lv_font_montserrat_18, 0);
    lv_obj_set_width(music_title_label, 186);
    lv_label_set_long_mode(music_title_label, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(music_title_label, 96, 12);

    music_artist_label = lv_label_create(card);
    lv_label_set_text(music_artist_label, "Connect phone media");
    lv_obj_set_style_text_color(music_artist_label, ACCENT_NAV, 0);
    lv_obj_set_style_text_font(music_artist_label, &lv_font_montserrat_14, 0);
    lv_obj_set_width(music_artist_label, 186);
    lv_label_set_long_mode(music_artist_label, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(music_artist_label, 96, 38);

    music_album_label = lv_label_create(card);
    lv_label_set_text(music_album_label, "");
    lv_obj_set_style_text_color(music_album_label, COL_TEXT_SEC, 0);
    lv_obj_set_style_text_font(music_album_label, &lv_font_montserrat_12, 0);
    lv_obj_set_width(music_album_label, 186);
    lv_label_set_long_mode(music_album_label, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(music_album_label, 96, 62);

    // Progress Bar (Width 296, x = 12, y = 156)
    music_progress_bar = lv_bar_create(scr_music);
    lv_obj_set_size(music_progress_bar, 296, 6);
    lv_obj_set_pos(music_progress_bar, 12, 156);
    lv_obj_set_style_bg_color(music_progress_bar, COL_CARD_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_color(music_progress_bar, ACCENT_MUSIC, LV_PART_INDICATOR);
    lv_obj_set_style_radius(music_progress_bar, 3, LV_PART_MAIN);
    lv_obj_set_style_radius(music_progress_bar, 3, LV_PART_INDICATOR);
    lv_bar_set_range(music_progress_bar, 0, 100);
    lv_bar_set_value(music_progress_bar, 0, LV_ANIM_OFF);

    music_time_label = lv_label_create(scr_music);
    lv_label_set_text(music_time_label, "0:00 / 0:00");
    lv_obj_set_style_text_color(music_time_label, COL_TEXT_SEC, 0);
    lv_obj_set_style_text_font(music_time_label, &lv_font_montserrat_12, 0);
    lv_obj_align(music_time_label, LV_ALIGN_TOP_MID, 0, 166);

    // Playback Controls Row (Touch targets >= 44px)
    lv_obj_t* controls = lv_obj_create(scr_music);
    lv_obj_remove_style_all(controls);
    lv_obj_set_size(controls, 296, 44);
    lv_obj_set_pos(controls, 12, 188);
    lv_obj_set_flex_flow(controls, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(controls, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Prev Button (56x38)
    lv_obj_t* prev = lv_btn_create(controls);
    lv_obj_remove_style_all(prev);
    lv_obj_set_size(prev, 56, 38);
    lv_obj_set_style_bg_color(prev, COL_CARD_BG, 0);
    lv_obj_set_style_border_width(prev, 1, 0);
    lv_obj_set_style_border_color(prev, COL_CARD_BORDER, 0);
    lv_obj_set_style_radius(prev, 10, 0);
    lv_obj_add_event_cb(prev, music_prev_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* pl = lv_label_create(prev);
    lv_label_set_text(pl, LV_SYMBOL_PREV);
    lv_obj_set_style_text_color(pl, COL_TEXT_PRI, 0);
    lv_obj_set_style_text_font(pl, &lv_font_montserrat_16, 0);
    lv_obj_center(pl);

    // Play/Pause Button (72x40)
    music_play_btn = lv_btn_create(controls);
    lv_obj_remove_style_all(music_play_btn);
    lv_obj_set_size(music_play_btn, 72, 40);
    lv_obj_set_style_bg_color(music_play_btn, ACCENT_MUSIC, 0);
    lv_obj_set_style_radius(music_play_btn, 12, 0);
    lv_obj_add_event_cb(music_play_btn, music_play_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* pp = lv_label_create(music_play_btn);
    lv_label_set_text(pp, LV_SYMBOL_PLAY);
    lv_obj_set_style_text_color(pp, lv_color_white(), 0);
    lv_obj_set_style_text_font(pp, &lv_font_montserrat_18, 0);
    lv_obj_center(pp);

    // Next Button (56x38)
    lv_obj_t* next = lv_btn_create(controls);
    lv_obj_remove_style_all(next);
    lv_obj_set_size(next, 56, 38);
    lv_obj_set_style_bg_color(next, COL_CARD_BG, 0);
    lv_obj_set_style_border_width(next, 1, 0);
    lv_obj_set_style_border_color(next, COL_CARD_BORDER, 0);
    lv_obj_set_style_radius(next, 10, 0);
    lv_obj_add_event_cb(next, music_next_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* nl = lv_label_create(next);
    lv_label_set_text(nl, LV_SYMBOL_NEXT);
    lv_obj_set_style_text_color(nl, COL_TEXT_PRI, 0);
    lv_obj_set_style_text_font(nl, &lv_font_montserrat_16, 0);
    lv_obj_center(nl);
}

static void update_music_screen() {
    if (current_screen != Screen::MUSIC) return;

    if (deviceState.media.title[0] != '\0') {
        lv_label_set_text(music_title_label, deviceState.media.title);
        lv_label_set_text(music_artist_label, deviceState.media.artist);
        lv_label_set_text(music_album_label, deviceState.media.album);

        if (deviceState.media.duration_s > 0) {
            int pct = (int)((float)deviceState.media.position_s / deviceState.media.duration_s * 100);
            lv_bar_set_value(music_progress_bar, pct, LV_ANIM_ON);

            char timeBuf[24];
            int pm = deviceState.media.position_s / 60;
            int ps = deviceState.media.position_s % 60;
            int dm = deviceState.media.duration_s / 60;
            int ds = deviceState.media.duration_s % 60;
            snprintf(timeBuf, sizeof(timeBuf), "%d:%02d / %d:%02d", pm, ps, dm, ds);
            lv_label_set_text(music_time_label, timeBuf);
        }

        lv_obj_t* pp_label = lv_obj_get_child(music_play_btn, 0);
        if (pp_label) {
            lv_label_set_text(pp_label, deviceState.media.playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
        }
    } else {
        lv_label_set_text(music_title_label, "No Track Playing");
        lv_label_set_text(music_artist_label, "Play music on your phone");
        lv_label_set_text(music_album_label, "");
    }
}

// ================================================================
// 8. TRACKING SCREEN (Trip Computer)
// ================================================================
static void tracking_back_cb(lv_event_t* e) { ui_navigate(Screen::HOME); }

static lv_obj_t* create_tracking_stat_card(lv_obj_t* parent, int x, int y, const char* title, lv_obj_t** val_out) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, 142, 58);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_style_bg_color(card, COL_CARD_BG, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, COL_CARD_BORDER, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_pad_all(card, 8, 0);

    lv_obj_t* t = lv_label_create(card);
    lv_label_set_text(t, title);
    lv_obj_set_style_text_color(t, COL_TEXT_SEC, 0);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_12, 0);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 2, 0);

    lv_obj_t* v = lv_label_create(card);
    lv_label_set_text(v, "--");
    lv_obj_set_style_text_color(v, COL_TEXT_PRI, 0);
    lv_obj_set_style_text_font(v, &lv_font_montserrat_20, 0);
    lv_obj_align(v, LV_ALIGN_BOTTOM_LEFT, 2, 0);

    *val_out = v;
    return card;
}

static void create_tracking_screen() {
    scr_tracking = lv_obj_create(nullptr);
    apply_screen_theme(scr_tracking);

    // Header Back button (Touch target 48x36)
    lv_obj_t* back = lv_btn_create(scr_tracking);
    lv_obj_remove_style_all(back);
    lv_obj_set_size(back, 48, 36);
    lv_obj_set_pos(back, 12, 10);
    lv_obj_set_style_bg_color(back, COL_CARD_BG, 0);
    lv_obj_set_style_border_width(back, 1, 0);
    lv_obj_set_style_border_color(back, COL_CARD_BORDER, 0);
    lv_obj_set_style_radius(back, 10, 0);
    lv_obj_add_event_cb(back, tracking_back_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* bl = lv_label_create(back);
    lv_label_set_text(bl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(bl, COL_TEXT_PRI, 0);
    lv_obj_set_style_text_font(bl, &lv_font_montserrat_16, 0);
    lv_obj_center(bl);

    lv_obj_t* title = lv_label_create(scr_tracking);
    lv_label_set_text(title, "Trip Computer");
    lv_obj_set_style_text_color(title, ACCENT_TRACK, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(title, 66, 16);

    // Standby message when tracking is inactive
    track_no_data_label = lv_label_create(scr_tracking);
    lv_label_set_text(track_no_data_label, "Live Trip Telemetry\nStart ride tracking on phone");
    lv_obj_set_style_text_color(track_no_data_label, COL_TEXT_SEC, 0);
    lv_obj_set_style_text_font(track_no_data_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(track_no_data_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(track_no_data_label);

    // 2x2 Stats Grid Container (x = 12, width = 296, height = 126, y = 52)
    lv_obj_t* grid = lv_obj_create(scr_tracking);
    lv_obj_remove_style_all(grid);
    lv_obj_set_size(grid, 296, 126);
    lv_obj_set_pos(grid, 12, 52);
    lv_obj_add_flag(grid, LV_OBJ_FLAG_HIDDEN);

    create_tracking_stat_card(grid, 0,   0,  "Distance",  &track_distance_label);
    create_tracking_stat_card(grid, 154, 0,  "Duration",  &track_time_label);
    create_tracking_stat_card(grid, 0,   66, "Avg Speed", &track_avg_speed_label);
    create_tracking_stat_card(grid, 154, 66, "Top Speed", &track_max_speed_label);

    // Current Speed Pill (x = 12, width = 296, height = 44, y = 186)
    lv_obj_t* cur_speed_card = lv_obj_create(scr_tracking);
    lv_obj_remove_style_all(cur_speed_card);
    lv_obj_set_size(cur_speed_card, 296, 44);
    lv_obj_set_pos(cur_speed_card, 12, 186);
    lv_obj_set_style_bg_color(cur_speed_card, COL_CARD_BG, 0);
    lv_obj_set_style_border_width(cur_speed_card, 1, 0);
    lv_obj_set_style_border_color(cur_speed_card, ACCENT_TRACK, 0);
    lv_obj_set_style_radius(cur_speed_card, 12, 0);
    lv_obj_add_flag(cur_speed_card, LV_OBJ_FLAG_HIDDEN);

    track_cur_speed_label = lv_label_create(cur_speed_card);
    lv_label_set_text(track_cur_speed_label, "0 km/h");
    lv_obj_set_style_text_color(track_cur_speed_label, ACCENT_TRACK, 0);
    lv_obj_set_style_text_font(track_cur_speed_label, &lv_font_montserrat_24, 0);
    lv_obj_center(track_cur_speed_label);
}

static void update_tracking_screen() {
    if (current_screen != Screen::TRACKING) return;

    if (deviceState.tracking.active) {
        lv_obj_add_flag(track_no_data_label, LV_OBJ_FLAG_HIDDEN);

        lv_obj_t* grid = lv_obj_get_parent(track_distance_label);
        if (grid) lv_obj_clear_flag(grid, LV_OBJ_FLAG_HIDDEN);
        lv_obj_t* spd_card = lv_obj_get_parent(track_cur_speed_label);
        if (spd_card) lv_obj_clear_flag(spd_card, LV_OBJ_FLAG_HIDDEN);

        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f km", deviceState.tracking.distance_km);
        lv_label_set_text(track_distance_label, buf);

        formatDuration(buf, sizeof(buf), deviceState.tracking.elapsed_s);
        lv_label_set_text(track_time_label, buf);

        snprintf(buf, sizeof(buf), "%.1f", deviceState.tracking.avg_speed_kmh);
        lv_label_set_text(track_avg_speed_label, buf);

        snprintf(buf, sizeof(buf), "%.1f", deviceState.tracking.max_speed_kmh);
        lv_label_set_text(track_max_speed_label, buf);

        snprintf(buf, sizeof(buf), "%.0f km/h", deviceState.tracking.current_speed_kmh);
        lv_label_set_text(track_cur_speed_label, buf);
    } else {
        lv_obj_clear_flag(track_no_data_label, LV_OBJ_FLAG_HIDDEN);
    }
}

// ================================================================
// 9. SETTINGS SCREEN
// ================================================================
static void settings_back_cb(lv_event_t* e) { ui_navigate(Screen::HOME); }
static void settings_brightness_cb(lv_event_t* e) {
    lv_obj_t* slider = lv_event_get_target(e);
    uint8_t val = (uint8_t)lv_slider_get_value(slider);
    ui_set_brightness(val);
    rpmBle.sendBrightnessAck(val);
}

static void create_settings_screen() {
    scr_settings = lv_obj_create(nullptr);
    apply_screen_theme(scr_settings);

    // Header Back button (Touch target 48x36)
    lv_obj_t* back = lv_btn_create(scr_settings);
    lv_obj_remove_style_all(back);
    lv_obj_set_size(back, 48, 36);
    lv_obj_set_pos(back, 12, 10);
    lv_obj_set_style_bg_color(back, COL_CARD_BG, 0);
    lv_obj_set_style_border_width(back, 1, 0);
    lv_obj_set_style_border_color(back, COL_CARD_BORDER, 0);
    lv_obj_set_style_radius(back, 10, 0);
    lv_obj_add_event_cb(back, settings_back_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* bl = lv_label_create(back);
    lv_label_set_text(bl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(bl, COL_TEXT_PRI, 0);
    lv_obj_set_style_text_font(bl, &lv_font_montserrat_16, 0);
    lv_obj_center(bl);

    lv_obj_t* title = lv_label_create(scr_settings);
    lv_label_set_text(title, "Settings");
    lv_obj_set_style_text_color(title, COL_TEXT_PRI, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(title, 66, 16);

    // Card 1: Display Brightness (x = 12, y = 52, width = 296, height = 62)
    lv_obj_t* bcard = lv_obj_create(scr_settings);
    lv_obj_remove_style_all(bcard);
    lv_obj_set_size(bcard, 296, 62);
    lv_obj_set_pos(bcard, 12, 52);
    lv_obj_set_style_bg_color(bcard, COL_CARD_BG, 0);
    lv_obj_set_style_bg_opa(bcard, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bcard, 1, 0);
    lv_obj_set_style_border_color(bcard, COL_CARD_BORDER, 0);
    lv_obj_set_style_radius(bcard, 14, 0);

    lv_obj_t* bright_label = lv_label_create(bcard);
    lv_label_set_text(bright_label, "Display Brightness");
    lv_obj_set_style_text_color(bright_label, COL_TEXT_PRI, 0);
    lv_obj_set_style_text_font(bright_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(bright_label, 14, 10);

    settings_brightness_slider = lv_slider_create(bcard);
    lv_obj_set_size(settings_brightness_slider, 268, 12);
    lv_obj_align(settings_brightness_slider, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_slider_set_range(settings_brightness_slider, 20, 255);
    lv_slider_set_value(settings_brightness_slider, 255, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(settings_brightness_slider, COL_CARD_INNER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(settings_brightness_slider, ACCENT_SETTINGS, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(settings_brightness_slider, ACCENT_SETTINGS, LV_PART_KNOB);
    lv_obj_add_event_cb(settings_brightness_slider, settings_brightness_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    // Card 2: System Information (x = 12, y = 122, width = 296, height = 106)
    // All strings shortened to guarantee zero clipping at 320x240
    lv_obj_t* info_card = lv_obj_create(scr_settings);
    lv_obj_remove_style_all(info_card);
    lv_obj_set_size(info_card, 296, 106);
    lv_obj_set_pos(info_card, 12, 122);
    lv_obj_set_style_bg_color(info_card, COL_CARD_BG, 0);
    lv_obj_set_style_bg_opa(info_card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(info_card, 1, 0);
    lv_obj_set_style_border_color(info_card, COL_CARD_BORDER, 0);
    lv_obj_set_style_radius(info_card, 14, 0);
    lv_obj_set_style_pad_all(info_card, 12, 0);

    lv_obj_t* fw_label = lv_label_create(info_card);
    lv_label_set_text(fw_label, "Firmware: RPMLOOP v3.0");
    lv_obj_set_style_text_color(fw_label, COL_TEXT_PRI, 0);
    lv_obj_set_style_text_font(fw_label, &lv_font_montserrat_14, 0);
    lv_obj_align(fw_label, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t* hw_label = lv_label_create(info_card);
    lv_label_set_text(hw_label, "Hardware: ESP32-S3 + ST7789");
    lv_obj_set_style_text_color(hw_label, COL_TEXT_SEC, 0);
    lv_obj_set_style_text_font(hw_label, &lv_font_montserrat_14, 0);
    lv_obj_align(hw_label, LV_ALIGN_TOP_LEFT, 0, 22);

    settings_mem_label = lv_label_create(info_card);
    char memBuf[64];
    snprintf(memBuf, sizeof(memBuf), "Memory: Heap %dK | PSRAM %dK free",
        (int)(ESP.getFreeHeap() / 1024), (int)(ESP.getFreePsram() / 1024));
    lv_label_set_text(settings_mem_label, memBuf);
    lv_obj_set_style_text_color(settings_mem_label, COL_TEXT_SEC, 0);
    lv_obj_set_style_text_font(settings_mem_label, &lv_font_montserrat_14, 0);
    lv_obj_align(settings_mem_label, LV_ALIGN_TOP_LEFT, 0, 44);

    settings_sd_label = lv_label_create(info_card);
    if (tileReader.isReady()) {
        char sdBuf[48];
        snprintf(sdBuf, sizeof(sdBuf), "SD Storage: %s (%llu MB)",
            tileReader.cardType(), tileReader.cardSizeBytes() / (1024 * 1024));
        lv_label_set_text(settings_sd_label, sdBuf);
        lv_obj_set_style_text_color(settings_sd_label, ACCENT_MAPS, 0);
    } else {
        lv_label_set_text(settings_sd_label, "SD Card: Standby (Offline)");
        lv_obj_set_style_text_color(settings_sd_label, ACCENT_DOCS, 0);
    }
    lv_obj_set_style_text_font(settings_sd_label, &lv_font_montserrat_14, 0);
    lv_obj_align(settings_sd_label, LV_ALIGN_TOP_LEFT, 0, 66);
}

// ================================================================
// 10. BIKE DOCUMENTS SCREEN (RC, DL, Aadhaar + Tap-to-Expand 1:1)
// ================================================================
static void docs_back_cb(lv_event_t* e) {
    if (docs_is_fullscreen) {
        docs_exit_fullscreen();
    } else {
        ui_navigate(Screen::HOME);
    }
}

static void docs_exit_fullscreen() {
    docs_is_fullscreen = false;
    if (docs_fs_overlay) {
        lv_obj_add_flag(docs_fs_overlay, LV_OBJ_FLAG_HIDDEN);
    }
}

static void docs_enter_fullscreen() {
    if (current_doc == DocType::DL) return; // DL is pending
    docs_is_fullscreen = true;
    if (docs_fs_overlay) {
        lv_obj_clear_flag(docs_fs_overlay, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(docs_fs_overlay);
    }
}

static void docs_card_click_cb(lv_event_t* e) {
    docs_enter_fullscreen();
}

static void docs_fs_click_cb(lv_event_t* e) {
    docs_exit_fullscreen();
}

static void docs_flip_cb(lv_event_t* e) {
    if (current_doc == DocType::RC) {
        rc_show_back = !rc_show_back;
        show_doc(DocType::RC);
    } else if (current_doc == DocType::AADHAAR) {
        aadhar_show_back = !aadhar_show_back;
        show_doc(DocType::AADHAAR);
    }
}

static void show_doc(DocType doc) {
    current_doc = doc;

    // Highlight active segmented tab
    if (docs_tab_rc) {
        lv_obj_set_style_bg_opa(docs_tab_rc, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(docs_tab_rc, (doc == DocType::RC) ? ACCENT_NAV : COL_CARD_BG, 0);
        lv_obj_set_style_border_width(docs_tab_rc, (doc == DocType::RC) ? 0 : 1, 0);
        lv_obj_set_style_border_color(docs_tab_rc, COL_CARD_BORDER, 0);
        lv_obj_t* c = lv_obj_get_child(docs_tab_rc, 0);
        if (c) lv_obj_set_style_text_color(c, (doc == DocType::RC) ? lv_color_white() : COL_TEXT_PRI, 0);
    }

    if (docs_tab_dl) {
        lv_obj_set_style_bg_opa(docs_tab_dl, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(docs_tab_dl, (doc == DocType::DL) ? ACCENT_TRACK : COL_CARD_BG, 0);
        lv_obj_set_style_border_width(docs_tab_dl, (doc == DocType::DL) ? 0 : 1, 0);
        lv_obj_set_style_border_color(docs_tab_dl, COL_CARD_BORDER, 0);
        lv_obj_t* c = lv_obj_get_child(docs_tab_dl, 0);
        if (c) lv_obj_set_style_text_color(c, (doc == DocType::DL) ? lv_color_white() : COL_TEXT_PRI, 0);
    }

    if (docs_tab_aadhar) {
        lv_obj_set_style_bg_opa(docs_tab_aadhar, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(docs_tab_aadhar, (doc == DocType::AADHAAR) ? ACCENT_DOCS : COL_CARD_BG, 0);
        lv_obj_set_style_border_width(docs_tab_aadhar, (doc == DocType::AADHAAR) ? 0 : 1, 0);
        lv_obj_set_style_border_color(docs_tab_aadhar, COL_CARD_BORDER, 0);
        lv_obj_t* c = lv_obj_get_child(docs_tab_aadhar, 0);
        if (c) lv_obj_set_style_text_color(c, (doc == DocType::AADHAAR) ? lv_color_white() : COL_TEXT_PRI, 0);
    }

    // Flip button label & visibility
    const char* flip_text = (doc == DocType::RC) ?
                            (rc_show_back ? (LV_SYMBOL_REFRESH " Front") : (LV_SYMBOL_REFRESH " Back")) :
                            (aadhar_show_back ? (LV_SYMBOL_REFRESH " Front") : (LV_SYMBOL_REFRESH " Back"));

    if (docs_flip_btn) {
        if (doc == DocType::RC || doc == DocType::AADHAAR) {
            lv_obj_clear_flag(docs_flip_btn, LV_OBJ_FLAG_HIDDEN);
            if (docs_flip_lbl) lv_label_set_text(docs_flip_lbl, flip_text);
        } else {
            lv_obj_add_flag(docs_flip_btn, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (docs_fs_flip_btn) {
        if (doc == DocType::RC || doc == DocType::AADHAAR) {
            lv_obj_clear_flag(docs_fs_flip_btn, LV_OBJ_FLAG_HIDDEN);
            if (docs_fs_flip_lbl) lv_label_set_text(docs_fs_flip_lbl, flip_text);
        } else {
            lv_obj_add_flag(docs_fs_flip_btn, LV_OBJ_FLAG_HIDDEN);
        }
    }

    const char* docName = "rc";
    const char* fsTitle = "Vehicle RC Book";
    if (doc == DocType::RC) {
        docName = rc_show_back ? "rc_back" : "rc";
        fsTitle = rc_show_back ? "RC Book (Back Specs)" : "RC Book (Front Punjab)";
    } else if (doc == DocType::DL) {
        docName = "dl";
        fsTitle = "Driving Licence";
    } else {
        docName = aadhar_show_back ? "aadhar_back" : "aadhar";
        fsTitle = aadhar_show_back ? "Aadhaar (Back Address)" : "Aadhaar (Front Photo)";
    }

    if (docs_fs_title_lbl) {
        lv_label_set_text(docs_fs_title_lbl, fsTitle);
    }

    bool imageLoaded = false;

    // Allocate 320x240 RGB565 buffer in PSRAM on first access
    if (!docs_img_buf) {
        docs_img_buf = (uint16_t*)ps_malloc(DOC_IMG_W * DOC_IMG_H * sizeof(uint16_t));
        if (!docs_img_buf) {
            docs_img_buf = (uint16_t*)malloc(DOC_IMG_W * DOC_IMG_H * sizeof(uint16_t));
        }
        if (docs_img_buf) {
            docs_img_dsc.header.always_zero = 0;
            docs_img_dsc.header.w = DOC_IMG_W;
            docs_img_dsc.header.h = DOC_IMG_H;
            docs_img_dsc.data_size = DOC_IMG_W * DOC_IMG_H * sizeof(uint16_t);
            docs_img_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
            docs_img_dsc.data = (const uint8_t*)docs_img_buf;
        }
    }

    if (docs_img_buf) {
        imageLoaded = tileReader.loadDocJpeg(docName, docs_img_buf, DOC_IMG_W, DOC_IMG_H);
    }

    if (imageLoaded) {
        if (docs_img_obj) {
            lv_img_set_src(docs_img_obj, &docs_img_dsc);
            // Fit inside 296x144 card: zoom = 146
            lv_img_set_zoom(docs_img_obj, 146);
            lv_obj_clear_flag(docs_img_obj, LV_OBJ_FLAG_HIDDEN);
        }
        if (docs_fs_img) {
            lv_img_set_src(docs_fs_img, &docs_img_dsc);
            lv_img_set_zoom(docs_fs_img, 256); // 1:1 Maximum native 320x240 ST7789 quality
            lv_obj_clear_flag(docs_fs_img, LV_OBJ_FLAG_HIDDEN);
        }
        if (docs_hint_lbl) lv_obj_clear_flag(docs_hint_lbl, LV_OBJ_FLAG_HIDDEN);
        if (docs_title_lbl) lv_obj_add_flag(docs_title_lbl, LV_OBJ_FLAG_HIDDEN);
        if (docs_sub_lbl) lv_obj_add_flag(docs_sub_lbl, LV_OBJ_FLAG_HIDDEN);
        if (docs_file_lbl) lv_obj_add_flag(docs_file_lbl, LV_OBJ_FLAG_HIDDEN);
        if (docs_info_lbl) lv_obj_add_flag(docs_info_lbl, LV_OBJ_FLAG_HIDDEN);
    } else {
        if (docs_img_obj) lv_obj_add_flag(docs_img_obj, LV_OBJ_FLAG_HIDDEN);
        if (docs_hint_lbl) lv_obj_add_flag(docs_hint_lbl, LV_OBJ_FLAG_HIDDEN);
        if (docs_title_lbl) lv_obj_clear_flag(docs_title_lbl, LV_OBJ_FLAG_HIDDEN);
        if (docs_sub_lbl) lv_obj_clear_flag(docs_sub_lbl, LV_OBJ_FLAG_HIDDEN);
        if (docs_file_lbl) lv_obj_clear_flag(docs_file_lbl, LV_OBJ_FLAG_HIDDEN);
        if (docs_info_lbl) lv_obj_clear_flag(docs_info_lbl, LV_OBJ_FLAG_HIDDEN);

        if (doc == DocType::DL) {
            if (docs_title_lbl) {
                lv_label_set_text(docs_title_lbl, "DRIVING LICENCE (DL)");
                lv_obj_set_style_text_color(docs_title_lbl, ACCENT_TRACK, 0);
            }
            if (docs_sub_lbl) lv_label_set_text(docs_sub_lbl, "Holder: Sunny Gupta | MCWG / LMV");
            if (docs_file_lbl) lv_label_set_text(docs_file_lbl, "Status: Pending (To be added later)");
            if (docs_info_lbl) lv_label_set_text(docs_info_lbl, "Save your Driving Licence photo as 'dl.jpg'\ninside '/docs/' folder on SD card.");
        }
    }
}

static void doc_tab_rc_cb(lv_event_t* e)     { show_doc(DocType::RC); }
static void doc_tab_dl_cb(lv_event_t* e)     { show_doc(DocType::DL); }
static void doc_tab_aadhar_cb(lv_event_t* e) { show_doc(DocType::AADHAAR); }

static void create_docs_screen() {
    scr_docs = lv_obj_create(nullptr);
    apply_screen_theme(scr_docs);

    // Header Back button (Touch target 48x34)
    lv_obj_t* back = lv_btn_create(scr_docs);
    lv_obj_remove_style_all(back);
    lv_obj_set_size(back, 48, 34);
    lv_obj_set_pos(back, 10, 6);
    lv_obj_set_style_bg_color(back, COL_CARD_BG, 0);
    lv_obj_set_style_border_width(back, 1, 0);
    lv_obj_set_style_border_color(back, COL_CARD_BORDER, 0);
    lv_obj_set_style_radius(back, 10, 0);
    lv_obj_add_event_cb(back, docs_back_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* bl = lv_label_create(back);
    lv_label_set_text(bl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(bl, COL_TEXT_PRI, 0);
    lv_obj_set_style_text_font(bl, &lv_font_montserrat_16, 0);
    lv_obj_center(bl);

    lv_obj_t* title = lv_label_create(scr_docs);
    lv_label_set_text(title, "Bike Documents");
    lv_obj_set_style_text_color(title, COL_TEXT_PRI, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(title, 64, 13);

    // Header Flip Front/Back Button (Touch target 74x32)
    docs_flip_btn = lv_btn_create(scr_docs);
    lv_obj_remove_style_all(docs_flip_btn);
    lv_obj_set_size(docs_flip_btn, 74, 32);
    lv_obj_align(docs_flip_btn, LV_ALIGN_TOP_RIGHT, -10, 7);
    lv_obj_set_style_bg_opa(docs_flip_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(docs_flip_btn, ACCENT_DOCS, 0);
    lv_obj_set_style_radius(docs_flip_btn, 8, 0);
    lv_obj_add_event_cb(docs_flip_btn, docs_flip_cb, LV_EVENT_CLICKED, nullptr);
    docs_flip_lbl = lv_label_create(docs_flip_btn);
    lv_label_set_text(docs_flip_lbl, LV_SYMBOL_REFRESH " Back");
    lv_obj_set_style_text_color(docs_flip_lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(docs_flip_lbl, &lv_font_montserrat_12, 0);
    lv_obj_center(docs_flip_lbl);

    // 3 Segmented Tabs (Width 296, x = 12, y = 44)
    lv_obj_t* tabs = lv_obj_create(scr_docs);
    lv_obj_remove_style_all(tabs);
    lv_obj_set_size(tabs, 296, 36);
    lv_obj_set_pos(tabs, 12, 44);
    lv_obj_set_flex_flow(tabs, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(tabs, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // RC Tab (94x32)
    docs_tab_rc = lv_btn_create(tabs);
    lv_obj_remove_style_all(docs_tab_rc);
    lv_obj_set_size(docs_tab_rc, 94, 32);
    lv_obj_set_style_bg_opa(docs_tab_rc, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(docs_tab_rc, ACCENT_NAV, 0);
    lv_obj_set_style_radius(docs_tab_rc, 8, 0);
    lv_obj_add_event_cb(docs_tab_rc, doc_tab_rc_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* rcl = lv_label_create(docs_tab_rc);
    lv_label_set_text(rcl, "RC Book");
    lv_obj_set_style_text_color(rcl, lv_color_white(), 0);
    lv_obj_set_style_text_font(rcl, &lv_font_montserrat_14, 0);
    lv_obj_center(rcl);

    // DL Tab (94x32)
    docs_tab_dl = lv_btn_create(tabs);
    lv_obj_remove_style_all(docs_tab_dl);
    lv_obj_set_size(docs_tab_dl, 94, 32);
    lv_obj_set_style_bg_opa(docs_tab_dl, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(docs_tab_dl, COL_CARD_BG, 0);
    lv_obj_set_style_border_width(docs_tab_dl, 1, 0);
    lv_obj_set_style_border_color(docs_tab_dl, COL_CARD_BORDER, 0);
    lv_obj_set_style_radius(docs_tab_dl, 8, 0);
    lv_obj_add_event_cb(docs_tab_dl, doc_tab_dl_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* dll = lv_label_create(docs_tab_dl);
    lv_label_set_text(dll, "Driving Lic");
    lv_obj_set_style_text_color(dll, COL_TEXT_PRI, 0);
    lv_obj_set_style_text_font(dll, &lv_font_montserrat_14, 0);
    lv_obj_center(dll);

    // Aadhaar Tab (94x32)
    docs_tab_aadhar = lv_btn_create(tabs);
    lv_obj_remove_style_all(docs_tab_aadhar);
    lv_obj_set_size(docs_tab_aadhar, 94, 32);
    lv_obj_set_style_bg_opa(docs_tab_aadhar, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(docs_tab_aadhar, COL_CARD_BG, 0);
    lv_obj_set_style_border_width(docs_tab_aadhar, 1, 0);
    lv_obj_set_style_border_color(docs_tab_aadhar, COL_CARD_BORDER, 0);
    lv_obj_set_style_radius(docs_tab_aadhar, 8, 0);
    lv_obj_add_event_cb(docs_tab_aadhar, doc_tab_aadhar_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* aal = lv_label_create(docs_tab_aadhar);
    lv_label_set_text(aal, "Aadhaar");
    lv_obj_set_style_text_color(aal, COL_TEXT_PRI, 0);
    lv_obj_set_style_text_font(aal, &lv_font_montserrat_14, 0);
    lv_obj_center(aal);

    // Document Card (296x146, x = 12, y = 86, Clickable for Fullscreen)
    docs_card = lv_obj_create(scr_docs);
    lv_obj_remove_style_all(docs_card);
    lv_obj_set_size(docs_card, 296, 146);
    lv_obj_set_pos(docs_card, 12, 86);
    lv_obj_set_style_bg_color(docs_card, COL_CARD_BG, 0);
    lv_obj_set_style_bg_opa(docs_card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(docs_card, 1, 0);
    lv_obj_set_style_border_color(docs_card, COL_CARD_BORDER, 0);
    lv_obj_set_style_radius(docs_card, 14, 0);
    lv_obj_add_flag(docs_card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(docs_card, docs_card_click_cb, LV_EVENT_CLICKED, nullptr);

    // Scaled Image Widget
    docs_img_obj = lv_img_create(docs_card);
    lv_obj_align(docs_img_obj, LV_ALIGN_CENTER, 0, -4);
    lv_obj_add_flag(docs_img_obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(docs_img_obj, docs_card_click_cb, LV_EVENT_CLICKED, nullptr);

    // Tap Card for Fullscreen Hint Pill
    docs_hint_lbl = lv_label_create(docs_card);
    lv_label_set_text(docs_hint_lbl, "Tap Card for Fullscreen");
    lv_obj_set_style_text_color(docs_hint_lbl, ACCENT_NAV, 0);
    lv_obj_set_style_text_font(docs_hint_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_bg_color(docs_hint_lbl, COL_BG_TOP, 0);
    lv_obj_set_style_bg_opa(docs_hint_lbl, LV_OPA_80, 0);
    lv_obj_set_style_pad_hor(docs_hint_lbl, 8, 0);
    lv_obj_set_style_pad_ver(docs_hint_lbl, 2, 0);
    lv_obj_set_style_radius(docs_hint_lbl, 6, 0);
    lv_obj_align(docs_hint_lbl, LV_ALIGN_BOTTOM_MID, 0, -4);

    // DL / Missing Doc Text Info
    docs_title_lbl = lv_label_create(docs_card);
    lv_label_set_text(docs_title_lbl, "DRIVING LICENCE (DL)");
    lv_obj_set_style_text_color(docs_title_lbl, ACCENT_TRACK, 0);
    lv_obj_set_style_text_font(docs_title_lbl, &lv_font_montserrat_16, 0);
    lv_obj_align(docs_title_lbl, LV_ALIGN_TOP_LEFT, 10, 10);

    docs_sub_lbl = lv_label_create(docs_card);
    lv_label_set_text(docs_sub_lbl, "Holder: Sunny Gupta | MCWG / LMV");
    lv_obj_set_style_text_color(docs_sub_lbl, COL_TEXT_PRI, 0);
    lv_obj_set_style_text_font(docs_sub_lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(docs_sub_lbl, LV_ALIGN_TOP_LEFT, 10, 34);

    docs_file_lbl = lv_label_create(docs_card);
    lv_label_set_text(docs_file_lbl, "Status: Pending (To be added later)");
    lv_obj_set_style_text_color(docs_file_lbl, COL_TEXT_SEC, 0);
    lv_obj_set_style_text_font(docs_file_lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(docs_file_lbl, LV_ALIGN_TOP_LEFT, 10, 58);

    docs_info_lbl = lv_label_create(docs_card);
    lv_label_set_text(docs_info_lbl, "Place 'dl.jpg' into '/docs/' folder on SD card.");
    lv_obj_set_style_text_color(docs_info_lbl, COL_TEXT_SEC, 0);
    lv_obj_set_style_text_font(docs_info_lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(docs_info_lbl, LV_ALIGN_BOTTOM_LEFT, 10, -10);

    // ================================================================
    // FULLSCREEN OVERLAY (320x240 Full Display Native Quality)
    // ================================================================
    docs_fs_overlay = lv_obj_create(scr_docs);
    lv_obj_remove_style_all(docs_fs_overlay);
    lv_obj_set_size(docs_fs_overlay, 320, 240);
    lv_obj_set_pos(docs_fs_overlay, 0, 0);
    lv_obj_set_style_bg_color(docs_fs_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(docs_fs_overlay, LV_OPA_COVER, 0);
    lv_obj_add_flag(docs_fs_overlay, LV_OBJ_FLAG_HIDDEN);

    docs_fs_img = lv_img_create(docs_fs_overlay);
    lv_obj_set_size(docs_fs_img, 320, 240);
    lv_obj_set_pos(docs_fs_img, 0, 0);
    lv_obj_add_flag(docs_fs_img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(docs_fs_img, docs_fs_click_cb, LV_EVENT_CLICKED, nullptr);

    // Floating Exit Button (Top-Left, Touch target 72x34)
    docs_fs_back_btn = lv_btn_create(docs_fs_overlay);
    lv_obj_remove_style_all(docs_fs_back_btn);
    lv_obj_set_size(docs_fs_back_btn, 72, 34);
    lv_obj_set_pos(docs_fs_back_btn, 10, 10);
    lv_obj_set_style_bg_color(docs_fs_back_btn, COL_CARD_BG, 0);
    lv_obj_set_style_bg_opa(docs_fs_back_btn, LV_OPA_90, 0);
    lv_obj_set_style_border_width(docs_fs_back_btn, 1, 0);
    lv_obj_set_style_border_color(docs_fs_back_btn, COL_CARD_BORDER, 0);
    lv_obj_set_style_radius(docs_fs_back_btn, 8, 0);
    lv_obj_add_event_cb(docs_fs_back_btn, docs_fs_click_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* fbl = lv_label_create(docs_fs_back_btn);
    lv_label_set_text(fbl, LV_SYMBOL_LEFT " Exit");
    lv_obj_set_style_text_color(fbl, COL_TEXT_PRI, 0);
    lv_obj_set_style_text_font(fbl, &lv_font_montserrat_12, 0);
    lv_obj_center(fbl);

    // Floating Title (Top-Center)
    docs_fs_title_lbl = lv_label_create(docs_fs_overlay);
    lv_label_set_text(docs_fs_title_lbl, "Vehicle RC Book");
    lv_obj_set_style_text_color(docs_fs_title_lbl, COL_TEXT_PRI, 0);
    lv_obj_set_style_text_font(docs_fs_title_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_bg_color(docs_fs_title_lbl, COL_BG_TOP, 0);
    lv_obj_set_style_bg_opa(docs_fs_title_lbl, LV_OPA_80, 0);
    lv_obj_set_style_pad_hor(docs_fs_title_lbl, 8, 0);
    lv_obj_set_style_pad_ver(docs_fs_title_lbl, 4, 0);
    lv_obj_set_style_radius(docs_fs_title_lbl, 6, 0);
    lv_obj_align(docs_fs_title_lbl, LV_ALIGN_TOP_MID, 0, 12);

    // Floating Flip Button (Top-Right, Touch target 76x34)
    docs_fs_flip_btn = lv_btn_create(docs_fs_overlay);
    lv_obj_remove_style_all(docs_fs_flip_btn);
    lv_obj_set_size(docs_fs_flip_btn, 76, 34);
    lv_obj_align(docs_fs_flip_btn, LV_ALIGN_TOP_RIGHT, -10, 10);
    lv_obj_set_style_bg_color(docs_fs_flip_btn, ACCENT_DOCS, 0);
    lv_obj_set_style_bg_opa(docs_fs_flip_btn, LV_OPA_90, 0);
    lv_obj_set_style_radius(docs_fs_flip_btn, 8, 0);
    lv_obj_add_event_cb(docs_fs_flip_btn, docs_flip_cb, LV_EVENT_CLICKED, nullptr);
    docs_fs_flip_lbl = lv_label_create(docs_fs_flip_btn);
    lv_label_set_text(docs_fs_flip_lbl, LV_SYMBOL_REFRESH " Back");
    lv_obj_set_style_text_color(docs_fs_flip_lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(docs_fs_flip_lbl, &lv_font_montserrat_12, 0);
    lv_obj_center(docs_fs_flip_lbl);

    show_doc(DocType::RC);
}

static void update_docs_screen() {
    // Docs updates are event-driven
}

void ui_docs_show_rc() {
    rc_show_back = false;
    show_doc(DocType::RC);
}

void ui_docs_show_aadhar() {
    aadhar_show_back = false;
    show_doc(DocType::AADHAAR);
}

void ui_docs_show_dl() {
    show_doc(DocType::DL);
}

void ui_docs_flip() {
    docs_flip_cb(nullptr);
}

void ui_docs_enter_fullscreen() {
    docs_enter_fullscreen();
}

void ui_docs_exit_fullscreen() {
    docs_exit_fullscreen();
}

// ================================================================
// 11. QUICK SHADE & BRIGHTNESS OVERLAY
// ================================================================
static void shade_home_cb(lv_event_t* e) {
    ui_shade_hide();
    ui_navigate(Screen::HOME);
}

static void shade_settings_cb(lv_event_t* e) {
    ui_shade_hide();
    ui_navigate(Screen::SETTINGS);
}

static void shade_brightness_change_cb(lv_event_t* e) {
    lv_obj_t* slider = lv_event_get_target(e);
    uint8_t val = (uint8_t)lv_slider_get_value(slider);
    ui_set_brightness(val);
}

static void create_shade() {
    shade_panel = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(shade_panel);
    lv_obj_set_size(shade_panel, 320, 108);
    lv_obj_set_pos(shade_panel, 0, -108);
    lv_obj_set_style_bg_color(shade_panel, COL_BG_TOP, 0);
    lv_obj_set_style_bg_opa(shade_panel, LV_OPA_90, 0);
    lv_obj_set_style_border_width(shade_panel, 1, 0);
    lv_obj_set_style_border_color(shade_panel, ACCENT_NAV, 0);
    lv_obj_set_style_border_side(shade_panel, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_pad_all(shade_panel, 10, 0);

    lv_obj_t* home_btn = lv_btn_create(shade_panel);
    lv_obj_remove_style_all(home_btn);
    lv_obj_set_size(home_btn, 60, 38);
    lv_obj_align(home_btn, LV_ALIGN_LEFT_MID, 4, -12);
    lv_obj_set_style_bg_color(home_btn, ACCENT_NAV, 0);
    lv_obj_set_style_radius(home_btn, 10, 0);
    lv_obj_add_event_cb(home_btn, shade_home_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* hl = lv_label_create(home_btn);
    lv_label_set_text(hl, LV_SYMBOL_HOME);
    lv_obj_set_style_text_color(hl, lv_color_white(), 0);
    lv_obj_set_style_text_font(hl, &lv_font_montserrat_18, 0);
    lv_obj_center(hl);

    lv_obj_t* set_btn = lv_btn_create(shade_panel);
    lv_obj_remove_style_all(set_btn);
    lv_obj_set_size(set_btn, 60, 38);
    lv_obj_align(set_btn, LV_ALIGN_LEFT_MID, 70, -12);
    lv_obj_set_style_bg_color(set_btn, COL_CARD_BG, 0);
    lv_obj_set_style_border_width(set_btn, 1, 0);
    lv_obj_set_style_border_color(set_btn, COL_CARD_BORDER, 0);
    lv_obj_set_style_radius(set_btn, 10, 0);
    lv_obj_add_event_cb(set_btn, shade_settings_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* sl = lv_label_create(set_btn);
    lv_label_set_text(sl, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_color(sl, COL_TEXT_PRI, 0);
    lv_obj_set_style_text_font(sl, &lv_font_montserrat_18, 0);
    lv_obj_center(sl);

    shade_bt_label = lv_label_create(shade_panel);
    lv_label_set_text(shade_bt_label, LV_SYMBOL_BLUETOOTH " RPMLOOP");
    lv_obj_set_style_text_color(shade_bt_label, ACCENT_MAPS, 0);
    lv_obj_set_style_text_font(shade_bt_label, &lv_font_montserrat_12, 0);
    lv_obj_align(shade_bt_label, LV_ALIGN_TOP_RIGHT, -6, 4);

    lv_obj_t* bright_icon = lv_label_create(shade_panel);
    lv_label_set_text(bright_icon, LV_SYMBOL_IMAGE);
    lv_obj_set_style_text_color(bright_icon, ACCENT_NAV, 0);
    lv_obj_align(bright_icon, LV_ALIGN_BOTTOM_LEFT, 6, -6);

    shade_brightness_slider = lv_slider_create(shade_panel);
    lv_obj_set_size(shade_brightness_slider, 260, 10);
    lv_obj_align(shade_brightness_slider, LV_ALIGN_BOTTOM_LEFT, 32, -8);
    lv_slider_set_range(shade_brightness_slider, 20, 255);
    lv_slider_set_value(shade_brightness_slider, 255, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(shade_brightness_slider, COL_CARD_INNER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(shade_brightness_slider, ACCENT_NAV, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(shade_brightness_slider, ACCENT_NAV, LV_PART_KNOB);
    lv_obj_add_event_cb(shade_brightness_slider, shade_brightness_change_cb, LV_EVENT_VALUE_CHANGED, nullptr);
}

static void create_brightness_overlay() {
    brightness_overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(brightness_overlay);
    lv_obj_set_size(brightness_overlay, 320, 240);
    lv_obj_set_pos(brightness_overlay, 0, 0);
    lv_obj_set_style_bg_color(brightness_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(brightness_overlay, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(brightness_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(brightness_overlay, LV_OBJ_FLAG_IGNORE_LAYOUT);
}

// ================================================================
// 12. PUBLIC UI API
// ================================================================
void ui_init() {
    create_splash_screen();
    create_connect_screen();
    create_home_screen();
    create_nav_screen();
    create_maps_screen();
    create_music_screen();
    create_tracking_screen();
    create_settings_screen();
    create_docs_screen();

    create_shade();
    create_brightness_overlay();

    mapRenderer.init(nav_map_container);

    lv_scr_load(scr_splash);
    current_screen = Screen::SPLASH;
}

void ui_navigate(Screen screen) {
    lv_obj_t* target = nullptr;
    switch (screen) {
        case Screen::SPLASH:     target = scr_splash; break;
        case Screen::CONNECT:    target = scr_connect; break;
        case Screen::HOME:       target = scr_home; break;
        case Screen::NAVIGATION: target = scr_nav; break;
        case Screen::MAPS:       target = scr_maps; break;
        case Screen::MUSIC:      target = scr_music; break;
        case Screen::TRACKING:   target = scr_tracking; break;
        case Screen::SETTINGS:   target = scr_settings; break;
        case Screen::DOCS:       target = scr_docs; break;
    }
    if (!target) return;
    if (current_screen == screen && lv_scr_act() == target) return;

    Serial.printf("[UI] ui_navigate -> %d\n", (int)screen);
    lv_scr_load_anim(target, LV_SCR_LOAD_ANIM_FADE_ON, 150, 0, false);
    current_screen = screen;

    if (screen != Screen::DOCS) docs_exit_fullscreen();
    if (shade_is_visible) ui_shade_hide();
}

Screen ui_current_screen() {
    return current_screen;
}

void ui_update() {
    update_connect_screen();
    update_home_screen();
    update_nav_screen();
    update_maps_screen();
    update_music_screen();
    update_tracking_screen();
    update_docs_screen();

    if (shade_is_visible && shade_bt_label) {
        if (rpmBle.isReady()) {
            lv_label_set_text(shade_bt_label, LV_SYMBOL_BLUETOOTH " Connected");
            lv_obj_set_style_text_color(shade_bt_label, ACCENT_MAPS, 0);
        } else {
            lv_label_set_text(shade_bt_label, LV_SYMBOL_BLUETOOTH " Disconnected");
            lv_obj_set_style_text_color(shade_bt_label, COL_TEXT_MUTED, 0);
        }
    }
}

void ui_shade_show() {
    if (!shade_panel || shade_is_visible) return;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, shade_panel);
    lv_anim_set_values(&a, -108, 0);
    lv_anim_set_exec_cb(&a, [](void* obj, int32_t v) {
        lv_obj_set_y((lv_obj_t*)obj, v);
    });
    lv_anim_set_time(&a, 200);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);

    shade_is_visible = true;
}

void ui_shade_hide() {
    if (!shade_panel || !shade_is_visible) return;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, shade_panel);
    lv_anim_set_values(&a, 0, -108);
    lv_anim_set_exec_cb(&a, [](void* obj, int32_t v) {
        lv_obj_set_y((lv_obj_t*)obj, v);
    });
    lv_anim_set_time(&a, 200);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_start(&a);

    shade_is_visible = false;
}

bool ui_shade_visible() {
    return shade_is_visible;
}

void ui_set_brightness(uint8_t level) {
    brightness_level = level;
    if (!brightness_overlay) return;

    uint8_t opa = 255 - level;
    lv_obj_set_style_bg_opa(brightness_overlay, opa, 0);
    deviceState.brightness.level = level;
}

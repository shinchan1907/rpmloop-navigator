// MiniNav-Mini: board.cpp
//
// Hardware: custom board with:
//   Display   : 2.8" ST7789V SPI (320×240) on SPI2_HOST
//   Touch     : HR2046 / XPT2046 resistive SPI on SPI3_HOST
//   Backlight : permanently 3V3 (no GPIO control needed)
//
// Stripped from original MiniNav board.cpp:
//   - QMI8658 IMU (auto-rotation) → removed; rotation fixed to landscape
//   - CST3530 capacitive touch (I2C) → replaced by XPT2046 resistive driver in LovyanGFX
//   - microSD / SDMMC / FatFS     → removed
//   - NFB (4 frame buffers)        → 2 frame buffers (saves 307 KB PSRAM)
//   - PIN_LCD_BL backlight GPIO    → not needed (wired to 3V3)
//
// Pinout (as wired by the user):
//   Display SPI2:
//     SCK  → GPIO 6    MOSI → GPIO 7    MISO → GPIO 8
//     CS   → GPIO 9    DC   → GPIO 10   RST  → GPIO 11
//   Touch SPI3:
//     CLK  → GPIO 1    MOSI → GPIO 2    MISO → GPIO 3
//     IRQ  → GPIO 4    CS   → GPIO 5
//
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <math.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "app.h"
#include "ui_fonts.h"

static const char *TAG = "board";

// ======================= Pin Definitions =====================================================
// Display (SPI2)
#define PIN_LCD_SCK    6
#define PIN_LCD_MOSI   7
#define PIN_LCD_MISO   8
#define PIN_LCD_CS     9
#define PIN_LCD_DC     10
#define PIN_LCD_RST    11

// Touch (SPI3) — HR2046 is electrically identical to XPT2046; LovyanGFX driver works directly
#define PIN_TP_CLK     1
#define PIN_TP_MOSI    2
#define PIN_TP_MISO    3
#define PIN_TP_IRQ     4
#define PIN_TP_CS      5

// ======================= LovyanGFX Display Class ============================================
class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789   _panel;
    lgfx::Bus_SPI        _bus;
    lgfx::Touch_XPT2046  _touch;

public:
    LGFX() {
        // ---- Display SPI bus (SPI2_HOST) ----
        {
            auto b = _bus.config();
            b.spi_host    = SPI2_HOST;
            b.spi_mode    = 0;
            // 40 MHz is safe for jumper-wire connections; bump to 60 MHz only if signal is clean
            b.freq_write  = 40000000;
            b.freq_read   = 16000000;
            b.spi_3wire   = false;   // MISO is connected (needed for touch on SPI2 sharing; but
                                     // we use a separate SPI bus for touch so just leave it here)
            b.use_lock    = true;
            b.dma_channel = SPI_DMA_CH_AUTO;
            b.pin_sclk    = PIN_LCD_SCK;
            b.pin_mosi    = PIN_LCD_MOSI;
            b.pin_miso    = PIN_LCD_MISO;
            b.pin_dc      = PIN_LCD_DC;
            _bus.config(b);
            _panel.setBus(&_bus);
        }

        // ---- ST7789V panel ----
        {
            auto p = _panel.config();
            p.pin_cs       = PIN_LCD_CS;
            p.pin_rst      = PIN_LCD_RST;
            p.pin_busy     = -1;
            p.panel_width  = 240;
            p.panel_height = 320;
            p.offset_x     = 0;
            p.offset_y     = 0;
            p.offset_rotation = 0;
            p.readable     = false;
            p.invert       = false;   // Normal un-inverted colours for daylight & Google Maps
            p.rgb_order    = false;   // BGR
            p.dlen_16bit   = false;
            p.bus_shared   = false;
            _panel.config(p);
        }

        // ---- HR2046 / XPT2046 resistive touch (SPI3_HOST, separate bus) ----
        {
            auto t = _touch.config();
            t.spi_host = SPI3_HOST;
            t.freq     = 1000000;    // 1 MHz — resistive digitisers don't need more
            t.pin_sclk = PIN_TP_CLK;
            t.pin_mosi = PIN_TP_MOSI;
            t.pin_miso = PIN_TP_MISO;
            t.pin_cs   = PIN_TP_CS;
            t.pin_int  = PIN_TP_IRQ;
            t.bus_shared = false;    // dedicated SPI bus, no need to share lock

            // Raw ADC calibration for a typical 2.8" resistive panel.
            // Touch panel mai slight difference ho sakta hai; agar touch offset aaye toh
            // LCD calibration utility se in values ko update karo.
            t.x_min = 200;
            t.x_max = 3900;
            t.y_min = 200;
            t.y_max = 3900;

            // 2.8" panel orientation: x/y may be swapped depending on how the panel is mounted.
            // rotation=1 (landscape) means panel_width becomes the long axis.
            t.offset_rotation = 0;
            _touch.config(t);
            _panel.setTouch(&_touch);
        }

        setPanel(&_panel);
    }
};

static LGFX lcd;

// ======================= Frame Buffers (PSRAM) ===============================================
// 2 frame buffers instead of 4 to fit within 2 MB PSRAM.
//   2 × 320 × 240 × 2 B = 307,200 B ≈ 300 KB
#define NFB 2
#define STRIP_LINES 16

static uint16_t     *s_fb[NFB];
static uint16_t     *s_strip[2];          // internal DMA-capable bounce buffers for display task
static volatile int  s_pending = -1;
static volatile int  s_showing = -1;
static volatile int  s_last    = -1;
static portMUX_TYPE  s_fb_lock = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t  s_display_task;

uint16_t *display_acquire_frame(void) {
    portENTER_CRITICAL(&s_fb_lock);
    int idx = 0;
    for (int i = 0; i < NFB; i++)
        if (i != s_pending && i != s_showing && i != s_last) { idx = i; break; }
    portEXIT_CRITICAL(&s_fb_lock);
    return s_fb[idx];
}

void display_submit_frame(uint16_t *fb) {
    portENTER_CRITICAL(&s_fb_lock);
    for (int i = 0; i < NFB; i++)
        if (s_fb[i] == fb) s_pending = i;
    portEXIT_CRITICAL(&s_fb_lock);
    xTaskNotifyGive(s_display_task);
}

void display_request_status(void) {
    g_app.status_dirty = true;
    if (s_display_task) xTaskNotifyGive(s_display_task);
}

// ======================= Colour Palette ======================================================
struct palette { uint32_t bg0, bg1, card, text, muted, accent, ok, err, pin, track; };

// "MiniNav sky-navy" palette (day / night)
static const palette PAL_DAY   = { 0xF4F7FB, 0xE2EBF7, 0xFFFFFF, 0x23304A, 0x8592A8,
                                    0x4A7FE0, 0x3DBE8B, 0xE5736A, 0xF06E64, 0xDCE4F0 };
static const palette PAL_NIGHT = { 0x10192B, 0x1B2740, 0x1F2D47, 0xE6EBF5, 0x8C9AB5,
                                    0x7AA7FF, 0x6FD3A5, 0xF28B82, 0xFF8A80, 0x344463 };

static const palette &pal(void) { return g_app.night ? PAL_NIGHT : PAL_DAY; }
static uint32_t c888(uint32_t v) { return lgfx::color888(v >> 16, (v >> 8) & 255, v & 255); }

// ======================= UI Drawing (sprite-based) ===========================================
// One PSRAM sprite (320×240) for compositing the splash/status screen.
static lgfx::LGFX_Sprite s_ui;
static uint16_t          *s_ui_buf;

static void draw_rounded_rect(lgfx::LGFX_Sprite &sp, int x, int y, int w, int h,
                               int r, uint32_t col) {
    sp.fillRoundRect(x, y, w, h, r, c888(col));
}

// Draw a tick (✓) at (cx, cy)
static void draw_tick(lgfx::LGFX_Sprite &sp, int cx, int cy, uint32_t col) {
    sp.drawLine(cx - 5, cy,     cx - 1, cy + 4, col);
    sp.drawLine(cx - 1, cy + 4, cx + 6, cy - 4, col);
    sp.drawLine(cx - 5, cy + 1, cx - 1, cy + 5, col);
    sp.drawLine(cx - 1, cy + 5, cx + 6, cy - 3, col);
}

// ---- Splash / status screen ---------------------------------------------------------------
// High-End Automotive UI: Crisp, modern, sophisticated dashboard for motorcycle handlebars
static void draw_status_screen(lgfx::LGFX_Sprite &sp) {
    // 1. Sleek pearl-slate background with subtle dual-tone header band
    sp.fillSprite(c888(0xF6F8FA));

    // Top subtle brand accent bar
    sp.fillRect(0, 0, LCD_W, 3, c888(0x2563EB)); // Royal blue brand stripe

    // 2. Premium Header Card (Floating white card with rounded corners)
    sp.fillRoundRect(8, 7, 304, 38, 7, c888(0xFFFFFF));
    sp.drawRoundRect(8, 7, 304, 38, 7, c888(0xE2E8F0));

    // Modern RPMLOOP Speed Compass Emblem
    sp.fillRoundRect(14, 13, 26, 26, 6, c888(0x0F172A)); // Dark slate pill
    sp.fillTriangle(27, 17, 19, 31, 27, 27, c888(0x38BDF8));
    sp.fillTriangle(27, 17, 35, 31, 27, 27, c888(0x0284C7));

    // Typography: "RPMLOOP" in bold + "COCKPIT NAV" in clean tracked caps
    sp.setTextColor(c888(0x0F172A));
    sp.setFont(&fonts::Font4);
    sp.setTextDatum(lgfx::top_left);
    sp.drawString("RPMLOOP", 46, 12);

    sp.setTextColor(c888(0x64748B));
    sp.setFont(&fonts::Font0);
    sp.drawString("COCKPIT NAV", 172, 16);

    // Live Status Pill (Top Right)
    bool connected = g_app.stage >= STAGE_PHONE;
    bool is_video  = g_app.stage >= STAGE_VIDEO;
    uint32_t status_bg = is_video ? c888(0xDCFCE7) : (connected ? c888(0xDBEAFE) : c888(0xF1F5F9));
    uint32_t status_text = is_video ? c888(0x166534) : (connected ? c888(0x1E40AF) : c888(0x475569));
    uint32_t dot_color   = is_video ? c888(0x16A34A) : (connected ? c888(0x2563EB) : c888(0x94A3B8));

    sp.fillRoundRect(244, 14, 62, 24, 12, status_bg);
    sp.fillCircle(252, 26, 3, dot_color);
    sp.setTextColor(status_text);
    sp.setFont(&fonts::Font0);
    sp.setTextDatum(lgfx::middle_center);
    sp.drawString(is_video ? "LIVE" : (connected ? "LINKED" : "READY"), 278, 26);
    sp.setTextDatum(lgfx::top_left);

    // 3. Elevated Hotspot Credentials Tile (Crisp, High-Contrast)
    sp.fillRoundRect(8, 50, 304, 52, 7, c888(0xFFFFFF));
    sp.drawRoundRect(8, 50, 304, 52, 7, c888(0xE2E8F0));

    // Left Column: Hotspot Name
    sp.setTextColor(c888(0x94A3B8));
    sp.setFont(&fonts::Font0);
    sp.drawString("HOTSPOT SSID", 20, 56);
    sp.setTextColor(c888(0x0F172A));
    sp.setFont(&fonts::Font4);
    sp.drawString(CONFIG_AAM_AP_SSID, 20, 70);

    // Clean hairline vertical separator
    sp.drawLine(152, 56, 152, 96, c888(0xE2E8F0));

    // Right Column: Passkey
    sp.setTextColor(c888(0x94A3B8));
    sp.setFont(&fonts::Font0);
    sp.drawString("WPA2 PASSKEY", 166, 56);
    sp.setTextColor(c888(0x2563EB));
    sp.setFont(&fonts::Font4);
    sp.drawString(CONFIG_AAM_AP_PASSWORD, 166, 70);

    // 4. Milestone Pipeline Stepper (Segmented progress bar)
    int node_x[4] = { 46, 122, 198, 274 };
    int node_y = 126;
    sp.drawLine(node_x[0], node_y, node_x[3], node_y, c888(0xCBD5E1));

    int active_step = 0;
    if (g_app.stage >= STAGE_VIDEO) active_step = 3;
    else if (g_app.stage >= STAGE_TCP) active_step = 2;
    else if (g_app.stage >= STAGE_PHONE) active_step = 1;
    else if (g_app.stage >= STAGE_AP) active_step = 0;

    if (active_step > 0) {
        sp.drawLine(node_x[0], node_y, node_x[active_step], node_y, c888(0x10B981));
    }

    const char *step_names[4] = { "WI-FI", "PHONE", "LINK", "MAPS" };
    for (int i = 0; i < 4; i++) {
        int nx = node_x[i];
        bool done = active_step > i || (active_step == 3 && i == 3);
        bool current = active_step == i && active_step != 3;

        if (done) {
            sp.fillCircle(nx, node_y, 13, c888(0x10B981)); // Clean Emerald
            draw_tick(sp, nx, node_y - 1, c888(0xFFFFFF));
        } else if (current) {
            sp.fillCircle(nx, node_y, 15, c888(0x2563EB)); // Royal Blue halo
            sp.fillCircle(nx, node_y, 6, c888(0xFFFFFF));
        } else {
            sp.fillCircle(nx, node_y, 12, c888(0xFFFFFF));
            sp.drawCircle(nx, node_y, 12, c888(0xCBD5E1));
        }

        sp.setTextColor(c888(done ? 0x059669 : (current ? 0x2563EB : 0x94A3B8)));
        sp.setFont(&fonts::Font0);
        sp.setTextDatum(lgfx::middle_center);
        sp.drawString(step_names[i], nx, node_y + 22);
        sp.setTextDatum(lgfx::top_left);
    }

    // 5. Intelligent Guidance Card (Elevated floating pill)
    bool has_err = g_app.error[0] != 0;
    uint32_t card_bg = has_err ? c888(0xFEF2F2) : c888(0xFFFFFF);
    uint32_t card_border = has_err ? c888(0xFECACA) : c888(0xE2E8F0);
    sp.fillRoundRect(8, 168, 304, 64, 7, card_bg);
    sp.drawRoundRect(8, 168, 304, 64, 7, card_border);

    if (has_err) {
        sp.setTextColor(c888(0xDC2626));
        sp.setFont(&fonts::Font2);
        sp.drawString("Session Alert", 20, 174);
        sp.setFont(&fonts::Font0);
        sp.setTextColor(c888(0xB91C1C));
        sp.drawString(g_app.error, 20, 194);
        sp.drawString("Tip: Restart 'Headunit server' on phone", 20, 210);
    } else {
        sp.setFont(&fonts::Font2);
        if (g_app.stage == STAGE_AP) {
            sp.setTextColor(c888(0x1E40AF));
            sp.drawString("Waiting for Connection...", 20, 174);
            sp.setFont(&fonts::Font0);
            sp.setTextColor(c888(0x475569));
            sp.drawString("Connect phone Wi-Fi to 'RPL Loop'", 20, 196);
            sp.drawString("Then tap 'Start headunit server' in Android Auto", 20, 210);
        } else if (g_app.stage == STAGE_PHONE || g_app.stage == STAGE_TCP) {
            sp.setTextColor(c888(0x0284C7));
            sp.drawString("Phone Linked - Initializing...", 20, 174);
            sp.setFont(&fonts::Font0);
            sp.setTextColor(c888(0x475569));
            sp.drawString("Securing TLS link with Android Auto", 20, 196);
            if (g_app.phone_ip[0]) sp.drawString(g_app.phone_ip, 20, 210);
        } else if (g_app.stage >= STAGE_VERSION && g_app.stage < STAGE_VIDEO) {
            sp.setTextColor(c888(0x16A34A));
            sp.drawString("Launching Google Maps...", 20, 174);
            sp.setFont(&fonts::Font0);
            sp.setTextColor(c888(0x15803D));
            sp.drawString("Preparing video pipeline & touch digitizer", 20, 196);
        } else {
            sp.setTextColor(c888(0x16A34A));
            sp.drawString("Navigation Active", 20, 174);
            sp.setFont(&fonts::Font0);
            sp.setTextColor(c888(0x15803D));
            sp.drawString("Live Google Maps streaming on display", 20, 196);
        }
    }
}

// ---- Toast banner -------------------------------------------------------------------------
static char     s_toast[64];
static uint32_t s_toast_until;

void display_toast(const char *text) {
    strlcpy(s_toast, text, sizeof s_toast);
    s_toast_until = xTaskGetTickCount() + pdMS_TO_TICKS(1500);
    display_request_status();
}

static void draw_toast(lgfx::LGFX_Sprite &sp) {
    if (!s_toast[0]) return;
    if (xTaskGetTickCount() > s_toast_until) { s_toast[0] = 0; return; }
    int tw = sp.textWidth(s_toast) + 20;
    int tx = (LCD_W - tw) / 2;
    draw_rounded_rect(sp, tx, 4, tw, 22, 6, pal().accent);
    sp.setTextColor(0xFFFFFF);
    sp.setFont(&fonts::Font2);
    sp.setTextDatum(lgfx::middle_center);
    sp.drawString(s_toast, LCD_W / 2, 15);
    sp.setTextDatum(lgfx::top_left);
}

static void draw_toast_direct(void) {
    if (!s_toast[0]) return;
    if (xTaskGetTickCount() > s_toast_until) { s_toast[0] = 0; return; }
    int tw = lcd.textWidth(s_toast) + 20;
    int tx = (LCD_W - tw) / 2;
    lcd.fillRoundRect(tx, 4, tw, 22, 6, c888(pal().accent));
    lcd.setTextColor(0xFFFFFF);
    lcd.setFont(&fonts::Font2);
    lcd.setTextDatum(lgfx::middle_center);
    lcd.drawString(s_toast, LCD_W / 2, 15);
    lcd.setTextDatum(lgfx::top_left);
}

// ---- Console dump -------------------------------------------------------------------------
void display_dump(void) {
    if (s_last < 0) return;
    uint16_t *fb = s_fb[s_last];
    for (int y = 0; y < LCD_H; y++) {
        char row[LCD_W * 5 + 4];
        int pos = 0;
        for (int x = 0; x < LCD_W; x++)
            pos += snprintf(row + pos, sizeof row - pos, "%04X ", fb[y * LCD_W + x]);
        printf("%s\n", row);
    }
}

// ---- Display task (pinned to Core 1, but runs mostly idle waiting for frames) ---------------
static void display_task(void *) {
    for (;;) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50));

        // Show a new video frame if one is pending
        bool have_frame = false;
        portENTER_CRITICAL(&s_fb_lock);
        if (s_pending >= 0) {
            s_showing = s_pending;
            s_pending = -1;
            have_frame = true;
        }
        portEXIT_CRITICAL(&s_fb_lock);

        if (have_frame) {
            // Push the frame in STRIP_LINES strips via internal DMA bounce buffers
            uint16_t *src = s_fb[s_showing];
            for (int y = 0; y < LCD_H; y += STRIP_LINES) {
                int lines = (y + STRIP_LINES <= LCD_H) ? STRIP_LINES : (LCD_H - y);
                uint16_t *buf = s_strip[(y / STRIP_LINES) & 1];
                memcpy(buf, src + y * LCD_W, lines * LCD_W * 2);
                lcd.pushImage(0, y, LCD_W, lines, buf);
            }

            // If there's a toast, overlay it on top
            if (s_toast[0]) {
                draw_toast_direct();
            }

            portENTER_CRITICAL(&s_fb_lock);
            s_last    = s_showing;
            s_showing = -1;
            portEXIT_CRITICAL(&s_fb_lock);
            continue;
        }

        // No new frame: redraw status/splash if dirty
        if (g_app.status_dirty && g_app.stage < STAGE_VIDEO) {
            g_app.status_dirty = false;
            draw_status_screen(s_ui);
            draw_toast(s_ui);
            s_ui.pushSprite(&lcd, 0, 0);
        } else if (s_toast[0] && s_last >= 0) {
            // Overlay toast on the last video frame
            draw_toast_direct();
        } else if (g_app.status_dirty) {
            g_app.status_dirty = false;
            // Video is live: if toast expired, nothing to draw
        }
    }
}

// ======================= Touch (HR2046 / XPT2046 via LovyanGFX) ==============================
// LovyanGFX's Touch_XPT2046 handles the SPI comms and IRQ polling internally.
// We just call lcd.getTouch() and forward the coordinates to Android Auto.

static void touch_poll(void) {
    if (g_app.stage < STAGE_VIDEO) return;
    if (!g_app.touch_enabled)      return;

    static bool     was_pressed = false;
    static uint32_t t_last_move = 0;

    uint16_t tx = 0, ty = 0;
    bool pressed = lcd.getTouch(&tx, &ty);

    if (pressed) {
        // Scale 320×240 screen coordinates → 800×480 Android Auto UI space
        // g_video_crop_x is the left offset of our 640-wide window in the 800-wide frame
        int aa_x = (int)tx * 2 + (g_video_crop_x - g_video_margin_w / 2);
        int aa_y = (int)ty * 2;

        if (!was_pressed) {
            was_pressed = true;
            t_last_move = xTaskGetTickCount();
            ESP_LOGI(TAG, "Touch DOWN: LCD=(%d, %d) -> AA=(%d, %d)", tx, ty, aa_x, aa_y);
            aap_send_touch(0, aa_x, aa_y);   // touch down
        } else {
            uint32_t now = xTaskGetTickCount();
            if (now - t_last_move >= pdMS_TO_TICKS(33)) {
                t_last_move = now;
                aap_send_touch(2, aa_x, aa_y);   // touch move (~30 Hz)
            }
        }
    } else if (was_pressed) {
        was_pressed = false;
        aap_send_touch(1, 0, 0);   // touch up
    }
}

// Status-screen touch: single tap advances night/touch mode when not in video
static void status_touch_poll(void) {
    if (g_app.stage >= STAGE_VIDEO) return;

    static bool     ui_down = false;
    static uint32_t t_last  = 0;

    uint16_t tx = 0, ty = 0;
    bool pressed = lcd.getTouch(&tx, &ty);
    uint32_t now = xTaskGetTickCount();

    if (pressed && !ui_down) {
        ui_down = true;
        t_last  = now;
        // Tap lower half of screen → toggle night mode on splash screen
        if (ty > LCD_H / 2) app_toggle_night("screen tap");
    }
    if (!pressed) {
        if (ui_down && now - t_last > pdMS_TO_TICKS(300)) ui_down = false;
        if (!pressed) ui_down = false;
    }
}

// ======================= board_poll_inputs ====================================================
void board_poll_inputs(void) {
    touch_poll();
    status_touch_poll();
}

// ======================= board_init ===========================================================
void board_init(void) {
    lcd.init();
    lcd.setRotation(1);   // 1 = landscape (320 wide, 240 tall); 3 = landscape flipped
    lcd.setBrightness(255);   // no-op if no backlight pin configured, harmless

    // Frame buffers in PSRAM
    for (int i = 0; i < NFB; i++) {
        s_fb[i] = (uint16_t *)heap_caps_calloc(LCD_W * LCD_H, sizeof(uint16_t), MALLOC_CAP_SPIRAM);
        if (!s_fb[i]) {
            ESP_LOGE(TAG, "PSRAM alloc failed for framebuffer %d", i);
            abort();
        }
    }

    // UI sprite in PSRAM: share s_fb[0] to save 153.6 KB PSRAM for H.264 decoder
    s_ui_buf = s_fb[0];
    s_ui.setColorDepth(16);
    s_ui.setBuffer(s_ui_buf, LCD_W, LCD_H, 16);

    // Bounce strips in internal DMA-capable RAM
    for (int i = 0; i < 2; i++) {
        s_strip[i] = (uint16_t *)heap_caps_malloc(LCD_W * STRIP_LINES * sizeof(uint16_t),
                                                    MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
        if (!s_strip[i]) { ESP_LOGE(TAG, "DMA strip alloc failed"); abort(); }
    }

    ESP_LOGI(TAG, "display init OK (ST7789V 320x240, HR2046 touch)");
    ESP_LOGI(TAG, "PSRAM free after board_init: %u KB",
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) >> 10));

    g_app.status_dirty = true;
    xTaskCreatePinnedToCore(display_task, "display", 6144, NULL, 6, &s_display_task, 1);
}

// sd_ready: always false (no SD card code compiled)
bool sd_ready(void) { return false; }

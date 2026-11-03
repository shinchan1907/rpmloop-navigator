// MiniNav-Mini: video.cpp
// Changes from original MiniNav:
//   - (testing/lag-fix) AU_SLOTS 3, AU_CAP 192 KB, stream declared 480x320,
//     non-blocking AU drop - see git history for the earlier 4->2 / 512 KB story
//   - Dual-task decoder DISABLED (CONFIG_ESP_H264_DUAL_TASK not set); single video task on Core 1
//   - demo mode fully removed (no demo.h264, no from_phone==false paths)
//   - SD card dump code removed
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_attr.h"
#include "esp_h264_dec_sw.h"
#include "esp_h264_dec_param.h"
#include "app.h"

static const char *TAG = "video";

// ---- Memory budget for 2 MB PSRAM -----------------------------------------------------------
// testing/lag-fix: 3 slots x 192 KB = 576 KB. (Was 2 x 96 KB: at 800x480 a detailed
// map keyframe routinely exceeds 96 KB and was dropped whole, freezing the picture
// until a small enough keyframe arrived - a major source of the on-road lag.)
#define AU_SLOTS 3
#define AU_CAP   (192 * 1024)

typedef struct {
    uint8_t *data;
    int      len;
} au_slot_t;

static au_slot_t     s_slots[AU_SLOTS];
static QueueHandle_t s_free_q, s_ready_q;
static volatile bool s_need_idr  = true;
static volatile bool s_reset_req = false;
static uint8_t       s_codec_cfg[1024];
static volatile int  s_codec_cfg_len;
static volatile bool s_codec_cfg_new;
static portMUX_TYPE  s_cfg_lock = portMUX_INITIALIZER_UNLOCKED;
static DRAM_ATTR uint8_t s_clamp[1024];   // s_clamp[512 + v] = clamp(v, 0, 255)

// Stats read by app_main
uint32_t g_vid_frames, g_vid_dropped, g_vid_decode_us, g_vid_scale_us, g_vid_errors;

// ---- H.264 helpers --------------------------------------------------------------------------
static bool has_idr(const uint8_t *p, int n) {
    for (int i = 0; i + 4 < n; i++) {
        if (p[i] == 0 && p[i+1] == 0 && (p[i+2] == 1 || (p[i+2] == 0 && p[i+3] == 1))) {
            int k    = p[i+2] == 1 ? i+3 : i+4;
            int type = p[k] & 0x1f;
            if (type == 5) return true;
            if (type == 1) return false;
            i = k;
        }
    }
    return false;
}

static bool is_annexb(const uint8_t *p, int n) {
    return n >= 4 && p[0] == 0 && p[1] == 0 && (p[2] == 1 || (p[2] == 0 && p[3] == 1));
}

static void avcc_to_annexb(uint8_t *p, int n) {
    int i = 0;
    while (i + 4 <= n) {
        uint32_t len = ((uint32_t)p[i] << 24) | (p[i+1] << 16) | (p[i+2] << 8) | p[i+3];
        p[i] = 0; p[i+1] = 0; p[i+2] = 0; p[i+3] = 1;
        if (len == 0 || i + 4 + (int)len > n) break;
        i += 4 + len;
    }
}

// ---- Public: receive an access unit from the network task -----------------------------------
bool video_push_au(const uint8_t *data, int len, bool from_phone) {
    if (!from_phone) return true;  // demo removed; ignore non-phone frames
    if (len <= 0 || len > AU_CAP) { g_vid_dropped++; return false; }

    bool idr = has_idr(data, len);
    if (s_need_idr && !idr) {
        g_vid_dropped++;
        aap_request_keyframe();
        return false;
    }

    int idx;
    // testing/lag-fix: never wait for a slot - this runs on the network task and
    // waiting stalls ACKs/touch and lets stale video pile up in the socket. If the
    // decoder is behind, shed this AU at once and resync on the next keyframe.
    if (xQueueReceive(s_free_q, &idx, 0) != pdTRUE) {
        g_vid_dropped++;
        s_need_idr = true;
        ESP_LOGW(TAG, "decoder busy, AU shed; requesting keyframe");
        aap_request_keyframe();
        return false;
    }
    au_slot_t *s = &s_slots[idx];
    memcpy(s->data, data, len);
    if (!is_annexb(s->data, len)) avcc_to_annexb(s->data, len);
    s->len = len;
    if (idr) s_need_idr = false;
    xQueueSend(s_ready_q, &idx, portMAX_DELAY);
    return true;
}

void video_set_codec_config(const uint8_t *data, int len) {
    if (len > (int)sizeof s_codec_cfg) return;
    portENTER_CRITICAL(&s_cfg_lock);
    memcpy(s_codec_cfg, data, len);
    s_codec_cfg_len = len;
    s_codec_cfg_new = true;
    portEXIT_CRITICAL(&s_cfg_lock);
}

void video_reset(void) {
    s_reset_req = true;
    s_need_idr  = true;
}

// ---- Scaler: 640×480 crop → 320×240 (2:1 luma average, BT.601 YUV→RGB565 byte-swapped) -----
static SemaphoreHandle_t s_job_go, s_job_done;
static const uint8_t *s_yuv_ptr;
static int   s_src_w, s_src_h, s_cx, s_cy;
static uint16_t *s_fb_out;

static inline uint16_t IRAM_ATTR yuv_to_565_swapped(int y, int u, int v) {
    int c = (y - 16) * 298 + 128;
    int d = u - 128, e = v - 128;
    int r = s_clamp[512 + ((c + 409 * e) >> 8)];
    int g = s_clamp[512 + ((c - 100 * d - 208 * e) >> 8)];
    int b = s_clamp[512 + ((c + 516 * d) >> 8)];
    uint16_t px = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    return (px >> 8) | (px << 8);   // byte-swap for little-endian SPI DMA
}

// Fast path: exact 2:1 crop of 640×480 → 320×240 from within the 800×480 decoded frame.
// The scale_helper_task processes the top half on Core 0 while the video task handles the bottom
// half on Core 1 — both run in parallel for ~30 % speed gain even without dual h264 task.
static void IRAM_ATTR scale_half(const uint8_t *yuv, int w, int h,
                                  int cx, int cy, uint16_t *fb,
                                  int row_start, int row_end) {
    int yw = w, cw2 = w / 2;
    for (int oy = row_start; oy < row_end; oy++) {
        int sy = cy + oy * 2;
        for (int ox = 0; ox < LCD_W; ox++) {
            int sx = cx + ox * 2;
            int y0 = yuv[sy * yw + sx];
            int y1 = yuv[sy * yw + sx + 1];
            int y2 = yuv[(sy+1) * yw + sx];
            int y3 = yuv[(sy+1) * yw + sx + 1];
            int yy = (y0 + y1 + y2 + y3 + 2) >> 2;
            int ux = (cx + ox * 2) / 2, uy = (cy + oy * 2) / 2;
            int u = yuv[yw * h + uy * cw2 + ux];
            int v = yuv[yw * h + cw2 * (h / 2) + uy * cw2 + ux];
            fb[oy * LCD_W + ox] = yuv_to_565_swapped(yy, u, v);
        }
    }
}

// Helper task runs on Core 0, scales top half while Core 1 does bottom half
static void scale_helper_task(void *) {
    for (;;) {
        xSemaphoreTake(s_job_go, portMAX_DELAY);
        scale_half(s_yuv_ptr, s_src_w, s_src_h, s_cx, s_cy, s_fb_out, 0, LCD_H / 2);
        xSemaphoreGive(s_job_done);
    }
}

// Generic scaler for non-640×480 source sizes (fallback)
static void scale_any(const uint8_t *yuv, int w, int h,
                       int cx, int cy, int cw, int ch, uint16_t *fb) {
    for (int oy = 0; oy < LCD_H; oy++) {
        for (int ox = 0; ox < LCD_W; ox++) {
            int sx = cx + ox * cw / LCD_W;
            int sy = cy + oy * ch / LCD_H;
            int yy = yuv[sy * w + sx];
            int ux = sx / 2, uy = sy / 2;
            int u  = yuv[w * h + uy * (w / 2) + ux];
            int v  = yuv[w * h + (w / 2) * (h / 2) + uy * (w / 2) + ux];
            fb[oy * LCD_W + ox] = yuv_to_565_swapped(yy, u, v);
        }
    }
}

static void show_frame(const uint8_t *yuv, int w, int h) {
    int64_t t0 = esp_timer_get_time();
    uint16_t *fb = display_acquire_frame();

    int cx, cy, cw, ch;
    if (w == 800 && h == 480) {
        // Exact MiniNav layout: 640×480 UI area inside 800×480, cropped from crop_x
        cw = 640; ch = 480;
        cx = g_video_crop_x; cy = 0;
        // Parallel scale: Core 0 does top half, Core 1 does bottom half
        s_yuv_ptr = yuv; s_src_w = w; s_src_h = h; s_cx = cx; s_cy = cy; s_fb_out = fb;
        xSemaphoreGive(s_job_go);
        scale_half(yuv, w, h, cx, cy, fb, LCD_H / 2, LCD_H);
        xSemaphoreTake(s_job_done, portMAX_DELAY);
    } else {
        // Any other resolution: centred 4:3 crop
        cw = w; ch = w * 3 / 4;
        if (ch > h) { ch = h; cw = h * 4 / 3; }
        cx = ((w - cw) / 2) & ~3;
        cy = ((h - ch) / 2) & ~1;
        scale_any(yuv, w, h, cx, cy, cw, ch, fb);
    }
    g_vid_scale_us += esp_timer_get_time() - t0;
    display_submit_frame(fb);
}

// ---- H.264 decoder --------------------------------------------------------------------------
static esp_h264_dec_handle_t      s_dec;
static esp_h264_dec_param_sw_handle_t s_dec_param;

static bool dec_open(void) {
    esp_h264_dec_cfg_sw_t cfg = {};
    cfg.pic_type = ESP_H264_RAW_FMT_I420;
    if (esp_h264_dec_sw_new(&cfg, &s_dec) != ESP_H264_ERR_OK) {
        ESP_LOGE(TAG, "decoder new failed"); return false;
    }
    if (esp_h264_dec_open(s_dec) != ESP_H264_ERR_OK) {
        ESP_LOGE(TAG, "decoder open failed"); return false;
    }
    esp_h264_dec_sw_get_param_hd(s_dec, &s_dec_param);
    return true;
}

static void dec_close(void) {
    if (!s_dec) return;
    esp_h264_dec_close(s_dec);
    esp_h264_dec_del(s_dec);
    s_dec = NULL;
}

static bool decode(uint8_t *data, int len, bool show) {
    esp_h264_dec_in_frame_t in = {};
    in.raw_data.buffer = data;
    in.raw_data.len    = len;
    while (in.raw_data.len > 0) {
        esp_h264_dec_out_frame_t out = {};
        int64_t t0 = esp_timer_get_time();
        esp_h264_err_t err = esp_h264_dec_process(s_dec, &in, &out);
        g_vid_decode_us += esp_timer_get_time() - t0;
        if (err != ESP_H264_ERR_OK) { ESP_LOGW(TAG, "decode error %d", err); return false; }
        if (in.consume == 0) break;
        in.raw_data.buffer += in.consume;
        in.raw_data.len    -= in.consume;
        if (out.out_size > 0 && show) {
            esp_h264_resolution_t res = {};
            esp_h264_dec_get_resolution(s_dec_param, &res);
            static int lw, lh;
            if (res.width != lw || res.height != lh) {
                lw = res.width; lh = res.height;
                ESP_LOGI(TAG, "stream %dx%d", lw, lh);
            }
            g_vid_frames++;
            show_frame(out.outbuf, res.width, res.height);
        }
    }
    return true;
}

// ---- Decoder task (pinned to Core 1) --------------------------------------------------------
static void video_task(void *) {
    dec_open();
    for (;;) {
        int idx;
        if (xQueueReceive(s_ready_q, &idx, pdMS_TO_TICKS(500)) != pdTRUE) {
            if (s_reset_req) { s_reset_req = false; dec_close(); dec_open(); }
            continue;
        }
        au_slot_t *s = &s_slots[idx];
        aap_queue_video_ack();   // pipeline: next frame travels over WiFi while this one decodes

        if (s_reset_req) { s_reset_req = false; dec_close(); dec_open(); s_codec_cfg_new = true; }

        static uint8_t *cfg_buf = nullptr;
        if (!cfg_buf)
            cfg_buf = (uint8_t *)heap_caps_malloc(1024, MALLOC_CAP_SPIRAM);
        int cfg_len = 0;
        portENTER_CRITICAL(&s_cfg_lock);
        if (s_codec_cfg_new) {
            memcpy(cfg_buf, s_codec_cfg, s_codec_cfg_len);
            cfg_len = s_codec_cfg_len;
            s_codec_cfg_new = false;
        }
        portEXIT_CRITICAL(&s_cfg_lock);
        if (cfg_len) decode(cfg_buf, cfg_len, false);

        if (!decode(s->data, s->len, true)) {
            g_vid_errors++;
            dec_close(); dec_open();
            s_need_idr = true;
            s_codec_cfg_new = true;
            aap_request_keyframe();
        }
        xQueueSend(s_free_q, &idx, portMAX_DELAY);
    }
}

void video_start(void) {
    // Build the clamp LUT
    for (int i = 0; i < 1024; i++) {
        int v = i - 512;
        s_clamp[i] = v < 0 ? 0 : v > 255 ? 255 : v;
    }

    s_free_q  = xQueueCreate(AU_SLOTS, sizeof(int));
    s_ready_q = xQueueCreate(AU_SLOTS, sizeof(int));
    for (int i = 0; i < AU_SLOTS; i++) {
        s_slots[i].data = (uint8_t *)heap_caps_malloc(AU_CAP, MALLOC_CAP_SPIRAM);
        if (!s_slots[i].data) {
            ESP_LOGE(TAG, "PSRAM alloc failed for AU slot %d (%u KB)", i, AU_CAP >> 10);
            abort();
        }
        xQueueSend(s_free_q, &i, 0);
    }

    s_job_go   = xSemaphoreCreateBinary();
    s_job_done = xSemaphoreCreateBinary();

    // scale_helper_task runs on Core 0 alongside the main loop (low CPU when no video)
    xTaskCreatePinnedToCore(scale_helper_task, "scale", 3072, NULL, 5, NULL, 0);
    // Decoder task pinned to Core 1
    xTaskCreatePinnedToCore(video_task, "video", 8192, NULL, 5, NULL, 1);
}

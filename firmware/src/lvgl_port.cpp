// LVGL display/touch port implementation for RPMLOOP Navigator
// Bridges LovyanGFX ↔ LVGL 8.3.11
//
// Display: 320x240 landscape (ST7789V rotation=1)
// Touch: HR2046 interrupt-driven via T_IRQ (GPIO4)
// Buffer: Double-buffered in PSRAM for smooth rendering

#include "lvgl_port.h"
#include <Arduino.h>

// The global LovyanGFX display object
LGFX tft;

// Display buffer size: 320 * 40 lines = 12800 pixels * 2 bytes = 25600 bytes
// Two buffers for double-buffering, allocated in PSRAM
static const uint32_t DISP_BUF_ROWS = 40;
static const uint32_t DISP_BUF_SIZE = 320 * DISP_BUF_ROWS;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t* buf1 = nullptr;
static lv_color_t* buf2 = nullptr;
static uint16_t* screen_mirror = nullptr;

static lv_disp_drv_t disp_drv;
static lv_indev_drv_t indev_drv;
static lv_disp_t* disp = nullptr;

// ---- Display flush callback ----
static void disp_flush_cb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.writePixels((lgfx::swap565_t*)&color_p->full, w * h);
    tft.endWrite();

    // Mirror to PSRAM for lossless serial screenshot capture
    if (screen_mirror) {
        for (uint32_t r = 0; r < h; r++) {
            uint32_t dst_offset = (area->y1 + r) * 320 + area->x1;
            uint32_t src_offset = r * w;
            memcpy(&screen_mirror[dst_offset], &color_p[src_offset].full, w * sizeof(uint16_t));
        }
    }

    lv_disp_flush_ready(drv);
}

// ---- Touch read callback (polled via SPI3) ----
static void touch_read_cb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    uint16_t tx, ty;
    if (tft.getTouch(&tx, &ty)) {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = (int16_t)tx;
        data->point.y = (int16_t)ty;
        static uint32_t last_log = 0;
        if (millis() - last_log > 250) {
            last_log = millis();
            Serial.printf("[Touch] Pressed: x=%u, y=%u\n", tx, ty);
        }
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

void lvgl_port_init(void) {
    // Ensure SD CS (GPIO13) is HIGH before touching display SPI bus
    pinMode(13, OUTPUT);
    digitalWrite(13, HIGH);

    // Initialize display hardware
    tft.init();
    tft.setRotation(1);  // Landscape: 320x240
    tft.fillScreen(TFT_BLACK);
    tft.waitDisplay();
    tft.setBrightness(255);

    // Initialize LVGL
    lv_init();

    // Allocate display buffers in PSRAM
    buf1 = (lv_color_t*)ps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t));
    buf2 = (lv_color_t*)ps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t));
    if (!buf1 || !buf2) {
        // Fallback to single buffer in regular RAM if PSRAM fails
        if (buf1) free(buf1);
        if (buf2) free(buf2);
        buf1 = (lv_color_t*)malloc(DISP_BUF_SIZE * sizeof(lv_color_t));
        buf2 = nullptr;
        lv_disp_draw_buf_init(&draw_buf, buf1, nullptr, DISP_BUF_SIZE);
    } else {
        lv_disp_draw_buf_init(&draw_buf, buf1, buf2, DISP_BUF_SIZE);
    }

    // Display driver
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = 320;
    disp_drv.ver_res  = 240;
    disp_drv.flush_cb = disp_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    disp = lv_disp_drv_register(&disp_drv);

    // Allocate 320x240 capture mirror in PSRAM
    screen_mirror = (uint16_t*)ps_malloc(320 * 240 * sizeof(uint16_t));
    if (screen_mirror) {
        memset(screen_mirror, 0, 320 * 240 * sizeof(uint16_t));
    }

    // Input driver (touch)
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read_cb;
    lv_indev_drv_register(&indev_drv);
}

void lvgl_port_update(void) {
    lv_timer_handler();
}

lv_disp_t* lvgl_port_get_disp(void) {
    return disp;
}

void lvgl_port_dump_bmp(Stream& s) {
    if (!screen_mirror) return;
    uint8_t bmpHeader[54] = {
        0x42, 0x4D,             // 'BM'
        0x36, 0x84, 0x03, 0x00, // file size: 230454
        0x00, 0x00, 0x00, 0x00, // reserved
        0x36, 0x00, 0x00, 0x00, // offset to pixel data (54)
        0x28, 0x00, 0x00, 0x00, // header size (40)
        0x40, 0x01, 0x00, 0x00, // width: 320
        0xF0, 0x00, 0x00, 0x00, // height: 240
        0x01, 0x00,             // planes: 1
        0x18, 0x00,             // bpp: 24
        0x00, 0x00, 0x00, 0x00, // compression: 0 (BI_RGB)
        0x00, 0x84, 0x03, 0x00, // image size: 230400
        0x12, 0x0B, 0x00, 0x00, // x pixels per meter (2834)
        0x12, 0x0B, 0x00, 0x00, // y pixels per meter (2834)
        0x00, 0x00, 0x00, 0x00, // colors used
        0x00, 0x00, 0x00, 0x00  // important colors
    };
    s.write(bmpHeader, 54);

    uint8_t rowBuf[320 * 3];
    // BMP is bottom-to-top
    for (int y = 239; y >= 0; y--) {
        int rowIdx = 0;
        for (int x = 0; x < 320; x++) {
            uint16_t raw = screen_mirror[y * 320 + x];
            // LV_COLOR_16_SWAP: byte 0 is high, byte 1 is low, so swap
            uint16_t c = (raw >> 8) | (raw << 8);
            uint8_t r = ((c >> 11) & 0x1F) * 255 / 31;
            uint8_t g = ((c >> 5) & 0x3F) * 255 / 63;
            uint8_t b = (c & 0x1F) * 255 / 31;
            rowBuf[rowIdx++] = b;
            rowBuf[rowIdx++] = g;
            rowBuf[rowIdx++] = r;
        }
        s.write(rowBuf, 960);
    }
}


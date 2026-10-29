#include <Arduino.h>
#include "esp_timer.h"
#include "lvgl_port.h"
#include "pinout_config.h"

LGFX tft;

// Two 320x40 draw buffers in internal DMA-capable RAM (double buffering).
static constexpr uint32_t kBufPx = TFT_LCD_W * 40;
static lv_color_t s_buf1[kBufPx];
static lv_color_t s_buf2[kBufPx];
static lv_disp_draw_buf_t s_drawBuf;

// ---------------------------------------------------------------- display ---
static void flush_cb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
    uint32_t w = (uint32_t)(area->x2 - area->x1 + 1);
    uint32_t h = (uint32_t)(area->y2 - area->y1 + 1);
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.writePixelsDMA((uint16_t*)color_p, w * h);
    tft.endWrite();
    lv_disp_flush_ready(drv);
}

static void tick_cb(void* arg) {
    (void)arg;
    lv_tick_inc(5);
}

// ------------------------------------------------------------------ touch ---
// The HR2046 is polled through LovyanGFX: getTouch() returns the point
// already converted into rotated screen coordinates (320x240) using the
// calibration ranges from pinout_config.h. Keep reporting the last point on
// release so LVGL sees a sane press->release pair at one spot.
static int16_t s_lastTouchX = 0;
static int16_t s_lastTouchY = 0;

static void touch_read_cb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    (void)drv;
    int16_t x = 0, y = 0;
    if (tft.getTouch(&x, &y) > 0) {
        s_lastTouchX = x;
        s_lastTouchY = y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
    data->point.x = (lv_coord_t)s_lastTouchX;
    data->point.y = (lv_coord_t)s_lastTouchY;
}

// -------------------------------------------------------------------- init --
void lvgl_port_init(void) {
    // Display + touch (LGFX init also initialises the attached XPT2046).
    tft.init();
    tft.setRotation(TFT_ROTATION);          // bench-proven landscape (5)
    tft.setBrightness(255);                 // no Light fitted: harmless no-op

    lv_init();
    lv_disp_draw_buf_init(&s_drawBuf, s_buf1, s_buf2, kBufPx);

    static lv_disp_drv_t dispDrv;
    lv_disp_drv_init(&dispDrv);
    dispDrv.hor_res = TFT_LCD_W;
    dispDrv.ver_res = TFT_LCD_H;
    dispDrv.flush_cb = flush_cb;
    dispDrv.draw_buf = &s_drawBuf;
    lv_disp_drv_register(&dispDrv);

    // 5 ms LVGL tick.
    const esp_timer_create_args_t tickArgs = {
        .callback = &tick_cb, .arg = nullptr, .dispatch_method = ESP_TIMER_TASK,
        .name = "lvgl_tick", .skip_unhandled_events = true,
    };
    esp_timer_handle_t tickTimer;
    esp_timer_create(&tickArgs, &tickTimer);
    esp_timer_start_periodic(tickTimer, 5000);

    // The one and only input device: touchscreen pointer.
    static lv_indev_drv_t ptrDrv;
    lv_indev_drv_init(&ptrDrv);
    ptrDrv.type = LV_INDEV_TYPE_POINTER;
    ptrDrv.read_cb = touch_read_cb;
    lv_indev_drv_register(&ptrDrv);
}

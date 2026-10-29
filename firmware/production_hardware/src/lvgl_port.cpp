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
static lv_group_t* s_group = nullptr;

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

// ----------------------------------------------------------------- inputs ---
// EC11 quadrature: poll CLK edges inside the indev read callback (LVGL calls
// it ~every 30 ms; detent encoders are slow enough for this at bench scale).
static int32_t s_encDiff = 0;
static int     s_encLastClk = HIGH;

static bool btn_low(uint8_t pin) { return digitalRead(pin) == LOW; }

static void encoder_read_cb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    (void)drv;
    int clk = digitalRead(ENC_CLK);
    if (clk == LOW && s_encLastClk == HIGH) {
        s_encDiff += (digitalRead(ENC_DT) == HIGH) ? 1 : -1;
    }
    s_encLastClk = clk;
    data->enc_diff = (int16_t)s_encDiff;
    s_encDiff = 0;
    data->state = btn_low(ENC_SW) ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

// Joystick thresholds: 12-bit ADC, centre ~2048, firmware deadzone required
// (handlebar vibration — see SYSTEM_DESIGN.md). Buttons are active-low with
// internal pull-ups (external 10k pull-ups on the custom PCB).
static uint32_t keypad_scan(void) {
    int jx = analogRead(JOY_X);
    int jy = analogRead(JOY_Y);
    if (jx < 1100) return UI_KEY_JOY_LEFT;
    if (jx > 3000) return UI_KEY_JOY_RIGHT;
    if (jy < 1100) return UI_KEY_JOY_UP;
    if (jy > 3000) return UI_KEY_JOY_DOWN;
    if (btn_low(JOY_SW))  return LV_KEY_ENTER;
    if (btn_low(BTN_BACK)) return LV_KEY_ESC;
    return 0;
}

static void keypad_read_cb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    (void)drv;
    static uint32_t lastKey = 0;
    static uint32_t lastChangeMs = 0;
    uint32_t key = keypad_scan();
    uint32_t now = millis();
    // 40 ms debounce / change blanking so a held direction doesn't machine-gun.
    if (key != lastKey && (now - lastChangeMs) < 40) key = lastKey;
    if (key != lastKey) { lastKey = key; lastChangeMs = now; }
    data->key = lastKey;
    data->state = lastKey ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    data->continue_reading = false;
}

bool lvgl_port_play_pressed(void) {
    static bool last = false;
    static uint32_t lastEdgeMs = 0;
    bool now = btn_low(BTN_PLAY);
    bool edge = now && !last && (millis() - lastEdgeMs > 60);
    if (edge) lastEdgeMs = millis();
    last = now;
    return edge;
}

bool lvgl_port_home_pressed(void) {
    static bool last = false;
    static uint32_t lastEdgeMs = 0;
    bool now = btn_low(BTN_HOME);
    bool edge = now && !last && (millis() - lastEdgeMs > 60);
    if (edge) lastEdgeMs = millis();
    last = now;
    return edge;
}

// -------------------------------------------------------------------- init --
void lvgl_port_init(void) {
    // Inputs first (pull-ups on every digital line).
    pinMode(ENC_CLK, INPUT_PULLUP);
    pinMode(ENC_DT, INPUT_PULLUP);
    pinMode(ENC_SW, INPUT_PULLUP);
    pinMode(JOY_SW, INPUT_PULLUP);
    pinMode(BTN_BACK, INPUT_PULLUP);
    pinMode(BTN_PLAY, INPUT_PULLUP);
    pinMode(BTN_HOME, INPUT_PULLUP);
    analogReadResolution(12);

    // Display.
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

    // Focus group shared by encoder + keypad; screens refill it per screen.
    s_group = lv_group_create();
    lv_group_set_default(s_group);

    static lv_indev_drv_t encDrv;
    lv_indev_drv_init(&encDrv);
    encDrv.type = LV_INDEV_TYPE_ENCODER;
    encDrv.read_cb = encoder_read_cb;
    lv_indev_t* encIndev = lv_indev_drv_register(&encDrv);
    lv_indev_set_group(encIndev, s_group);

    static lv_indev_drv_t keyDrv;
    lv_indev_drv_init(&keyDrv);
    keyDrv.type = LV_INDEV_TYPE_KEYPAD;
    keyDrv.read_cb = keypad_read_cb;
    lv_indev_t* keyIndev = lv_indev_drv_register(&keyDrv);
    lv_indev_set_group(keyIndev, s_group);
}

lv_group_t* lvgl_port_group(void) { return s_group; }

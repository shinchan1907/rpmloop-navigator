// MiniNav-Mini: shared state between network, protocol, video and display tasks.
// Stripped from original MiniNav: no demo mode, no IMU, no SD card, no capacitive touch.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "sdkconfig.h"

#define LCD_W 320
#define LCD_H 240

// Android Auto's app rail is 80 dp wide. With CONFIG_AAM_HIDE_RAIL the board requests a UI
// area that much wider than the 640 px shown, then crops the rail off.
#ifndef CONFIG_AAM_HIDE_RAIL
#define CONFIG_AAM_HIDE_RAIL 0
#endif
#define RAIL_PX (80 * CONFIG_AAM_VIDEO_DENSITY / 160)  // 100 px at 200 dpi

extern volatile int g_video_margin_w;  // margin_width announced to the phone (both sides total)
extern volatile int g_video_crop_x;   // first column of the 800x480 frame that is displayed

// Connection stages shown on the splash/status screen.
enum aam_stage {
    STAGE_BOOT = 0,
    STAGE_AP,        // access point up, waiting for phone
    STAGE_PHONE,     // phone joined, has IP
    STAGE_TCP,       // TCP connected to headunit server
    STAGE_VERSION,   // version handshake OK
    STAGE_TLS,       // TLS + auth done
    STAGE_DISCOVERY, // service discovery answered
    STAGE_VIDEO,     // live map frames on screen
};

typedef struct {
    volatile int  stage;
    volatile bool night;           // dark theme (synced with Android Auto night mode)
    volatile bool touch_enabled;   // touch forwarded to Android Auto
    volatile bool full_screen;     // rail hidden (needs touch off)
    volatile bool status_dirty;    // status/splash screen needs a redraw
    volatile int  rotation;        // LovyanGFX rotation value (1 = landscape)
    char phone_ip[16];
    char phone_name[32];
    char error[64];                // last failure message shown on splash screen
} aam_state_t;

extern aam_state_t g_app;

#ifdef __cplusplus
extern "C" {
#endif

void app_set_stage(int stage);
void app_set_error(const char *fmt, ...);
void app_toggle_night(const char *why);
void app_toggle_touch(const char *why);
void app_toggle_full(const char *why);

// board.cpp
void     board_init(void);
void     board_poll_inputs(void);           // resistive touch poll; call every ~10 ms
uint16_t *display_acquire_frame(void);      // 320x240 byte-swapped RGB565 buffer to fill
void     display_submit_frame(uint16_t *fb);
void     display_request_status(void);
void     display_toast(const char *text);   // brief banner over the screen for ~1.5 s
void     display_dump(void);                // console command: print screen as hex rows

// net.cpp
void net_start(void);
bool net_phone_ip(char *out, int len);     // latest phone IP; false if none

// aap.cpp
void aap_start(void);
void aap_send_night(bool night);
void aap_send_video_ack(void);
void aap_queue_video_ack(void);
void aap_request_keyframe(void);
void aap_restart_session(void);
void aap_send_touch(int action, int x, int y);  // action 0=down 1=up 2=move; x,y in 800x480

// video.cpp
void video_start(void);
bool video_push_au(const uint8_t *data, int len, bool from_phone);
void video_set_codec_config(const uint8_t *data, int len);
void video_reset(void);

#ifdef __cplusplus
}
#endif

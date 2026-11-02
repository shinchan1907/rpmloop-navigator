// LVGL display/touch port for RPMLOOP Navigator
// Bridges LovyanGFX <-> LVGL 8.3.11

#pragma once

#include <lvgl.h>
#include <Arduino.h>
#include "lgfx_config.h"

// Global display instance
extern LGFX tft;

// Initialize LVGL display driver, input driver, and buffers
void lvgl_port_init(void);

// Call in loop() — runs LVGL task handler
void lvgl_port_update(void);

// Get the LVGL display object
lv_disp_t* lvgl_port_get_disp(void);

// Dump active screen capture buffer as 24-bit BMP over Stream
void lvgl_port_dump_bmp(Stream& s);


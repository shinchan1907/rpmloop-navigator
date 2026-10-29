#ifndef LVGL_PORT_H
#define LVGL_PORT_H

#include <lvgl.h>
#include "lgfx_config.h"

// LVGL port layer: owns the LGFX display, draw buffers, the 5 ms tick timer
// and the single input device — a pointer indev fed by the module's HR2046
// touchscreen (the product's ONLY input; there are no physical controls).
// Screens are plain touchable LVGL widgets; no focus group is used.

extern LGFX tft;

void lvgl_port_init(void);

#endif // LVGL_PORT_H

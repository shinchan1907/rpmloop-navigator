#ifndef PINOUT_CONFIG_H
#define PINOUT_CONFIG_H

// ==============================================================================
// RPMLOOP NAVIGATOR — HARDWARE PIN MAPPING (BENCH-VERIFIED 2026-10-29)
// Target: ESP32-S3 (dev: ESP32-S3 Mini) + generic red 2.8" SPI TFT module
//         (ILI9341, 240x320 native, used landscape 320x240).
//
// PRODUCT SCOPE (owner decision, FINAL): the device is ONLY the ESP32-S3 and
// this display module. The module's HR2046 resistive touchscreen is the ONLY
// input. There is NO GPS module, NO encoder/joystick/buttons and NO SD card
// in this product — all navigation data arrives from the phone companion
// apps over BLE. (An earlier standalone design carried those peripherals;
// their pin defines were removed so the firmware cannot quietly grow them
// back. The module's own microSD slot on its back is likewise unused.)
//
// This map is the one physically tested on the bench; the same firmware
// runs on the dev board today and the custom PCB (ESP32-S3-WROOM-1-N16R8)
// tomorrow.
// CONFLICT NOTE: an older Waveshare map had GPIO9 = DC / GPIO10 = CS — on
// the bench hardware GPIO9 is CS and GPIO10 is DC. Do not mix the two maps.
// ==============================================================================

// ---- Display SPI interface (SPI2 / FSPI) ----
#define TFT_SCLK         6    // Display SCK
#define TFT_MOSI         7    // Display SDI (MOSI)
#define TFT_MISO         8    // Display SDO (MISO)
#define TFT_CS           9    // Display CS (active low)
#define TFT_DC           10   // Display DC / RS
#define TFT_RST          11   // Display RESET (active low)
// Backlight: the module's LED pin is tied directly to 3V3 (always on, module
// has onboard current limiting). There is NO backlight GPIO and no PWM
// dimming on this hardware — brightness is done in software (LVGL dim
// overlay), see BikeState.brightnessPct.

// ---- Bench-proven panel configuration (DO NOT "fix" without re-testing) ----
// SPI write clock: 16 MHz is stable on bench jumper wires; 40 MHz garbles
// pixels (long loose Dupont leads). A soldered PCB build with short traces
// can raise this back toward 40 MHz.
#define TFT_SPI_FREQ_WRITE  16000000
#define TFT_SPI_FREQ_READ   8000000
// This clone ILI9341 panel shows INVERTED colours unless panel invert is on.
#define TFT_PANEL_INVERT    true
// Landscape needs the mirror-corrected LovyanGFX rotation on this panel
// (plain rotation 1 renders mirrored text). ONE place to change it:
#define TFT_ROTATION        5
#define TFT_LCD_W           320   // landscape width  (after rotation)
#define TFT_LCD_H           240   // landscape height (after rotation)

// ---- Touchscreen: HR2046 on the display module (XPT2046-compatible) -------
// The ONLY input device in the product. T_CLK / T_DIN / T_DO share the
// display SPI bus — wire T_CLK -> TFT_SCLK (GPIO 6), T_DIN -> TFT_MOSI
// (GPIO 7), T_DO -> TFT_MISO (GPIO 8). Only CS and IRQ get their own GPIOs.
#define TOUCH_CS         12   // T_CS (chip select, active low)
#define TOUCH_IRQ        13   // T_IRQ / PENIRQ (LOW while the panel is pressed)
#define TOUCH_SPI_FREQ   2500000  // XPT2046-class ADCs top out around 2.5 MHz

// Touch calibration: RAW ADC ranges that map to the panel's corners
// (consumed by the LGFX class in lgfx_config.h via Touch_XPT2046 config).
// !!! UNVERIFIED PRESETS — typical XPT2046 starting values, NOT measured on
// this module. Calibrate on the real hardware before trusting taps: run
// LovyanGFX calibrateTouch() once, or log the raw values at the four
// corners, then update these four numbers. If taps land mirrored/swapped
// versus the picture, the fix is the touch offset_rotation in
// lgfx_config.h, not these ranges.
#define TOUCH_CAL_X_MIN  200
#define TOUCH_CAL_X_MAX  3900
#define TOUCH_CAL_Y_MIN  200
#define TOUCH_CAL_Y_MAX  3900

// ---- Onboard WS2812 status LED (original Waveshare Zero target only) ------
// The BLE status LED code only matters on the Waveshare board that
// physically has the WS2812; on the bench S3 Mini there is no such LED and
// the writes are harmless no-ops.
#ifndef RGB_BUILTIN
#define RGB_BUILTIN      21
#endif

#endif // PINOUT_CONFIG_H

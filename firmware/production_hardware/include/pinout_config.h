#ifndef PINOUT_CONFIG_H
#define PINOUT_CONFIG_H

// ==============================================================================
// RPMLOOP NAVIGATOR — HARDWARE PIN MAPPING (BENCH-VERIFIED 2026-10-29)
// Target: ESP32-S3 (dev: ESP32-S3 Mini) + generic red 2.8" SPI TFT module
//         (ILI9341, 240x320 native, used landscape 320x240).
//
// This REPLACES the previous Waveshare-Zero pinout (MOSI 11 / SCLK 12 / CS 10 /
// DC 9 / RST 8 / BL-PWM 7 / touch on 5-6). That map was never wired to real
// hardware; the map below is the one physically tested on the bench and it
// matches the bike-nav custom-PCB project pin map (SYSTEM_DESIGN.md), so the
// same firmware runs on the dev board today and the custom PCB tomorrow.
// CONFLICT NOTE: on the old map GPIO9 was DC and GPIO10 was CS — on the bench
// hardware GPIO9 is CS and GPIO10 is DC. Do not mix the two maps.
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

// ---- Human input: EC11 encoder + PSP joystick + 3 buttons (NO touch) ----
// The module's HR2046 resistive touch is present but intentionally UNUSED —
// the product design is glove-friendly physical controls only.
#define ENC_CLK          12   // EC11 channel A
#define ENC_DT           13   // EC11 channel B
#define ENC_SW           4    // EC11 push = SELECT / ENTER
#define JOY_X            5    // Joystick X axis (ADC1_CH4)
#define JOY_Y            1    // Joystick Y axis (ADC1_CH0)
#define JOY_SW           2    // Joystick push
#define BTN_BACK         41   // BACK button
#define BTN_PLAY         42   // PLAY / PAUSE button
#define BTN_HOME         47   // HOME button
#define STATUS_LED       48   // Plain status LED (custom PCB)

// ---- Reserved for later milestones (routed on the custom PCB, unused here) -
#define SD_SCLK          14   // microSD slot on the display module's back:
#define SD_MOSI          15   // SD_SCK / SD_MOSI / SD_MISO / SD_CS pads.
#define SD_MISO          16   // (SD is out of scope for the UI milestone.)
#define SD_CS            17
#define GPS_RXD          18   // NEO-M8N UART1: S3 RX <- module TX
#define GPS_TXD          21   // NEO-M8N UART1: S3 TX -> module RX

// ---- Onboard WS2812 status LED (original Waveshare Zero target only) -------
// NOTE: GPIO21 is GPS_TXD in the pin map above. The BLE status LED code only
// matters on the Waveshare board that physically has the WS2812; on the bench
// S3 Mini there is no such LED and the writes are harmless no-ops. If GPS is
// ever enabled on a board with the WS2812 fitted, move one of the two.
#ifndef RGB_BUILTIN
#define RGB_BUILTIN      21
#endif

#endif // PINOUT_CONFIG_H

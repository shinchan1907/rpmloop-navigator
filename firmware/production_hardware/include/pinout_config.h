#ifndef PINOUT_CONFIG_H
#define PINOUT_CONFIG_H

// ==============================================================================
// RPMLOOP NAVIGATOR — HARDWARE PIN MAPPING
// Target: Waveshare ESP32-S3 Mini / Zero + 2.8" SPI TFT LCD (ST7789/ILI9341)
// ==============================================================================

// Display SPI Interface (FSPI / SPI2)
#define LCD_MOSI         11   // Master Out Slave In
#define LCD_MISO         13   // Master In Slave Out
#define LCD_SCLK         12   // Serial Clock
#define LCD_CS           10   // Chip Select (Active Low)
#define LCD_DC           9    // Data / Command
#define LCD_RST          8    // Hardware Reset (Active Low)
#define LCD_BL           7    // Backlight PWM Dimming (LEDC Channel 0)

// Resistive Touch Controller (XPT2046)
#define TOUCH_CS         6    // Touch Chip Select
#define TOUCH_IRQ        5    // Touch Pen Interrupt

// MicroSD Card SPI Interface
#define SD_CS            4    // MicroSD Chip Select

// Onboard WS2812 Mini RGB LED
#ifndef RGB_BUILTIN
#define RGB_BUILTIN      21   // Onboard RGB data pin on Waveshare ESP32-S3
#endif

// Power / Backlight Configuration
#define LCD_BL_FREQ      5000 // 5 kHz PWM frequency for flicker-free dimming
#define LCD_BL_RES       8    // 8-bit resolution (0-255)
#define LCD_BL_CHANNEL   0

#endif // PINOUT_CONFIG_H

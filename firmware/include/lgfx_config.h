// LovyanGFX display + touch configuration for RPMLOOP Navigator
// Board: ESP32-S3 Mini, Display: ST7789V 2.8" 240x320, Touch: HR2046
// SD Card on shared SPI bus with display

#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device {
    // Display panel
    lgfx::Panel_ST7789 _panel_instance;
    // Display SPI bus (SPI2_HOST) — shared with SD card
    lgfx::Bus_SPI _bus_instance;
    // Touch controller on separate SPI bus (SPI3_HOST)
    lgfx::Touch_XPT2046 _touch_instance;

public:
    LGFX(void) {
        // ---- SPI bus for display (SPI2_HOST) ----
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host   = SPI2_HOST;
            cfg.spi_mode   = 0;
            cfg.freq_write = 16000000;   // 16 MHz write clock (bench-verified)
            cfg.freq_read  = 8000000;
            cfg.spi_3wire  = false;
            cfg.use_lock   = true;
            cfg.dma_channel = 0;          // Shared SPI bus with SD card — disable DMA to prevent bus stall
            cfg.pin_sclk   = 6;   // GPIO6 — Display SCK
            cfg.pin_mosi   = 7;   // GPIO7 — Display MOSI
            cfg.pin_miso   = 8;   // GPIO8 — Display MISO
            cfg.pin_dc     = 10;  // GPIO10 — Display DC
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        // ---- Display panel (ST7789V) ----
        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs     = 9;    // GPIO9 — Display CS
            cfg.pin_rst    = 11;   // GPIO11 — Display RESET
            cfg.pin_busy   = -1;
            cfg.panel_width  = 240;
            cfg.panel_height = 320;
            cfg.offset_x     = 0;
            cfg.offset_y     = 0;
            cfg.offset_rotation = 0;
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits  = 1;
            cfg.readable   = true;
            cfg.invert     = false;  // ST7789V: invert=false (bench-verified)
            cfg.rgb_order  = false;
            cfg.dlen_16bit = false;
            cfg.bus_shared = true;   // SPI bus shared with SD card
            _panel_instance.config(cfg);
        }

        // ---- Touch (HR2046 / XPT2046-compatible) on SPI3_HOST ----
        {
            auto cfg = _touch_instance.config();
            cfg.x_min      = 270;   // Calibration: raw X min (bench-measured)
            cfg.x_max      = 3780;  // Calibration: raw X max
            cfg.y_min      = 280;   // Calibration: raw Y min
            cfg.y_max      = 3650;  // Calibration: raw Y max
            cfg.pin_int    = -1;    // Polling mode! T_IRQ is not connected on bench unit, SPI polling reads Z-pressure
            cfg.bus_shared = false; // Touch has its OWN SPI bus (SPI3_HOST)
            cfg.offset_rotation = 4;// Axis-swapped (bench-verified)

            cfg.spi_host   = SPI3_HOST;
            cfg.freq       = 1000000; // 1 MHz for touch
            cfg.pin_sclk   = 1;   // GPIO1 — T_CLK
            cfg.pin_mosi   = 2;   // GPIO2 — T_DIN
            cfg.pin_miso   = 3;   // GPIO3 — T_DO
            cfg.pin_cs     = 5;   // GPIO5 — T_CS

            _touch_instance.config(cfg);
            _panel_instance.setTouch(&_touch_instance);
        }

        setPanel(&_panel_instance);
    }
};

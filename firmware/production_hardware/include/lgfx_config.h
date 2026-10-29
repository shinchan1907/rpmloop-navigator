#ifndef LGFX_CONFIG_H
#define LGFX_CONFIG_H

#include <LovyanGFX.hpp>
#include "pinout_config.h"

// ==============================================================================
// LovyanGFX device for the bench-verified panel:
//   generic red 2.8" SPI TFT module, ILI9341, 240x320 native, used 320x240,
//   with its HR2046 (XPT2046-compatible) resistive touch on the same SPI bus
//   — the touchscreen is the product's ONLY input.
// BENCH-PROVEN QUIRKS (2026-10-29, do not remove without re-testing):
//   * pcfg.invert = true  — this clone panel shows inverted colours otherwise
//   * SPI write = 16 MHz  — 40 MHz garbles pixels on jumper wires
//     (TFT_SPI_FREQ_WRITE in pinout_config.h; raise on the soldered PCB)
//   * rotation = TFT_ROTATION (5) — plain landscape (1) renders mirrored text
// TOUCH NOTE: the touch calibration ranges (TOUCH_CAL_* in pinout_config.h)
// are UNVERIFIED presets — calibrate on the real module. LovyanGFX converts
// raw touch points through the panel rotation (getTouch), using the touch
// config's offset_rotation (0 below). If taps come out swapped/mirrored
// against the picture on hardware, adjust tcfg.offset_rotation (0..7) here.
// If a different panel batch is ever fitted, change ONLY this class.
// ==============================================================================
class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ILI9341  _panel;
    lgfx::Bus_SPI        _bus;
    lgfx::Touch_XPT2046  _touch;

public:
    LGFX() {
        auto bcfg = _bus.config();
        bcfg.spi_host    = SPI2_HOST;
        bcfg.spi_mode    = 0;
        bcfg.freq_write  = TFT_SPI_FREQ_WRITE;
        bcfg.freq_read   = TFT_SPI_FREQ_READ;
        bcfg.spi_3wire   = false;
        bcfg.use_lock    = true;
        bcfg.dma_channel = SPI_DMA_CH_AUTO;
        bcfg.pin_sclk    = TFT_SCLK;
        bcfg.pin_mosi    = TFT_MOSI;
        bcfg.pin_miso    = TFT_MISO;
        bcfg.pin_dc      = TFT_DC;
        _bus.config(bcfg);
        _panel.setBus(&_bus);

        auto pcfg = _panel.config();
        pcfg.pin_cs           = TFT_CS;
        pcfg.pin_rst          = TFT_RST;
        pcfg.pin_busy         = -1;
        pcfg.memory_width     = 240;
        pcfg.memory_height    = 320;
        pcfg.panel_width      = 240;
        pcfg.panel_height     = 320;
        pcfg.offset_x         = 0;
        pcfg.offset_y         = 0;
        pcfg.offset_rotation  = 0;
        pcfg.readable         = true;
        pcfg.invert           = TFT_PANEL_INVERT;   // clone panel: REQUIRED
        pcfg.rgb_order        = false;
        pcfg.dlen_16bit       = false;
        pcfg.bus_shared       = false;
        _panel.config(pcfg);

        // Touchscreen (HR2046) sharing the display's SPI2 bus: only CS and
        // IRQ are separate wires; SCLK/MOSI/MISO are the TFT pins. Config
        // fields per lgfx::ITouch::config_t (LovyanGFX 1.2.x, Touch.hpp).
        auto tcfg = _touch.config();
        tcfg.spi_host    = SPI2_HOST;   // same host as the panel bus
        tcfg.pin_sclk    = TFT_SCLK;
        tcfg.pin_mosi    = TFT_MOSI;
        tcfg.pin_miso    = TFT_MISO;
        tcfg.pin_cs      = TOUCH_CS;
        tcfg.pin_int     = TOUCH_IRQ;
        tcfg.pin_rst     = -1;          // module breaks out no touch reset
        tcfg.bus_shared  = true;        // shares SPI2 with the panel
        tcfg.freq        = TOUCH_SPI_FREQ;
        tcfg.x_min       = TOUCH_CAL_X_MIN;   // UNVERIFIED presets — see
        tcfg.x_max       = TOUCH_CAL_X_MAX;   // pinout_config.h calibration
        tcfg.y_min       = TOUCH_CAL_Y_MIN;   // note before trusting taps.
        tcfg.y_max       = TOUCH_CAL_Y_MAX;
        tcfg.offset_rotation = 0;
        _touch.config(tcfg);
        _panel.setTouch(&_touch);

        // No Light component: module LED pin is wired straight to 3V3.
        setPanel(&_panel);
    }
};

#endif // LGFX_CONFIG_H

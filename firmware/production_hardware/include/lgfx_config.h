#ifndef LGFX_CONFIG_H
#define LGFX_CONFIG_H

#include <LovyanGFX.hpp>
#include "pinout_config.h"

// ==============================================================================
// LovyanGFX device for the bench panel:
//   generic red 2.8" SPI TFT module, ST7789V (chip ID probed on hardware:
//   0x85 0x85 0x52), 240x320 native, used 320x240, with its HR2046
//   (XPT2046-compatible) resistive touch wired to its own SPI pins on
//   SPI3 — the touchscreen is the product's ONLY input.
// PANEL FACTS (2026-10-29, do not remove without re-testing):
//   * the panel is ST7789V, NOT ILI9341 — it was misdiagnosed for a day
//     because the command sets overlap (ILI9341 init gave readable but
//     wrong output: inverted colours, mirrored text, garbled full draws)
//   * pcfg.invert = false — ST7789V shows true colours with invert OFF
//   * SPI write = 16 MHz  — 40 MHz garbles pixels on jumper wires
//     (TFT_SPI_FREQ_WRITE in pinout_config.h)
//   * rotation = TFT_ROTATION (1) — plain landscape on ST7789V
//   * pcfg.offset_x / offset_y are 0 (LovyanGFX Panel_ST7789 defaults).
//     If the picture ever lands shifted on a panel batch, THOSE two are
//     the adjustment knob — ST7789 GRAM is 240x320 and some modules need
//     a small offset; this bench module has not shown a shift yet.
// TOUCH NOTE: the touch calibration ranges (TOUCH_CAL_* in pinout_config.h)
// are UNVERIFIED presets — calibrate on the real module. LovyanGFX converts
// raw touch points through the panel rotation (getTouch), using the touch
// config's offset_rotation (0 below). If taps come out swapped/mirrored
// against the picture on hardware, adjust tcfg.offset_rotation (0..7) here.
// If a different panel batch is ever fitted, change ONLY this class.
// ==============================================================================
class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789   _panel;
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
        pcfg.invert           = TFT_PANEL_INVERT;   // ST7789V: false (see header)
        pcfg.rgb_order        = false;
        pcfg.dlen_16bit       = false;
        pcfg.bus_shared       = false;
        _panel.config(pcfg);

        // Touchscreen (HR2046) on its OWN SPI peripheral: SPI3 with its
        // own pins (pinout_config.h). It must NOT reuse the panel's
        // SPI2_HOST with different pins: on ESP32, lgfx spi::init() only
        // routes pins the first time a host is initialised (the
        // _spi_dev_handle guard in platforms/esp32/common.inl), so a
        // second init on SPI2 would silently keep the display's pins.
        // Config fields per lgfx::ITouch::config_t (LovyanGFX 1.2.x,
        // Touch.hpp). pin_int = -1 (T_IRQ not wired) is the library's
        // supported polling-only mode: Touch_XPT2046::init/getTouchRaw
        // guard every pin_int use with `>= 0`.
        auto tcfg = _touch.config();
        tcfg.spi_host    = SPI3_HOST;   // touch has its own peripheral
        tcfg.pin_sclk    = TOUCH_SCLK;
        tcfg.pin_mosi    = TOUCH_MOSI;
        tcfg.pin_miso    = TOUCH_MISO;
        tcfg.pin_cs      = TOUCH_CS;
        tcfg.pin_int     = TOUCH_IRQ;   // -1: not wired, polling only
        tcfg.pin_rst     = -1;          // module breaks out no touch reset
        tcfg.bus_shared  = false;       // separate bus from the panel
        tcfg.freq        = TOUCH_SPI_FREQ;
        tcfg.x_min       = TOUCH_CAL_X_MIN;   // UNVERIFIED presets — see
        tcfg.x_max       = TOUCH_CAL_X_MAX;   // pinout_config.h calibration
        tcfg.y_min       = TOUCH_CAL_Y_MIN;   // note before trusting taps.
        tcfg.y_max       = TOUCH_CAL_Y_MAX;
        tcfg.offset_rotation = 4;
        _touch.config(tcfg);
        _panel.setTouch(&_touch);

        // No Light component: module LED pin is wired straight to 3V3.
        setPanel(&_panel);
    }
};

#endif // LGFX_CONFIG_H

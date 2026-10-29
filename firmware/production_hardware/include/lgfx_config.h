#ifndef LGFX_CONFIG_H
#define LGFX_CONFIG_H

#include <LovyanGFX.hpp>
#include "pinout_config.h"

// ==============================================================================
// LovyanGFX device for the bench-verified panel:
//   generic red 2.8" SPI TFT module, ILI9341, 240x320 native, used 320x240.
// BENCH-PROVEN QUIRKS (2026-10-29, do not remove without re-testing):
//   * pcfg.invert = true  — this clone panel shows inverted colours otherwise
//   * SPI write = 16 MHz  — 40 MHz garbles pixels on jumper wires
//     (TFT_SPI_FREQ_WRITE in pinout_config.h; raise on the soldered PCB)
//   * rotation = TFT_ROTATION (5) — plain landscape (1) renders mirrored text
// If a different panel batch is ever fitted, change ONLY this class.
// ==============================================================================
class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ILI9341 _panel;
    lgfx::Bus_SPI       _bus;

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

        // No Light component: module LED pin is wired straight to 3V3.
        setPanel(&_panel);
    }
};

#endif // LGFX_CONFIG_H

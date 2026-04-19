#pragma once
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789  _panel_instance;
  lgfx::Bus_SPI       _bus_instance;
  lgfx::Touch_XPT2046 _touch_instance;

public:
  LGFX(void) {
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host    = VSPI_HOST;
      cfg.spi_mode    = 0;
      cfg.freq_write  = 20000000;
      cfg.freq_read   = 16000000;
      cfg.pin_sclk    = 14;
      cfg.pin_mosi    = 13;
      cfg.pin_miso    = 12;
      cfg.pin_dc      =  2;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs        = 15;
      cfg.pin_rst       = -1;
      cfg.panel_width   = 240;
      cfg.panel_height  = 320;
      cfg.memory_width  = 240;
      cfg.memory_height = 320;
      cfg.offset_x      = 0;
      cfg.offset_y      = 0;
      cfg.invert        = false;
      cfg.rgb_order     = false;
      _panel_instance.config(cfg);
    }

    {
      auto cfg = _touch_instance.config();
      cfg.x_min          = 300;
      cfg.x_max          = 3900;
      cfg.y_min          = 3700;
      cfg.y_max          = 200;
      cfg.pin_int        = -1;
      cfg.bus_shared     = false;
      cfg.spi_host       = -1;
      cfg.pin_sclk       = 25;
      cfg.pin_mosi       = 32;
      cfg.pin_miso       = 39;
      cfg.pin_cs         = 33;
      cfg.freq           = 1000000;
      cfg.offset_rotation = 2;
      _touch_instance.config(cfg);
      _panel_instance.setTouch(&_touch_instance);
    }

    setPanel(&_panel_instance);
  }
};

extern LGFX lcd;

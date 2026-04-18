#include <Arduino.h>
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9341 _panel_instance;
  lgfx::Bus_SPI       _bus_instance;
  lgfx::Light_PWM     _light_instance;
  lgfx::Touch_XPT2046 _touch_instance;

public:
  LGFX(void) {
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host    = HSPI_HOST;
      cfg.spi_mode    = 0;
      cfg.freq_write  = 40000000;
      cfg.freq_read   = 16000000;
      cfg.spi_3wire   = true;
      cfg.use_lock    = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = 14;
      cfg.pin_mosi = 13;
      cfg.pin_miso = 12;
      cfg.pin_dc   =  2;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs           = 15;
      cfg.pin_rst          = -1;
      cfg.pin_busy         = -1;
      cfg.panel_width      = 240;
      cfg.panel_height     = 320;
      cfg.offset_x         = 0;
      cfg.offset_y         = 0;
      cfg.offset_rotation  = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;
      cfg.readable         = true;
      cfg.invert           = true;
      cfg.rgb_order        = false;
      cfg.dlen_16bit       = false;
      cfg.bus_shared       = false;
      _panel_instance.config(cfg);
    }

    {
      auto cfg = _light_instance.config();
      cfg.pin_bl      = 21;
      cfg.invert      = false;
      cfg.freq        = 12000;
      cfg.pwm_channel = 7;
      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);
    }

    {
      auto cfg = _touch_instance.config();
      cfg.x_min      =  300;
      cfg.x_max      = 3900;
      cfg.y_min      = 3700;
      cfg.y_max      =  200;
      cfg.pin_int    = -1;
      cfg.bus_shared = false;
      cfg.spi_host   = -1;
      cfg.pin_sclk   = 25;
      cfg.pin_mosi   = 32;
      cfg.pin_miso   = 39;
      cfg.pin_cs     = 33;
      _touch_instance.config(cfg);
      _panel_instance.setTouch(&_touch_instance);
    }

    setPanel(&_panel_instance);
  }
};

LGFX lcd;

void setup() {
  Serial.begin(115200);
  lcd.init();
  Serial.println("Cycling rotations...");
}

void loop() {
  for (int rot = 0; rot < 4; rot++) {
    lcd.setRotation(rot);
    lcd.fillScreen(TFT_BLACK);
    int w = lcd.width();
    int h = lcd.height();

    lcd.fillRect(0, 0, 10, 10, TFT_RED);
    lcd.fillRect(w - 10, 0, 10, 10, TFT_GREEN);
    lcd.fillRect(0, h - 10, 10, 10, TFT_BLUE);
    lcd.fillRect(w - 10, h - 10, 10, 10, TFT_YELLOW);

    lcd.setTextDatum(textdatum_t::middle_center);
    lcd.setFont(&fonts::Font2);
    lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    lcd.setTextSize(1);
    char buf[32];
    snprintf(buf, sizeof(buf), "rot %d  %dx%d", rot, w, h);
    lcd.drawString(buf, w / 2, h / 2);

    Serial.printf("rot %d: %dx%d\n", rot, w, h);
    delay(3000);
  }
}

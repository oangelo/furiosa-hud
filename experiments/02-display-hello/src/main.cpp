#include <Arduino.h>
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 _panel_instance;
  lgfx::Bus_SPI      _bus_instance;

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

    setPanel(&_panel_instance);
  }
};

LGFX lcd;

void setup() {
  Serial.begin(115200);
  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);
  lcd.init();
  lcd.setRotation(1);
  Serial.println("Display init done. Cycling rotations...");
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

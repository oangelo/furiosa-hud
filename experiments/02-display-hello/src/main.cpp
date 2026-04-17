/*
 * Experimento 02 — Hello World no display ILI9341 (CYD)
 *
 * Objetivo: configurar LovyanGFX para o ESP32-2432S028R e
 *           renderizar texto "Furiosa HUD" na tela.
 */
#include <Arduino.h>
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Default {
public:
  LGFX(void) {
    auto cfg = bus_spi();
    cfg.spi_host = VSPI_HOST;
    cfg.spi_mode = 0;
    cfg.freq_write = 40000000;
    cfg.freq_read  = 16000000;
    cfg.pin_sclk   = 14;
    cfg.pin_mosi   = 13;
    cfg.pin_miso   = 12;
    cfg.pin_dc     =  2;
    bus_spi(cfg);

    auto cfg_lcd = panel();
    cfg_lcd.panel_width   = 240;
    cfg_lcd.panel_height  = 320;
    cfg_lcd.offset_x      = 0;
    cfg_lcd.offset_y      = 0;
    cfg_lcd.offset_rotation = 0;
    cfg_lcd.pin_cs        = 15;
    cfg_lcd.pin_rst       = -1;
    cfg_lcd.readable      = true;
    cfg_lcd.invert        = true;
    panel(cfg_lcd);

    auto cfg_touch = touch();
    cfg_touch.x_min      = 0;
    cfg_touch.x_max      = 239;
    cfg_touch.y_min      = 0;
    cfg_touch.y_max      = 319;
    cfg_touch.pin_int     = 36;
    cfg_touch.bus_shared  = true;
    cfg_touch.spi_host    = VSPI_HOST;
    cfg_touch.freq        = 1000000;
    cfg_touch.pin_sclk    = 14;
    cfg_touch.pin_mosi    = 13;
    cfg_touch.pin_miso    = 12;
    cfg_touch.pin_cs      = 33;
    touch(cfg_touch);

    setRotation(1);
  }
};

LGFX lcd;

void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.fillScreen(TFT_BLACK);
  lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  lcd.setTextSize(2);
  lcd.setCursor(40, 140);
  lcd.print("Furiosa HUD");
  Serial.println("Display initialized.");
}

void loop() {
}

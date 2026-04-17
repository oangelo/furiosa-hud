/*
 * Experimento 04 — Dados do VESC na tela via Bluetooth
 *
 * Objetivo: combinar tudo — conectar ao VESC BT via SPP,
 *           ler telemetria com VescUart e exibir no display.
 */
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include "BluetoothSerial.h"
#include <VescUart.h>

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled!
#endif

BluetoothSerial SerialBT;
VescUart vesc;

class LGFX : public lgfx::LGFX_Default {
public:
  LGFX(void) {
    auto cfg = bus_spi();
    cfg.spi_host   = VSPI_HOST;
    cfg.spi_mode   = 0;
    cfg.freq_write = 40000000;
    cfg.freq_read  = 16000000;
    cfg.pin_sclk   = 14;
    cfg.pin_mosi   = 13;
    cfg.pin_miso   = 12;
    cfg.pin_dc     =  2;
    bus_spi(cfg);

    auto cfg_lcd = panel();
    cfg_lcd.panel_width    = 240;
    cfg_lcd.panel_height   = 320;
    cfg_lcd.offset_x       = 0;
    cfg_lcd.offset_y       = 0;
    cfg_lcd.offset_rotation = 0;
    cfg_lcd.pin_cs         = 15;
    cfg_lcd.pin_rst        = -1;
    cfg_lcd.readable       = true;
    cfg_lcd.invert         = true;
    panel(cfg_lcd);

    auto cfg_touch = touch();
    cfg_touch.x_min     = 0;
    cfg_touch.x_max     = 239;
    cfg_touch.y_min     = 0;
    cfg_touch.y_max     = 319;
    cfg_touch.pin_int    = 36;
    cfg_touch.bus_shared = true;
    cfg_touch.spi_host   = VSPI_HOST;
    cfg_touch.freq       = 1000000;
    cfg_touch.pin_sclk   = 14;
    cfg_touch.pin_mosi   = 13;
    cfg_touch.pin_miso   = 12;
    cfg_touch.pin_cs     = 33;
    touch(cfg_touch);

    setRotation(1);
  }
};

LGFX lcd;

const char *VESC_BT_NAME = "VESC_BT"; // TODO: ajustar nome do módulo

void drawTelemetry(float voltage, float current, float rpm, float duty) {
  lcd.fillScreen(TFT_BLACK);
  lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  lcd.setTextSize(2);
  lcd.setCursor(10, 20);
  lcd.printf("V: %.1f V", voltage);
  lcd.setCursor(10, 50);
  lcd.printf("A: %.1f A", current);
  lcd.setCursor(10, 80);
  lcd.printf("RPM: %.0f", rpm);
  lcd.setCursor(10, 110);
  lcd.printf("Duty: %.0f%%", duty * 100.0);
}

void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.fillScreen(TFT_BLACK);
  lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
  lcd.setTextSize(2);
  lcd.setCursor(30, 140);
  lcd.print("Furiosa HUD");

  SerialBT.begin("FuriosaHUD", true);
  Serial.printf("Connecting to \"%s\"...\n", VESC_BT_NAME);
  if (SerialBT.connect(VESC_BT_NAME)) {
    Serial.println("BT connected!");
    vesc.setSerialPort(&SerialBT);
  } else {
    Serial.println("BT connect failed.");
  }
}

void loop() {
  if (SerialBT.connected() && vesc.getVescValues()) {
    drawTelemetry(vesc.data.inpVoltage,
                  vesc.data.avgInputCurrent,
                  vesc.data.rpm,
                  vesc.data.dutyCycle);
  }
  delay(250);
}

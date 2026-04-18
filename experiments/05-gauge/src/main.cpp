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

constexpr int CX        = 160;
constexpr int CY        = 130;
constexpr int R_ARC     = 80;
constexpr int ARC_THICK = 10;
constexpr float MAX_SPD = 35.0f;
constexpr float A_START = M_PI * 1.25f;
constexpr float A_SWEEP = M_PI * 1.5f;

constexpr int NEEDLE_R  = R_ARC - 14;
constexpr int HUB_R     = 6;
constexpr int SPD_X     = CX;
constexpr int SPD_Y     = CY + 35;
constexpr int KMH_Y     = CY + 53;

float spdAngle(float spd) {
  return A_START + (constrain(spd, 0.0f, MAX_SPD) / MAX_SPD) * A_SWEEP;
}

void aXY(float a, int r, int &x, int &y) {
  x = CX + (int)(r * sinf(a));
  y = CY - (int)(r * cosf(a));
}

uint16_t spdColor(float spd) {
  if (spd < 15.0f) return 0x07E0;
  if (spd < 25.0f) return 0xFD00;
  return 0xF800;
}

void drawArcZone(float a0, float a1, uint16_t col) {
  float step = 0.03f;
  int half = ARC_THICK / 2;
  for (float a = a0; a < a1; a += step) {
    int x = CX + (int)(R_ARC * sinf(a));
    int y = CY - (int)(R_ARC * cosf(a));
    lcd.fillCircle(x, y, half, col);
  }
}

void drawStatic() {
  lcd.fillScreen(TFT_BLACK);

  float a15 = spdAngle(15);
  float a25 = spdAngle(25);
  float a35 = spdAngle(35);

  drawArcZone(A_START, a15, 0x07E0);
  drawArcZone(a15, a25, 0xFD00);
  drawArcZone(a25, a35 + 0.04f, 0xF800);

  for (int s = 0; s <= 35; s++) {
    float a = spdAngle(s);
    bool major = (s % 5 == 0);
    int r0 = R_ARC + ARC_THICK / 2 + 3;
    int r1 = r0 + (major ? 12 : 5);
    int x0, y0, x1, y1;
    aXY(a, r0, x0, y0);
    aXY(a, r1, x1, y1);
    lcd.drawLine(x0, y0, x1, y1, major ? TFT_WHITE : 0x632C);

    if (major) {
      int lx, ly;
      aXY(a, r1 + 11, lx, ly);
      lcd.setTextDatum(middle_center);
      lcd.setFont(&fonts::Font2);
      lcd.setTextColor(TFT_WHITE);
      char buf[4];
      snprintf(buf, sizeof(buf), "%d", s);
      lcd.drawString(buf, lx, ly);
    }
  }

  lcd.setTextDatum(middle_center);
  lcd.setFont(&fonts::Font2);
  lcd.setTextColor(0x8410);
  lcd.drawString("km/h", CX, KMH_Y);
}

int prevNx = -1, prevNy = -1;

void eraseNeedle() {
  if (prevNx < 0) return;
  lcd.drawWideLine(CX, CY, prevNx, prevNy, 5, TFT_BLACK);
  lcd.fillCircle(CX, CY, HUB_R + 1, TFT_BLACK);
}

void drawNeedle(float spd) {
  float na = spdAngle(spd);
  int nx, ny;
  aXY(na, NEEDLE_R, nx, ny);

  lcd.startWrite();
  eraseNeedle();
  lcd.drawWideLine(CX, CY, nx, ny, 3, TFT_WHITE);
  lcd.fillCircle(CX, CY, HUB_R, TFT_WHITE);
  lcd.fillCircle(CX, CY, 3, 0x2104);
  lcd.endWrite();

  prevNx = nx;
  prevNy = ny;
}

void drawSpeed(float spd) {
  lcd.fillRect(SPD_X - 40, SPD_Y - 12, 80, 24, TFT_BLACK);
  lcd.setTextDatum(middle_center);
  lcd.setFont(&fonts::Font4);
  lcd.setTextColor(spdColor(spd));
  char buf[8];
  snprintf(buf, sizeof(buf), "%.1f", spd);
  lcd.drawString(buf, SPD_X, SPD_Y);
}

void setup() {
  Serial.begin(115200);
  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);
  lcd.init();
  lcd.setRotation(1);
  drawStatic();
  Serial.println("Gauge ready");
}

void loop() {
  static float spd = 0;
  static float dir = 0.3f;
  spd += dir;
  if (spd >= MAX_SPD) { spd = MAX_SPD; dir = -0.3f; }
  if (spd <= 0)        { spd = 0;       dir =  0.3f; }
  drawSpeed(spd);
  drawNeedle(spd);
  delay(100);
}

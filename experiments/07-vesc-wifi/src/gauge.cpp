#include "gauge.h"
#include "display.h"
#include "config.h"

static constexpr int CX        = 160;
static constexpr int CY        = 135;
static constexpr int R_ARC     = 80;
static constexpr int ARC_THICK = 10;
static constexpr float A_START = M_PI * 1.25f;
static constexpr float A_SWEEP = M_PI * 1.5f;

static constexpr int NEEDLE_R  = R_ARC - 14;
static constexpr int HUB_R     = 6;
static constexpr int SPD_X     = CX;
static constexpr int SPD_Y     = CY + 35;
static constexpr int KMH_Y     = CY + 53;

static int prevNx = -1, prevNy = -1;

static float spdAngle(float spd) {
  return A_START + (constrain(spd, 0.0f, MAX_SPEED_KMH) / MAX_SPEED_KMH) * A_SWEEP;
}

static void aXY(float a, int r, int &x, int &y) {
  x = CX + (int)(r * sinf(a));
  y = CY - (int)(r * cosf(a));
}

static uint16_t spdColor(float spd) {
  if (spd < 15.0f) return 0x07E0;
  if (spd < 25.0f) return 0xFD00;
  return 0xF800;
}

static void drawArcZone(float a0, float a1, uint16_t col) {
  float step = 0.03f;
  int half = ARC_THICK / 2;
  for (float a = a0; a < a1; a += step) {
    int x = CX + (int)(R_ARC * sinf(a));
    int y = CY - (int)(R_ARC * cosf(a));
    lcd.fillCircle(x, y, half, col);
  }
}

void gauge::drawStatic() {
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

void gauge::update(float speed) {
  if (prevNx >= 0) {
    lcd.drawWideLine(CX, CY, prevNx, prevNy, 5, TFT_BLACK);
    lcd.fillCircle(CX, CY, HUB_R + 1, TFT_BLACK);
  }

  float na = spdAngle(speed);
  int nx, ny;
  aXY(na, NEEDLE_R, nx, ny);

  lcd.startWrite();
  lcd.drawWideLine(CX, CY, nx, ny, 3, TFT_WHITE);
  lcd.fillCircle(CX, CY, HUB_R, TFT_WHITE);
  lcd.fillCircle(CX, CY, 3, 0x2104);
  lcd.endWrite();

  prevNx = nx;
  prevNy = ny;

  lcd.fillRect(SPD_X - 40, SPD_Y - 12, 80, 24, TFT_BLACK);
  lcd.setTextDatum(middle_center);
  lcd.setFont(&fonts::Font4);
  lcd.setTextColor(spdColor(speed));
  char buf[8];
  snprintf(buf, sizeof(buf), "%.1f", speed);
  lcd.drawString(buf, SPD_X, SPD_Y);
}

void gauge::reset() {
  prevNx = -1;
  prevNy = -1;
}

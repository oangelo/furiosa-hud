#include "screens.h"
#include "display.h"
#include "gauge.h"
#include "config.h"

static Screen currentScreen = SCREEN_MAIN;
static unsigned long detailEnterTime = 0;
static bool screenDirty = false;
static unsigned long lastTouchTime = 0;
static constexpr unsigned long TOUCH_DEBOUNCE = 300;

static constexpr int TOPBAR_H = 20;

static constexpr int BAT_X = 4;
static constexpr int BAT_Y = 4;
static constexpr int BAT_W = 22;
static constexpr int BAT_H = 12;

static constexpr int BT_CX = 304;
static constexpr int BT_CY = 10;
static constexpr int BT_R  = 4;

static constexpr int DET_COL_X[2] = {80, 240};
static constexpr int DET_LBL_Y[3] = {36, 96, 156};
static constexpr int DET_VAL_Y[3] = {58, 118, 178};
static constexpr int DET_FOOTER_Y = 224;

static void drawBatteryBar(int pct) {
  bool low = pct <= BATTERY_LOW_PCT;
  bool blink = low && ((millis() / 500) % 2 == 0);
  uint16_t fillColor = low ? (blink ? TFT_RED : 0x4208) : 0x07E0;

  lcd.drawRect(BAT_X, BAT_Y, BAT_W, BAT_H, TFT_WHITE);
  lcd.fillRect(BAT_X + BAT_W, BAT_Y + 3, 3, BAT_H - 6, TFT_WHITE);

  int innerW = BAT_W - 2;
  int fillW = (int)(innerW * pct / 100.0f);
  if (fillW > 0)
    lcd.fillRect(BAT_X + 1, BAT_Y + 1, fillW, BAT_H - 2, fillColor);
  if (fillW < innerW)
    lcd.fillRect(BAT_X + 1 + fillW, BAT_Y + 1, innerW - fillW, BAT_H - 2, TFT_BLACK);
}

static void drawBtStatus(BtState btState) {
  uint16_t col;
  switch (btState) {
    case BT_CONNECTED:
      col = 0x07E0;
      break;
    case BT_RECONNECTING:
      col = 0xFFE0;
      break;
    default:
      col = ((millis() / 400) % 2 == 0) ? 0xF800 : 0xFD00;
      break;
  }

  lcd.setTextDatum(middle_right);
  lcd.setFont(&fonts::Font2);
  lcd.setTextColor(col);
  lcd.drawString("BT", BT_CX - 6, BT_CY);
  lcd.fillCircle(BT_CX + 4, BT_CY, BT_R, col);
}

static void drawTopBar(const VescData& data, bool showVoltage) {
  lcd.fillRect(0, 0, 320, TOPBAR_H, TFT_BLACK);

  drawBatteryBar(data.batteryPct);

  lcd.setTextDatum(middle_left);
  lcd.setFont(&fonts::Font2);
  lcd.setTextColor(TFT_WHITE);

  if (data.batteryPct <= BATTERY_LOW_PCT && (millis() / 500) % 2 == 0)
    lcd.setTextColor(TFT_RED);

  char buf[16];
  if (showVoltage) {
    snprintf(buf, sizeof(buf), "%d%%  %.1fV", data.batteryPct, data.voltage);
  } else {
    snprintf(buf, sizeof(buf), "%d%%", data.batteryPct);
  }
  lcd.drawString(buf, BAT_X + BAT_W + 6, BAT_Y + BAT_H / 2);

  drawBtStatus(data.btState);
}

static void drawAlerts(const VescData& data) {
  bool blink = (millis() / 500) % 2 == 0;

  if (data.tempEsc > TEMP_ESC_WARNING) {
    if (blink) {
      lcd.setTextDatum(middle_center);
      lcd.setFont(&fonts::Font2);
      lcd.setTextColor(TFT_RED);
      lcd.drawString("! ESC HOT !", 160, 208);
    } else {
      lcd.fillRect(110, 200, 100, 16, TFT_BLACK);
    }
  }

  if (data.tempMotor > TEMP_MOTOR_WARNING) {
    if (blink) {
      lcd.setTextDatum(middle_center);
      lcd.setFont(&fonts::Font2);
      lcd.setTextColor(TFT_RED);
      lcd.drawString("! MOT HOT !", 160, 224);
    } else {
      lcd.fillRect(110, 216, 100, 16, TFT_BLACK);
    }
  }
}

static void drawDetailStatic() {
  static const char* labels[3][2] = {
    {"TRIP", "POWER"},
    {"ESC TEMP", "MOTOR"},
    {"CURRENT", "DUTY"}
  };

  lcd.setTextDatum(middle_center);
  lcd.setFont(&fonts::Font2);
  lcd.setTextColor(0x8410);

  for (int r = 0; r < 3; r++) {
    for (int c = 0; c < 2; c++) {
      lcd.drawString(labels[r][c], DET_COL_X[c], DET_LBL_Y[r]);
    }
  }

  lcd.setTextColor(0x632C);
  lcd.drawString("toque p/ voltar", 160, DET_FOOTER_Y);
}

static void drawDetailValue(int col, int row, const char* text, uint16_t color) {
  int x = DET_COL_X[col];
  int y = DET_VAL_Y[row];

  lcd.fillRect(x - 65, y - 14, 130, 28, TFT_BLACK);
  lcd.setTextDatum(middle_center);
  lcd.setFont(&fonts::Font4);
  lcd.setTextColor(color);
  lcd.drawString(text, x, y);
}

static void drawDetailError(int error) {
  lcd.fillRect(60, 100, 200, 28, TFT_BLACK);
  lcd.setTextDatum(middle_center);
  lcd.setFont(&fonts::Font4);
  lcd.setTextColor(TFT_RED);
  char buf[24];
  snprintf(buf, sizeof(buf), "FAULT: %d", error);
  lcd.drawString(buf, 160, 114);
}

static void updateDetailValues(const VescData& data) {
  char buf[16];

  if (data.error != 0) {
    drawDetailError(data.error);
  } else {
    snprintf(buf, sizeof(buf), "%.1f km", data.tripKm);
    drawDetailValue(0, 0, buf, TFT_WHITE);

    float power = data.voltage * data.current;
    snprintf(buf, sizeof(buf), "%.0f W", power);
    drawDetailValue(1, 0, buf, TFT_WHITE);
  }

  snprintf(buf, sizeof(buf), "%.0f C", data.tempEsc);
  uint16_t escCol = data.tempEsc > TEMP_ESC_WARNING ? TFT_RED : TFT_WHITE;
  drawDetailValue(0, 1, buf, escCol);

  if (data.tempMotor < 0) {
    drawDetailValue(1, 1, "N/A", 0x8410);
  } else {
    snprintf(buf, sizeof(buf), "%.0f C", data.tempMotor);
    uint16_t motCol = data.tempMotor > TEMP_MOTOR_WARNING ? TFT_RED : TFT_WHITE;
    drawDetailValue(1, 1, buf, motCol);
  }

  snprintf(buf, sizeof(buf), "%.1f A", data.current);
  drawDetailValue(0, 2, buf, TFT_WHITE);

  snprintf(buf, sizeof(buf), "%.0f%%", data.duty);
  drawDetailValue(1, 2, buf, TFT_WHITE);
}

void screens::init() {
  currentScreen = SCREEN_MAIN;
  screenDirty = true;
  lcd.fillScreen(TFT_BLACK);
  gauge::drawStatic();
  gauge::reset();
}

void screens::update(const VescData& data) {
  if (currentScreen == SCREEN_DETAIL &&
      millis() - detailEnterTime >= DETAIL_SCREEN_TIMEOUT) {
    currentScreen = SCREEN_MAIN;
    screenDirty = true;
  }

  if (screenDirty) {
    lcd.fillScreen(TFT_BLACK);
    if (currentScreen == SCREEN_MAIN) {
      gauge::drawStatic();
      gauge::reset();
    } else {
      drawDetailStatic();
    }
    screenDirty = false;
  }

  if (currentScreen == SCREEN_MAIN) {
    drawTopBar(data, false);
    float displaySpeed = (data.btState == BT_CONNECTED) ? data.speed : 0.0f;
    gauge::update(displaySpeed);
    drawAlerts(data);
  } else {
    drawTopBar(data, true);
    updateDetailValues(data);
  }
}

void screens::handleTouch() {
  int16_t tx, ty;
  bool touched = lcd.getTouch(&tx, &ty);

  if (touched && millis() - lastTouchTime > TOUCH_DEBOUNCE) {
    lastTouchTime = millis();

    if (currentScreen == SCREEN_MAIN) {
      currentScreen = SCREEN_DETAIL;
      detailEnterTime = millis();
    } else {
      currentScreen = SCREEN_MAIN;
    }
    screenDirty = true;
  }
}

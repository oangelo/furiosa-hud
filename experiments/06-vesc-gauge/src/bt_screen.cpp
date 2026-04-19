#include "bt_screen.h"
#include "display.h"
#include "config.h"

static constexpr int HEADER_H     = 30;
static constexpr int ROW_H       = 40;
static constexpr int ROW_START_Y = HEADER_H + 8;
static constexpr int ROW_PAD     = 4;
static constexpr int BTN_H       = 36;
static constexpr int BTN_Y       = 240 - BTN_H - 4;
static constexpr int BTN_X       = 80;
static constexpr int BTN_W       = 160;

static constexpr uint16_t COL_BG       = TFT_BLACK;
static constexpr uint16_t COL_HEADER   = 0x18E3;
static constexpr uint16_t COL_ROW_BG   = 0x2104;
static constexpr uint16_t COL_TEXT     = TFT_WHITE;
static constexpr uint16_t COL_DIM      = 0x8410;
static constexpr uint16_t COL_GREEN    = 0x07E0;
static constexpr uint16_t COL_BT_BLUE  = 0x1BF5;
static constexpr uint16_t COL_BTN_BG   = 0x3186;
static constexpr uint16_t COL_BTN_TXT  = TFT_WHITE;

static void drawHeader() {
  lcd.fillRect(0, 0, 320, HEADER_H, COL_HEADER);
  lcd.setTextDatum(middle_left);
  lcd.setFont(&fonts::Font2);
  lcd.setTextColor(COL_TEXT);
  lcd.drawString("Furiosa HUD", 8, HEADER_H / 2);

  lcd.setTextDatum(middle_right);
  lcd.setTextColor(COL_BT_BLUE);
  lcd.drawString("BT", 310, HEADER_H / 2);
}

void bt_screen::drawScanningStatic() {
  lcd.fillScreen(COL_BG);
  drawHeader();

  lcd.setTextDatum(middle_center);
  lcd.setFont(&fonts::Font4);
  lcd.setTextColor(COL_TEXT);
  lcd.drawString("Procurando...", 160, 100);
}

void bt_screen::updateScanningDots(unsigned long elapsed) {
  lcd.fillRect(120, 120, 80, 16, COL_BG);
  lcd.fillRect(100, 150, 120, 16, COL_BG);

  int dots = (elapsed / 500) % 4;
  char dotBuf[8];
  for (int i = 0; i < dots; i++) dotBuf[i] = '.';
  dotBuf[dots] = '\0';
  lcd.setTextDatum(middle_center);
  lcd.setFont(&fonts::Font4);
  lcd.setTextColor(COL_TEXT);
  lcd.drawString(dotBuf, 160, 128);

  char buf[32];
  unsigned long sec = elapsed / 1000;
  unsigned long total = BT_SCAN_TIMEOUT_MS / 1000;
  unsigned long m = sec / 60, s = sec % 60;
  unsigned long tm = total / 60, ts = total % 60;
  snprintf(buf, sizeof(buf), "%lum %lus / %lum %lus", m, s, tm, ts);
  lcd.setFont(&fonts::Font2);
  lcd.setTextColor(COL_DIM);
  lcd.drawString(buf, 160, 158);
}

void bt_screen::drawDeviceListStatic(BtDevice* devices, int count) {
  lcd.fillScreen(COL_BG);
  drawHeader();

  if (count == 0) {
    lcd.setTextDatum(middle_center);
    lcd.setFont(&fonts::Font4);
    lcd.setTextColor(TFT_RED);
    lcd.drawString("Nenhum dispositivo", 160, 100);
    lcd.setFont(&fonts::Font2);
    lcd.setTextColor(COL_DIM);
    lcd.drawString("Toque p/ escanear", 160, 140);
    return;
  }

  for (int i = 0; i < count; i++) {
    int y = ROW_START_Y + i * (ROW_H + ROW_PAD);

    lcd.fillRoundRect(8, y, 304, ROW_H, 6, COL_ROW_BG);

    char idx[4];
    snprintf(idx, sizeof(idx), "%d", i);
    lcd.setTextDatum(middle_left);
    lcd.setFont(&fonts::Font2);
    lcd.setTextColor(COL_TEXT);
    lcd.drawString(idx, 18, y + ROW_H / 2);

    if (devices[i].hasName) {
      lcd.setFont(&fonts::Font2);
      lcd.setTextColor(COL_GREEN);
      lcd.drawString(devices[i].name.c_str(), 42, y + ROW_H / 2 - 7);

      lcd.setFont(&fonts::Font0);
      lcd.setTextColor(COL_DIM);
      lcd.drawString(devices[i].address.c_str(), 42, y + ROW_H / 2 + 8);
    } else {
      lcd.setFont(&fonts::Font2);
      lcd.setTextColor(COL_DIM);
      lcd.drawString("(sem nome)", 42, y + ROW_H / 2 - 7);

      lcd.setFont(&fonts::Font0);
      lcd.setTextColor(COL_DIM);
      lcd.drawString(devices[i].address.c_str(), 42, y + ROW_H / 2 + 8);
    }
  }

  lcd.fillRoundRect(BTN_X, BTN_Y, BTN_W, BTN_H, 8, COL_BTN_BG);
  lcd.setTextDatum(middle_center);
  lcd.setFont(&fonts::Font2);
  lcd.setTextColor(COL_BTN_TXT);
  lcd.drawString("Re-escanear", BTN_X + BTN_W / 2, BTN_Y + BTN_H / 2);
}

void bt_screen::drawConnectingStatic(const char* name) {
  lcd.fillScreen(COL_BG);
  drawHeader();

  lcd.setTextDatum(middle_center);
  lcd.setFont(&fonts::Font4);
  lcd.setTextColor(COL_BT_BLUE);
  lcd.drawString("Conectando...", 160, 90);

  lcd.setFont(&fonts::Font2);
  lcd.setTextColor(COL_TEXT);
  lcd.drawString(name, 160, 130);
}

void bt_screen::drawConnectedStatic(const char* name) {
  lcd.fillScreen(COL_BG);
  drawHeader();

  lcd.setTextDatum(middle_center);
  lcd.setFont(&fonts::Font4);
  lcd.setTextColor(COL_GREEN);
  lcd.drawString("Conectado!", 160, 80);

  lcd.setFont(&fonts::Font2);
  lcd.setTextColor(COL_TEXT);
  lcd.drawString(name, 160, 115);

  lcd.setFont(&fonts::Font0);
  lcd.setTextColor(COL_DIM);
  lcd.drawString("Dados no Serial Monitor", 160, 145);
}

void bt_screen::drawDisconnectedStatic() {
  lcd.fillScreen(COL_BG);
  drawHeader();

  lcd.setTextDatum(middle_center);
  lcd.setFont(&fonts::Font4);
  lcd.setTextColor(TFT_RED);
  lcd.drawString("Desconectado", 160, 100);

  lcd.setFont(&fonts::Font2);
  lcd.setTextColor(COL_DIM);
  lcd.drawString("Reconectando...", 160, 140);
}

int bt_screen::handleTouch(BtDevice* devices, int count) {
  int16_t tx, ty;
  if (!lcd.getTouch(&tx, &ty)) return -1;

  if (tx >= BTN_X && tx <= BTN_X + BTN_W && ty >= BTN_Y && ty <= BTN_Y + BTN_H) {
    return -2;
  }

  for (int i = 0; i < count; i++) {
    int y = ROW_START_Y + i * (ROW_H + ROW_PAD);
    if (tx >= 8 && tx <= 312 && ty >= y && ty <= y + ROW_H) {
      return i;
    }
  }

  return -1;
}

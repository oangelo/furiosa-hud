#include "bt_screen.h"
#include "display.h"
#include "config.h"

static constexpr int HEADER_H     = 30;
static constexpr int ROW_H       = 40;
static constexpr int ROW_START_Y = HEADER_H + 8;
static constexpr int ROW_PAD     = 4;
static constexpr int MAX_VISIBLE = 3;

static constexpr int BTN_H       = 36;
static constexpr int BTN_Y       = 240 - BTN_H - 4;

static constexpr int TOG_W       = 100;
static constexpr int TOG_X       = 4;

static constexpr int SCAN_W      = 100;
static constexpr int SCAN_X      = 110;

static constexpr int SCR_W       = 36;
static constexpr int SCR_UP_X    = 244;
static constexpr int SCR_DN_X    = 284;

static constexpr uint16_t COL_BG       = TFT_BLACK;
static constexpr uint16_t COL_HEADER   = 0x18E3;
static constexpr uint16_t COL_ROW_BG   = 0x2104;
static constexpr uint16_t COL_TEXT     = TFT_WHITE;
static constexpr uint16_t COL_DIM      = 0x8410;
static constexpr uint16_t COL_GREEN    = 0x07E0;
static constexpr uint16_t COL_BT_BLUE  = 0x1BF5;
static constexpr uint16_t COL_BTN_BG   = 0x3186;
static constexpr uint16_t COL_BTN_TXT  = TFT_WHITE;
static constexpr uint16_t COL_TOG_BG   = 0x04DF;
static constexpr uint16_t COL_TOG_TXT  = TFT_WHITE;

static constexpr uint16_t COL_FLASH_TOG  = 0x7FFF;
static constexpr uint16_t COL_FLASH_BTN  = 0x7BEF;
static constexpr uint16_t COL_FLASH_ROW  = 0x5ACB;
static constexpr uint16_t COL_FLASH_SCR  = 0xB5B6;

static int scrollOffset = 0;

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

static void drawBottomBar(BtType btType, int count) {
  lcd.fillRoundRect(TOG_X, BTN_Y, TOG_W, BTN_H, 8, COL_TOG_BG);
  lcd.setTextDatum(middle_center);
  lcd.setFont(&fonts::Font2);
  lcd.setTextColor(COL_TOG_TXT);
  const char* modeLabel = btType == BT_TYPE_CLASSIC ? "Classic" : "BLE";
  lcd.drawString(modeLabel, TOG_X + TOG_W / 2, BTN_Y + BTN_H / 2);

  lcd.fillRoundRect(SCAN_X, BTN_Y, SCAN_W, BTN_H, 8, COL_BTN_BG);
  lcd.setTextDatum(middle_center);
  lcd.setFont(&fonts::Font2);
  lcd.setTextColor(COL_BTN_TXT);
  lcd.drawString("Scan", SCAN_X + SCAN_W / 2, BTN_Y + BTN_H / 2);

  if (count > MAX_VISIBLE) {
    lcd.fillRoundRect(SCR_UP_X, BTN_Y, SCR_W, BTN_H, 8, COL_BTN_BG);
    lcd.setTextDatum(middle_center);
    lcd.setFont(&fonts::Font2);
    lcd.setTextColor(COL_BTN_TXT);
    lcd.drawString("^", SCR_UP_X + SCR_W / 2, BTN_Y + BTN_H / 2);

    lcd.fillRoundRect(SCR_DN_X, BTN_Y, SCR_W, BTN_H, 8, COL_BTN_BG);
    lcd.setTextDatum(middle_center);
    lcd.setFont(&fonts::Font2);
    lcd.setTextColor(COL_BTN_TXT);
    lcd.drawString("v", SCR_DN_X + SCR_W / 2, BTN_Y + BTN_H / 2);

    int page = scrollOffset / MAX_VISIBLE + 1;
    int totalPages = (count + MAX_VISIBLE - 1) / MAX_VISIBLE;
    char pg[8];
    snprintf(pg, sizeof(pg), "%d/%d", page, totalPages);
    lcd.setFont(&fonts::Font0);
    lcd.setTextColor(COL_DIM);
    lcd.setTextDatum(middle_center);
    lcd.drawString(pg, (SCAN_X + SCAN_W + SCR_UP_X) / 2, BTN_Y + BTN_H / 2);
  }
}

void bt_screen::drawDeviceListStatic(BtDevice* devices, int count, BtType btType) {
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
    drawBottomBar(btType, 0);
    return;
  }

  int maxVis = min(MAX_VISIBLE, count - scrollOffset);
  for (int v = 0; v < maxVis; v++) {
    int i = scrollOffset + v;
    int y = ROW_START_Y + v * (ROW_H + ROW_PAD);

    lcd.fillRoundRect(8, y, 304, ROW_H, 6, COL_ROW_BG);

    char idx[4];
    snprintf(idx, sizeof(idx), "%d", i + 1);
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

    char tagBuf[16];
    const char* typeStr = devices[i].type == BT_TYPE_BLE ? "BLE" : "BT";
    snprintf(tagBuf, sizeof(tagBuf), "%s %d", typeStr, devices[i].rssi);
    lcd.setTextDatum(middle_right);
    lcd.setFont(&fonts::Font0);
    lcd.setTextColor(COL_BT_BLUE);
    lcd.drawString(tagBuf, 310, y + ROW_H / 2);
  }

  drawBottomBar(btType, count);
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

  if (tx >= TOG_X && tx <= TOG_X + TOG_W && ty >= BTN_Y && ty <= BTN_Y + BTN_H) {
    return -3;
  }

  if (tx >= SCAN_X && tx <= SCAN_X + SCAN_W && ty >= BTN_Y && ty <= BTN_Y + BTN_H) {
    return -2;
  }

  if (count > MAX_VISIBLE) {
    if (tx >= SCR_UP_X && tx <= SCR_UP_X + SCR_W && ty >= BTN_Y && ty <= BTN_Y + BTN_H) {
      return -4;
    }
    if (tx >= SCR_DN_X && tx <= SCR_DN_X + SCR_W && ty >= BTN_Y && ty <= BTN_Y + BTN_H) {
      return -5;
    }
  }

  int maxVis = min(MAX_VISIBLE, count - scrollOffset);
  for (int v = 0; v < maxVis; v++) {
    int y = ROW_START_Y + v * (ROW_H + ROW_PAD);
    if (tx >= 8 && tx <= 312 && ty >= y && ty <= y + ROW_H) {
      return scrollOffset + v;
    }
  }

  return -1;
}

void bt_screen::flashButton(int touchResult, BtDevice* devices, int count) {
  if (touchResult == -3) {
    lcd.fillRoundRect(TOG_X, BTN_Y, TOG_W, BTN_H, 8, COL_FLASH_TOG);
    delay(80);
  } else if (touchResult == -2) {
    lcd.fillRoundRect(SCAN_X, BTN_Y, SCAN_W, BTN_H, 8, COL_FLASH_BTN);
    delay(80);
  } else if (touchResult == -4) {
    lcd.fillRoundRect(SCR_UP_X, BTN_Y, SCR_W, BTN_H, 8, COL_FLASH_SCR);
    delay(80);
  } else if (touchResult == -5) {
    lcd.fillRoundRect(SCR_DN_X, BTN_Y, SCR_W, BTN_H, 8, COL_FLASH_SCR);
    delay(80);
  } else if (touchResult >= 0) {
    int vis = touchResult - scrollOffset;
    if (vis >= 0 && vis < MAX_VISIBLE) {
      int y = ROW_START_Y + vis * (ROW_H + ROW_PAD);
      lcd.fillRoundRect(8, y, 304, ROW_H, 6, COL_FLASH_ROW);
      delay(80);
    }
  }
}

void bt_screen::resetScroll() {
  scrollOffset = 0;
}

void bt_screen::scrollUp(int count) {
  if (scrollOffset >= MAX_VISIBLE) {
    scrollOffset -= MAX_VISIBLE;
  }
}

void bt_screen::scrollDown(int count) {
  if (scrollOffset + MAX_VISIBLE < count) {
    scrollOffset += MAX_VISIBLE;
  }
}

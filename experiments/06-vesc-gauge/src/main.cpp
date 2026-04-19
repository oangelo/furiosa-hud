#include <Arduino.h>
#include "config.h"
#include "display.h"
#include "vesc_bt.h"
#include "bt_screen.h"

LGFX lcd;

enum State { SCANNING, LIST, CONNECTING, CONNECTED, RECONNECTING };
static State state = SCANNING;
static unsigned long stateEnterTime = 0;
static int selectedIndex = -1;
static unsigned long lastReconnectAttempt = 0;
static bool screenDirty = true;

static void enterState(State s) {
  state = s;
  stateEnterTime = millis();
  screenDirty = true;
  Serial.printf("[STATE] -> %d\n", (int)s);
}

void setup() {
  Serial.begin(115200);
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);
  lcd.init();
  lcd.setRotation(DISPLAY_ROTATION);

  vesc_bt::init();
  vesc_bt::startScan();
  enterState(SCANNING);
  Serial.println("06-vesc-gauge BT ready");
}

void loop() {
  unsigned long now = millis();

  switch (state) {
    case SCANNING: {
      if (screenDirty) {
        bt_screen::drawScanningStatic();
        screenDirty = false;
      }
      bt_screen::updateScanningDots(now - stateEnterTime);

      if (vesc_bt::isScanComplete()) {
        int count = vesc_bt::getDeviceCount();
        int vescIdx = vesc_bt::findDeviceByName(VESC_BT_NAME);
        if (vescIdx >= 0) {
          Serial.printf("[BT] Auto-connecting to %s\n", VESC_BT_NAME);
          selectedIndex = vescIdx;
          const char* name = vesc_bt::getDevice(vescIdx).hasName
            ? vesc_bt::getDevice(vescIdx).name.c_str() : vesc_bt::getDevice(vescIdx).address.c_str();
          bt_screen::drawConnectingStatic(name);
          screenDirty = false;
          enterState(CONNECTING);
        } else {
          enterState(LIST);
        }
      }
      break;
    }

    case LIST: {
      if (screenDirty) {
        int count = vesc_bt::getDeviceCount();
        BtDevice devs[BT_MAX_DEVICES];
        for (int i = 0; i < count; i++) devs[i] = vesc_bt::getDevice(i);
        bt_screen::drawDeviceListStatic(devs, count);
        screenDirty = false;
      }

      int count = vesc_bt::getDeviceCount();
      BtDevice devs[BT_MAX_DEVICES];
      for (int i = 0; i < count; i++) devs[i] = vesc_bt::getDevice(i);
      int touch = bt_screen::handleTouch(devs, count);

      if (touch == -2) {
        vesc_bt::startScan();
        enterState(SCANNING);
      } else if (touch >= 0) {
        selectedIndex = touch;
        const char* name = devs[touch].hasName
          ? devs[touch].name.c_str() : devs[touch].address.c_str();
        enterState(CONNECTING);
      }
      break;
    }

    case CONNECTING: {
      if (screenDirty) {
        const BtDevice& dev = vesc_bt::getDevice(selectedIndex);
        const char* name = dev.hasName ? dev.name.c_str() : dev.address.c_str();
        bt_screen::drawConnectingStatic(name);
        screenDirty = false;
      }

      if (vesc_bt::connectByIndex(selectedIndex)) {
        enterState(CONNECTED);
      } else {
        enterState(LIST);
      }
      break;
    }

    case CONNECTED: {
      if (screenDirty) {
        const BtDevice& dev = vesc_bt::getDevice(selectedIndex);
        const char* name = dev.hasName ? dev.name.c_str() : dev.address.c_str();
        bt_screen::drawConnectedStatic(name);
        screenDirty = false;
      }

      if (!vesc_bt::isConnected()) {
        Serial.println("[BT] Lost connection");
        lastReconnectAttempt = now;
        enterState(RECONNECTING);
        break;
      }

      VescData data = {};
      vesc_bt::read(data);
      break;
    }

    case RECONNECTING: {
      if (screenDirty) {
        bt_screen::drawDisconnectedStatic();
        screenDirty = false;
      }

      if (now - lastReconnectAttempt >= BT_RECONNECT_MS) {
        lastReconnectAttempt = now;
        Serial.println("[BT] Attempting reconnect...");

        if (vesc_bt::lastConnectedAddress.length() > 0) {
          if (vesc_bt::connectByIndex(selectedIndex)) {
            enterState(CONNECTED);
          }
        } else {
          vesc_bt::startScan();
          enterState(SCANNING);
        }
      }
      break;
    }
  }

  delay(DISPLAY_UPDATE_MS);
}

#include <Arduino.h>
#include "config.h"
#include "display.h"
#include "screens.h"

#if SIMULATE_VESC

LGFX lcd;

static VescData simData = {};
static float simSpeedDir = 0.5f;
static float simTempPhase = 0;

static void updateSimulation() {
  simData.speed += simSpeedDir;
  if (simData.speed >= MAX_SPEED_KMH) { simData.speed = MAX_SPEED_KMH; simSpeedDir = -0.5f; }
  if (simData.speed <= 0)             { simData.speed = 0;             simSpeedDir =  0.5f; }

  simData.voltage = 37.5f + 4.5f * sinf(millis() / 10000.0f);
  float vClamped = constrain(simData.voltage, 33.0f, 42.0f);
  simData.batteryPct = (int)((vClamped - 33.0f) / 9.0f * 100.0f);

  simTempPhase += 0.015f;
  if (simTempPhase > 2.0f) simTempPhase = 0;
  float tBase;
  if (simTempPhase < 1.0f)
    tBase = 25.0f + simTempPhase * 70.0f;
  else
    tBase = 95.0f - (simTempPhase - 1.0f) * 70.0f;

  simData.tempEsc   = tBase;
  simData.tempMotor = tBase + 12.0f;

  simData.current = simData.speed * 0.42f;
  simData.duty    = (simData.speed / MAX_SPEED_KMH) * 95.0f;

  simData.tripKm += simData.speed / 36000.0f;

  simData.btState = BT_CONNECTED;
  simData.error = 0;
}

void setup() {
  Serial.begin(115200);
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);
  lcd.init();
  lcd.setRotation(DISPLAY_ROTATION);

  simData.speed       = 0;
  simData.voltage     = 37.5f;
  simData.tempEsc     = 30.0f;
  simData.tempMotor   = 28.0f;
  simData.current     = 0;
  simData.duty        = 0;
  simData.tripKm      = 0;
  simData.batteryPct  = 75;
  simData.btState     = BT_CONNECTED;
  simData.ampHours    = 0;
  simData.tachometerAbs = 0;
  simData.error       = 0;

  screens::init();
  Serial.println("06-vesc-gauge SIMULATE mode ready");
}

void loop() {
  updateSimulation();
  screens::handleTouch();
  screens::update(simData);
  delay(DISPLAY_UPDATE_MS);
}

#else

#include "vesc_bt.h"
#include "bt_screen.h"

LGFX lcd;

enum State { SCANNING, LIST, CONNECTING, DASHBOARD, RECONNECTING };
static State state = SCANNING;
static unsigned long stateEnterTime = 0;
static int selectedIndex = -1;
static unsigned long lastReconnectAttempt = 0;
static bool screenDirty = true;
static unsigned long lastVescPoll = 0;
static VescData vescData = {};

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

  vescData.speed       = 0;
  vescData.voltage     = 0;
  vescData.tempEsc     = 0;
  vescData.tempMotor   = 0;
  vescData.current     = 0;
  vescData.duty        = 0;
  vescData.tripKm      = 0;
  vescData.batteryPct  = 0;
  vescData.btState     = BT_OFF;
  vescData.ampHours    = 0;
  vescData.tachometerAbs = 0;
  vescData.error       = 0;

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
        bt_screen::drawDeviceListStatic(devs, count, vesc_bt::getBtType());
        screenDirty = false;
      }

      int count = vesc_bt::getDeviceCount();
      BtDevice devs[BT_MAX_DEVICES];
      for (int i = 0; i < count; i++) devs[i] = vesc_bt::getDevice(i);
      int touch = bt_screen::handleTouch(devs, count);

      if (touch == -3) {
        BtType newType = (vesc_bt::getBtType() == BT_TYPE_CLASSIC) ? BT_TYPE_BLE : BT_TYPE_CLASSIC;
        vesc_bt::setBtType(newType);
        vesc_bt::startScan();
        enterState(SCANNING);
      } else if (touch == -2) {
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
        vescData.btState = BT_CONNECTED;
        screens::init();
        lastVescPoll = now;
        enterState(DASHBOARD);
      } else {
        enterState(LIST);
      }
      break;
    }

    case DASHBOARD: {
      if (!vesc_bt::isConnected()) {
        Serial.println("[BT] Lost connection");
        vescData.speed = 0;
        vescData.btState = BT_RECONNECTING;
        lastReconnectAttempt = now;
        enterState(RECONNECTING);
        break;
      }

      if (now - lastVescPoll >= VESC_POLL_MS) {
        lastVescPoll = now;
        VescData freshData = {};
        if (vesc_bt::read(freshData)) {
          vescData = freshData;
          vescData.btState = BT_CONNECTED;
        }
      }

      screens::handleTouch();
      screens::update(vescData);
      break;
    }

    case RECONNECTING: {
      if (screenDirty) {
        screens::update(vescData);
        screenDirty = false;
      }

      screens::handleTouch();
      screens::update(vescData);

      if (now - lastReconnectAttempt >= BT_RECONNECT_MS) {
        lastReconnectAttempt = now;
        Serial.println("[BT] Attempting reconnect...");

        if (vesc_bt::lastConnectedAddress.length() > 0) {
          if (vesc_bt::connectByIndex(selectedIndex)) {
            vescData.btState = BT_CONNECTED;
            enterState(DASHBOARD);
          }
        } else {
          vesc_bt::startScan();
          vescData.btState = BT_OFF;
          enterState(SCANNING);
        }
      }
      break;
    }
  }

  delay(DISPLAY_UPDATE_MS);
}

#endif

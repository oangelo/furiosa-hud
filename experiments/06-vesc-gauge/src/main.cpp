#include <Arduino.h>
#include "config.h"
#include "display.h"
#include "screens.h"

LGFX lcd;

static VescData simData = {
  .speed       = 0,
  .voltage     = 37.5f,
  .tempEsc     = 30.0f,
  .tempMotor   = 28.0f,
  .current     = 0,
  .duty        = 0,
  .tripKm      = 0,
  .batteryPct  = 75,
  .btConnected = true
};

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
}

void setup() {
  Serial.begin(115200);
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);
  pinMode(TOUCH_PIN, INPUT_PULLUP);

  lcd.init();
  lcd.setRotation(DISPLAY_ROTATION);

  screens::init();
  Serial.println("06-vesc-gauge ready");
}

void loop() {
  updateSimulation();
  screens::handleTouch();
  screens::update(simData);
  delay(DISPLAY_UPDATE_MS);
}

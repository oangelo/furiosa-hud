#pragma once
#include <Arduino.h>

enum BtState { BT_OFF, BT_CONNECTED, BT_RECONNECTING };

struct VescData {
  float speed;
  float voltage;
  float tempEsc;
  float tempMotor;
  float current;
  float duty;
  float tripKm;
  int batteryPct;
  BtState btState;
  float ampHours;
  long tachometerAbs;
  int error;
};

enum Screen { SCREEN_MAIN, SCREEN_DETAIL };

namespace screens {
  void init();
  void update(const VescData& data);
  void handleTouch();
}

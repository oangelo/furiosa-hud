#pragma once
#include <Arduino.h>

struct VescData {
  float speed;
  float voltage;
  float tempEsc;
  float tempMotor;
  float current;
  float duty;
  float tripKm;
  int batteryPct;
  bool btConnected;
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

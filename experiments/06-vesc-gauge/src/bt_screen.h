#pragma once
#include <Arduino.h>
#include "vesc_bt.h"

namespace bt_screen {
  void drawScanningStatic();
  void updateScanningDots(unsigned long elapsed);

  void drawDeviceListStatic(BtDevice* devices, int count, BtType btType);
  void drawConnectingStatic(const char* name);
  void drawConnectedStatic(const char* name);
  void drawDisconnectedStatic();

  int handleTouch(BtDevice* devices, int count);
}

#pragma once
#include <Arduino.h>

enum BtType { BT_TYPE_CLASSIC, BT_TYPE_BLE };

struct BtDevice {
  String name;
  String address;
  bool hasName;
  BtType type;
  int16_t rssi;
};

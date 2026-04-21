#pragma once
#include <Arduino.h>

enum ConnType { CONN_CLASSIC, CONN_BLE, CONN_WIFI };

struct BtDevice {
  String name;
  String address;
  bool hasName;
  ConnType type;
  int16_t rssi;
};

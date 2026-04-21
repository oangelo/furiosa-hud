#pragma once
#include <Arduino.h>
#include "transport.h"
#include "config.h"

namespace ble_transport {
  void init();
  void startScan();
  bool isScanComplete();
  int getDeviceCount();
  const BtDevice& getDevice(int index);
  int findDeviceByName(const char* name);
  bool connectByIndex(int index);
  bool connectByAddress(const char* address);
  bool isConnected();
  Stream* getStream();
  void disconnect();
}

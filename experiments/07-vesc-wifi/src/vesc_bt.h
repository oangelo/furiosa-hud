#pragma once
#include <Arduino.h>
#include "transport.h"
#include "screens.h"

namespace vesc_bt {
  void init();
  void setConnType(ConnType type);
  ConnType getConnType();

  void startScan();
  bool isScanComplete();
  int getDeviceCount();
  const BtDevice& getDevice(int index);
  int findDeviceByName(const char* name);

  bool connectByName(const char* name);
  bool connectByIndex(int index);
  bool isConnected();
  bool read(VescData& data);
  void disconnect();

  extern String lastConnectedAddress;
}

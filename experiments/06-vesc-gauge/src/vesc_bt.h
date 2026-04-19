#pragma once
#include <Arduino.h>
#include "screens.h"

struct BtDevice {
  String name;
  String address;
  bool hasName;
};

namespace vesc_bt {
  void init();
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

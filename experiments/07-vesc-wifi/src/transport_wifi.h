#pragma once
#include <Arduino.h>
#include "transport.h"
#include "config.h"

namespace wifi_transport {
  void init();
  bool connect();
  bool isConnected();
  Stream* getStream();
  void disconnect();
}

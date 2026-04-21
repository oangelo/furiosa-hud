#include "vesc_bt.h"
#include "config.h"
#include "BluetoothSerial.h"
#include <VescUart.h>
#include "transport_ble.h"
#include "transport_wifi.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled!
#endif

static BluetoothSerial SerialBT;
static VescUart vesc;

static ConnType currentConnType = CONN_CLASSIC;
static BtDevice devices[BT_MAX_DEVICES];
static int deviceCount = 0;
static bool scanDone = false;

String vesc_bt::lastConnectedAddress = "";

static const char* connTypeName(ConnType t) {
  switch (t) {
    case CONN_CLASSIC: return "Classic";
    case CONN_BLE:     return "BLE";
    case CONN_WIFI:    return "WiFi";
    default:           return "?";
  }
}

static void sortByRssi(BtDevice* devs, int count) {
  for (int i = 0; i < count - 1; i++) {
    for (int j = i + 1; j < count; j++) {
      if (devs[j].rssi > devs[i].rssi) {
        BtDevice tmp = devs[i];
        devs[i] = devs[j];
        devs[j] = tmp;
      }
    }
  }
}

static void onConfirmRequest(uint32_t numVal) {
  Serial.printf("[BT] SSP confirm request: %lu — auto-accepting\n", numVal);
  SerialBT.confirmReply(true);
}

void vesc_bt::init() {
  SerialBT.enableSSP();
  SerialBT.onConfirmRequest(onConfirmRequest);
  SerialBT.begin(BT_LOCAL_NAME, true);
  ble_transport::init();
  wifi_transport::init();
  Serial.println("[CONN] Initialized (Classic + BLE + WiFi)");
}

void vesc_bt::setConnType(ConnType type) {
  if (currentConnType == type) return;
  currentConnType = type;
  Serial.printf("[CONN] Mode changed to %s\n", connTypeName(type));
}

ConnType vesc_bt::getConnType() { return currentConnType; }

void vesc_bt::startScan() {
  deviceCount = 0;
  scanDone = false;

  if (currentConnType == CONN_WIFI) {
    scanDone = true;
    BtDevice dev;
    dev.name = VESC_WIFI_SSID;
    dev.address = VESC_WIFI_HOST;
    dev.hasName = true;
    dev.type = CONN_WIFI;
    dev.rssi = 0;
    devices[0] = dev;
    deviceCount = 1;
    Serial.printf("[WIFI] Virtual device added\n");
    return;
  }

  if (currentConnType == CONN_CLASSIC) {
    Serial.println("[BT] Starting Classic scan...");
    BTScanResults* results = SerialBT.discover(BT_SCAN_TIMEOUT_MS);
    if (results) {
      int count = results->getCount();
      Serial.printf("[BT] Scan returned %d raw results\n", count);
      for (int i = 0; i < count && deviceCount < BT_MAX_DEVICES; i++) {
        BTAdvertisedDevice* dev = results->getDevice(i);
        if (!dev) continue;

        BtDevice btDev;
        btDev.address = dev->getAddress().toString().c_str();
        btDev.hasName = dev->haveName();
        btDev.name = btDev.hasName ? dev->getName().c_str() : "";
        btDev.type = CONN_CLASSIC;
        btDev.rssi = dev->getRSSI();
        devices[deviceCount] = btDev;
        deviceCount++;

        Serial.printf("[BT] Found: %s %s\n",
          btDev.hasName ? btDev.name.c_str() : "(sem nome)",
          btDev.address.c_str());
      }
    } else {
      Serial.println("[BT] Scan returned null!");
    }
    scanDone = true;
    sortByRssi(devices, deviceCount);
  } else {
    ble_transport::startScan();
    deviceCount = ble_transport::getDeviceCount();
    for (int i = 0; i < deviceCount && i < BT_MAX_DEVICES; i++) {
      BtDevice d = ble_transport::getDevice(i);
      devices[i] = d;
    }
  }

  Serial.printf("[CONN] Scan complete, %d devices found\n", deviceCount);
}

bool vesc_bt::isScanComplete() {
  if (currentConnType == CONN_WIFI) return true;
  if (currentConnType == CONN_CLASSIC) return true;
  if (ble_transport::isScanComplete()) {
    deviceCount = ble_transport::getDeviceCount();
    for (int i = 0; i < deviceCount && i < BT_MAX_DEVICES; i++) {
      BtDevice d = ble_transport::getDevice(i);
      devices[i] = d;
    }
    sortByRssi(devices, deviceCount);
    return true;
  }
  return false;
}

int vesc_bt::getDeviceCount() { return deviceCount; }
const BtDevice& vesc_bt::getDevice(int index) { return devices[index]; }

int vesc_bt::findDeviceByName(const char* name) {
  for (int i = 0; i < deviceCount; i++) {
    if (devices[i].hasName && devices[i].name == name) return i;
  }
  return -1;
}

bool vesc_bt::connectByName(const char* name) {
  if (currentConnType == CONN_BLE) return false;
  if (currentConnType == CONN_WIFI) return false;

  Serial.printf("[BT] Connecting to \"%s\"...\n", name);
  if (SerialBT.connect(name)) {
    vesc.setSerialPort(&SerialBT);
    lastConnectedAddress = "";
    Serial.printf("[BT] Connected to %s\n", name);
    return true;
  }
  Serial.println("[BT] Connection failed");
  return false;
}

bool vesc_bt::connectByIndex(int index) {
  if (index < 0 || index >= deviceCount) return false;

  if (currentConnType == CONN_WIFI) {
    if (!wifi_transport::connect()) return false;
    vesc.setSerialPort(wifi_transport::getStream());
    lastConnectedAddress = VESC_WIFI_HOST;
    return true;
  }

  if (currentConnType == CONN_BLE) {
    if (!ble_transport::connectByAddress(devices[index].address.c_str())) return false;
    vesc.setSerialPort(ble_transport::getStream());
    lastConnectedAddress = devices[index].address;
    return true;
  }

  BtDevice &dev = devices[index];
  Serial.printf("[BT] Connecting to %s (%s)...\n",
    dev.hasName ? dev.name.c_str() : "(sem nome)",
    dev.address.c_str());

  if (dev.hasName) {
    if (SerialBT.connect(dev.name.c_str())) {
      vesc.setSerialPort(&SerialBT);
      lastConnectedAddress = dev.address;
      Serial.printf("[BT] Connected to %s\n", dev.name.c_str());
      return true;
    }
  }

  uint8_t mac[6];
  sscanf(dev.address.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
    &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
  if (SerialBT.connect(mac)) {
    vesc.setSerialPort(&SerialBT);
    lastConnectedAddress = dev.address;
    Serial.printf("[BT] Connected to %s\n", dev.address.c_str());
    return true;
  }

  Serial.println("[BT] Trying direct channel 1...");
  if (SerialBT.connect(mac, 1)) {
    vesc.setSerialPort(&SerialBT);
    lastConnectedAddress = dev.address;
    Serial.printf("[BT] Connected to %s (ch1)\n", dev.address.c_str());
    return true;
  }

  Serial.println("[BT] Connection failed");
  return false;
}

bool vesc_bt::isConnected() {
  if (currentConnType == CONN_WIFI) return wifi_transport::isConnected();
  if (currentConnType == CONN_BLE) return ble_transport::isConnected();
  return SerialBT.connected();
}

bool vesc_bt::read(VescData& data) {
  if (!isConnected()) {
    Serial.println("[VESC] read: not connected");
    return false;
  }

  Stream* stream;
  if (currentConnType == CONN_BLE) stream = ble_transport::getStream();
  else if (currentConnType == CONN_WIFI) stream = wifi_transport::getStream();
  else stream = (Stream*)&SerialBT;

  Serial.printf("[VESC] read: stream available=%d\n", stream->available());

  if (!vesc.getVescValues()) {
    Serial.println("[VESC] getVescValues failed");
    return false;
  }

  data.voltage     = vesc.data.inpVoltage;
  data.current     = vesc.data.avgInputCurrent;
  data.duty        = vesc.data.dutyCycleNow * 100.0f;
  data.tempEsc     = vesc.data.tempMosfet;
  data.tempMotor   = vesc.data.tempMotor;
  data.ampHours    = vesc.data.ampHours;
  data.tachometerAbs = vesc.data.tachometerAbs;
  data.error       = vesc.data.error;
  data.btState = BT_CONNECTED;
  data.connType = currentConnType;

  float erpm = vesc.data.rpm;
  float rpm = erpm / (float)MOTOR_POLE_PAIRS;
  float wheelCircM = (WHEEL_DIAMETER_MM / 1000.0f) * PI;
  data.speed = (rpm * wheelCircM * 60.0f) / 1000.0f;

  float vClamped = constrain(data.voltage, 33.0f, 42.0f);
  data.batteryPct = (int)((vClamped - 33.0f) / 9.0f * 100.0f);

  Serial.printf("[VESC] RPM:%.0f | V:%.1fV | A:%.1fA | Duty:%.0f%% | Speed:%.1fkm/h\n",
    vesc.data.rpm, data.voltage, data.current, data.duty, data.speed);
  Serial.printf("[VESC] TempMosfet:%.0fC | TempMotor:%.0fC | Ah:%.2f | TachoAbs:%ld | Fault:%d\n",
    data.tempEsc, data.tempMotor, data.ampHours, data.tachometerAbs, data.error);

  return true;
}

void vesc_bt::disconnect() {
  if (currentConnType == CONN_WIFI) {
    wifi_transport::disconnect();
  } else if (currentConnType == CONN_BLE) {
    ble_transport::disconnect();
  } else {
    SerialBT.disconnect();
  }
  Serial.println("[CONN] Disconnected");
}

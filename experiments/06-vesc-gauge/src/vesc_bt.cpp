#include "vesc_bt.h"
#include "config.h"
#include "BluetoothSerial.h"
#include <VescUart.h>
#include "transport_ble.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled!
#endif

static BluetoothSerial SerialBT;
static VescUart vesc;

static BtType currentBtType = BT_TYPE_CLASSIC;
static BtDevice devices[BT_MAX_DEVICES];
static int deviceCount = 0;
static bool scanDone = false;

String vesc_bt::lastConnectedAddress = "";

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
  Serial.println("[BT] Initialized (Classic + BLE)");
}

void vesc_bt::setBtType(BtType type) {
  if (currentBtType == type) return;
  currentBtType = type;
  Serial.printf("[BT] Mode changed to %s\n", type == BT_TYPE_CLASSIC ? "Classic" : "BLE");
}

BtType vesc_bt::getBtType() { return currentBtType; }

void vesc_bt::startScan() {
  deviceCount = 0;
  scanDone = false;

  if (currentBtType == BT_TYPE_CLASSIC) {
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
        btDev.type = BT_TYPE_CLASSIC;
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

  Serial.printf("[BT] Scan complete, %d devices found\n", deviceCount);
}

bool vesc_bt::isScanComplete() {
  if (currentBtType == BT_TYPE_CLASSIC) return true;
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
  if (currentBtType == BT_TYPE_BLE) return false;

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

  if (currentBtType == BT_TYPE_BLE) {
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
  if (currentBtType == BT_TYPE_BLE) return ble_transport::isConnected();
  return SerialBT.connected();
}

bool vesc_bt::read(VescData& data) {
  if (!isConnected()) return false;
  if (!vesc.getVescValues()) return false;

  data.voltage     = vesc.data.inpVoltage;
  data.current     = vesc.data.avgInputCurrent;
  data.duty        = vesc.data.dutyCycleNow * 100.0f;
  data.tempEsc     = vesc.data.tempMosfet;
  data.tempMotor   = vesc.data.tempMotor;
  data.ampHours    = vesc.data.ampHours;
  data.tachometerAbs = vesc.data.tachometerAbs;
  data.error       = vesc.data.error;
  data.btState = BT_CONNECTED;

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
  if (currentBtType == BT_TYPE_BLE) {
    ble_transport::disconnect();
  } else {
    SerialBT.disconnect();
  }
  Serial.println("[BT] Disconnected");
}

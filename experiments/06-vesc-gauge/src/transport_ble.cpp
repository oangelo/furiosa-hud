#include "transport_ble.h"

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>

static BLEScan* bleScan = nullptr;
static BLEClient* bleClient = nullptr;
static BLERemoteCharacteristic* txChar = nullptr;
static BLERemoteCharacteristic* rxChar = nullptr;

static BtDevice devices[BT_MAX_DEVICES];
static int deviceCount = 0;
static bool scanDone = false;

static BLEUUID VESC_SERVICE_UUID("6e400001-b5a3-f393-e0a9-e50e24dcca9e");
static BLEUUID VESC_RX_UUID("6e400002-b5a3-f393-e0a9-e50e24dcca9e");
static BLEUUID VESC_TX_UUID("6e400003-b5a3-f393-e0a9-e50e24dcca9e");

static constexpr int BLE_BUF_SIZE = 512;
static volatile uint8_t bleBuffer[BLE_BUF_SIZE];
static volatile int bleBufHead = 0;
static volatile int bleBufTail = 0;

static String lastConnectedAddr = "";

class VescAdvertisedDevice : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    if (deviceCount >= BT_MAX_DEVICES) return;

    String addr = advertisedDevice.getAddress().toString().c_str();
    for (int i = 0; i < deviceCount; i++) {
      if (devices[i].address == addr) return;
    }

    BtDevice dev;
    dev.address = addr;
    dev.hasName = advertisedDevice.haveName();
    dev.name = dev.hasName ? advertisedDevice.getName().c_str() : "";
    dev.type = BT_TYPE_BLE;
    devices[deviceCount] = dev;
    deviceCount++;

    Serial.printf("[BLE] Found: %s %s\n",
      dev.hasName ? dev.name.c_str() : "(sem nome)",
      dev.address.c_str());
  }
};

static void notifyCallback(BLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
  for (size_t i = 0; i < length; i++) {
    int next = (bleBufHead + 1) % BLE_BUF_SIZE;
    if (next != bleBufTail) {
      bleBuffer[bleBufHead] = pData[i];
      bleBufHead = next;
    }
  }
}

class BLEStream : public Stream {
public:
  int available() override {
    if (bleBufHead >= bleBufTail)
      return bleBufHead - bleBufTail;
    return BLE_BUF_SIZE - bleBufTail + bleBufHead;
  }

  int read() override {
    if (bleBufHead == bleBufTail) return -1;
    uint8_t c = bleBuffer[bleBufTail];
    bleBufTail = (bleBufTail + 1) % BLE_BUF_SIZE;
    return c;
  }

  int peek() override {
    if (bleBufHead == bleBufTail) return -1;
    return bleBuffer[bleBufTail];
  }

  size_t write(uint8_t byte) override {
    if (!rxChar) return 0;
    rxChar->writeValue(&byte, 1);
    return 1;
  }

  size_t write(const uint8_t* buf, size_t size) override {
    if (!rxChar) return 0;
    size_t written = 0;
    while (written < size) {
      size_t chunk = min((size_t)20, size - written);
      rxChar->writeValue((uint8_t*)(buf + written), chunk);
      written += chunk;
    }
    return written;
  }
};

static BLEStream bleStream;

void ble_transport::init() {
  BLEDevice::init(BT_LOCAL_NAME);
  bleClient = BLEDevice::createClient();
  Serial.println("[BLE] Initialized");
}

void ble_transport::startScan() {
  deviceCount = 0;
  scanDone = false;
  bleBufHead = 0;
  bleBufTail = 0;

  if (!bleScan) {
    bleScan = BLEDevice::getScan();
    bleScan->setAdvertisedDeviceCallbacks(new VescAdvertisedDevice());
    bleScan->setActiveScan(true);
    bleScan->setInterval(100);
    bleScan->setWindow(99);
  }

  Serial.println("[BLE] Starting async scan...");
  bleScan->start(BT_SCAN_TIMEOUT_MS / 1000, false);
  scanDone = true;
  Serial.printf("[BLE] Scan complete, %d devices found\n", deviceCount);
}

bool ble_transport::isScanComplete() { return scanDone; }
int ble_transport::getDeviceCount() { return deviceCount; }
const BtDevice& ble_transport::getDevice(int index) { return devices[index]; }

int ble_transport::findDeviceByName(const char* name) {
  for (int i = 0; i < deviceCount; i++) {
    if (devices[i].hasName && devices[i].name == name) return i;
  }
  return -1;
}

bool ble_transport::connectByIndex(int index) {
  if (index < 0 || index >= deviceCount) return false;

  BtDevice& dev = devices[index];
  Serial.printf("[BLE] Connecting to %s (%s)...\n",
    dev.hasName ? dev.name.c_str() : "(sem nome)",
    dev.address.c_str());

  BLEAddress addr(dev.address.c_str());
  if (!bleClient->connect(addr)) {
    Serial.println("[BLE] Connection failed");
    return false;
  }

  Serial.println("[BLE] Connected, discovering service...");
  BLERemoteService* service = bleClient->getService(VESC_SERVICE_UUID);
  if (!service) {
    Serial.println("[BLE] VESC service not found");
    bleClient->disconnect();
    return false;
  }

  txChar = service->getCharacteristic(VESC_TX_UUID);
  rxChar = service->getCharacteristic(VESC_RX_UUID);

  if (!txChar || !rxChar) {
    Serial.println("[BLE] Characteristics not found");
    bleClient->disconnect();
    txChar = nullptr;
    rxChar = nullptr;
    return false;
  }

  if (txChar->canNotify()) {
    txChar->registerForNotify(notifyCallback);
    Serial.println("[BLE] Subscribed to TX notifications");
  }

  bleBufHead = 0;
  bleBufTail = 0;
  lastConnectedAddr = dev.address;

  Serial.printf("[BLE] Connected to %s\n", dev.address.c_str());
  return true;
}

bool ble_transport::isConnected() {
  return bleClient && bleClient->isConnected();
}

Stream* ble_transport::getStream() {
  return &bleStream;
}

void ble_transport::disconnect() {
  if (bleClient && bleClient->isConnected()) {
    bleClient->disconnect();
  }
  txChar = nullptr;
  rxChar = nullptr;
  Serial.println("[BLE] Disconnected");
}

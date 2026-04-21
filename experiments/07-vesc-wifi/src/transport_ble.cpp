#include "transport_ble.h"
#include "config.h"

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
static volatile bool bleConnected = false;
static volatile bool bleSecured = false;

class BleSecurityCallbacks : public BLESecurityCallbacks {
  uint32_t onPassKeyRequest() {
    Serial.printf("[BLE-SEC] Passkey request, sending %s\n", VESC_BLE_PIN);
    return atol(VESC_BLE_PIN);
  }

  void onPassKeyNotify(uint32_t pass_key) {
    Serial.printf("[BLE-SEC] Passkey notify: %06lu\n", pass_key);
  }

  bool onConfirmPIN(uint32_t pin) {
    Serial.printf("[BLE-SEC] Confirm PIN: %06lu -> YES\n", pin);
    return true;
  }

  bool onSecurityRequest() {
    Serial.println("[BLE-SEC] Security request -> accepting");
    return true;
  }

  void onAuthenticationComplete(esp_ble_auth_cmpl_t auth_cmpl) {
    if (auth_cmpl.success) {
      bleSecured = true;
      Serial.printf("[BLE-SEC] Auth OK, addr=%s\n",
        auth_cmpl.bd_addr);
    } else {
      bleSecured = false;
      Serial.printf("[BLE-SEC] Auth FAILED, reason=0x%x\n", auth_cmpl.fail_reason);
    }
  }
};

class BleClientCallbacks : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    bleConnected = true;
    bleSecured = false;
    Serial.println("[BLE] onConnect");
  }
  void onDisconnect(BLEClient* pclient) {
    bleConnected = false;
    bleSecured = false;
    Serial.println("[BLE] onDisconnect");
  }
};

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
    dev.type = CONN_BLE;
    dev.rssi = advertisedDevice.getRSSI();
    devices[deviceCount] = dev;
    deviceCount++;

    Serial.printf("[BLE] Found: %s %s\n",
      dev.hasName ? dev.name.c_str() : "(sem nome)",
      dev.address.c_str());
  }
};

static void notifyCallback(BLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
  Serial.printf("[BLE-NOTIFY] %d bytes\n", length);
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
    rxChar->writeValue(&byte, 1, false);
    return 1;
  }

  size_t write(const uint8_t* buf, size_t size) override {
    if (!rxChar) return 0;
    size_t written = 0;
    while (written < size) {
      size_t chunk = min((size_t)20, size - written);
      rxChar->writeValue((uint8_t*)(buf + written), chunk, false);
      written += chunk;
    }
    return written;
  }
};

static BLEStream bleStream;

static void setupBLESecurity() {
  BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT_MITM);
  BLEDevice::setSecurityCallbacks(new BleSecurityCallbacks());

  esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_MITM_BOND;
  esp_ble_io_cap_t iocap = ESP_IO_CAP_KBDISP;
  uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
  uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
  uint8_t key_size = 16;

  esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));

  uint32_t passkey = atol(VESC_BLE_PIN);
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &passkey, sizeof(uint32_t));

  Serial.printf("[BLE] Security configured, PIN=%s\n", VESC_BLE_PIN);
}

void ble_transport::init() {
  BLEDevice::init(BT_LOCAL_NAME);
  setupBLESecurity();
  bleClient = BLEDevice::createClient();
  bleClient->setClientCallbacks(new BleClientCallbacks());
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

  Serial.println("[BLE] Starting scan...");
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
  return connectByAddress(devices[index].address.c_str());
}

static bool waitForSecurity(int timeoutMs) {
  unsigned long start = millis();
  while (!bleSecured && (millis() - start < (unsigned long)timeoutMs)) {
    delay(10);
  }
  if (!bleSecured) {
    Serial.println("[BLE] Security timeout!");
    return false;
  }
  Serial.println("[BLE] Security established");
  return true;
}

bool ble_transport::connectByAddress(const char* address) {
  if (bleScan) bleScan->stop();

  txChar = nullptr;
  rxChar = nullptr;
  bleSecured = false;

  Serial.printf("[BLE] Connecting to %s...\n", address);

  BLEAddress addr(address);
  if (!bleClient->connect(addr)) {
    Serial.println("[BLE] Connection failed");
    return false;
  }

  Serial.println("[BLE] Connected, waiting for security...");
  if (!waitForSecurity(5000)) {
    Serial.println("[BLE] Continuing without security...");
  }

  Serial.println("[BLE] Negotiating MTU...");
  bleClient->setMTU(517);

  Serial.println("[BLE] Discovering service...");
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

  Serial.printf("[BLE] TX canNotify=%d canIndicate=%d\n",
    txChar->canNotify(), txChar->canIndicate());
  Serial.printf("[BLE] RX canWrite=%d canWriteNoResponse=%d\n",
    rxChar->canWrite(), rxChar->canWriteNoResponse());

  txChar->registerForNotify(notifyCallback);
  Serial.println("[BLE] registerForNotify called");

  bleBufHead = 0;
  bleBufTail = 0;
  lastConnectedAddr = address;

  Serial.printf("[BLE] Ready! secured=%d mtu=%d\n", bleSecured, bleClient->getMTU());
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

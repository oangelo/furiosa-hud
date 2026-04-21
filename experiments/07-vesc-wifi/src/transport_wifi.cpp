#include "transport_wifi.h"
#include <WiFi.h>

static WiFiClient tcpClient;
static bool wifiInitialized = false;

void wifi_transport::init() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);
  wifiInitialized = true;
  Serial.println("[WIFI] Initialized (STA mode)");
}

bool wifi_transport::connect() {
  if (!wifiInitialized) init();

  Serial.printf("[WIFI] Connecting to SSID \"%s\"...\n", VESC_WIFI_SSID);

  WiFi.begin(VESC_WIFI_SSID, VESC_WIFI_PASS);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > WIFI_CONNECT_MS) {
      Serial.println("[WIFI] WiFi connection timeout");
      WiFi.disconnect();
      return false;
    }
    delay(100);
    Serial.printf(".");
  }
  Serial.printf("\n[WIFI] WiFi connected, IP=%s\n", WiFi.localIP().toString().c_str());

  Serial.printf("[WIFI] Connecting TCP to %s:%d...\n", VESC_WIFI_HOST, VESC_WIFI_PORT);

  if (!tcpClient.connect(VESC_WIFI_HOST, VESC_WIFI_PORT, 5000)) {
    Serial.println("[WIFI] TCP connection failed");
    WiFi.disconnect();
    return false;
  }

  tcpClient.setNoDelay(true);
  Serial.println("[WIFI] TCP connected");
  return true;
}

bool wifi_transport::isConnected() {
  return WiFi.status() == WL_CONNECTED && tcpClient.connected();
}

Stream* wifi_transport::getStream() {
  return &tcpClient;
}

void wifi_transport::disconnect() {
  tcpClient.stop();
  WiFi.disconnect();
  Serial.println("[WIFI] Disconnected");
}

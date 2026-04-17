/*
 * Experimento 01 — Conectar ao VESC via Bluetooth Classic SPP
 *
 * Objetivo: parear e abrir porta serial BT com o módulo VESC BT,
 *           verificar handshake básico lendo COMM_GET_VALUES.
 */
#include <Arduino.h>
#include "BluetoothSerial.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to enable it.
#endif

BluetoothSerial SerialBT;

const char *VESC_BT_NAME = "VESC_BT"; // TODO: ajustar nome do módulo

void setup() {
  Serial.begin(115200);
  SerialBT.begin("FuriosaHUD", true); // true = master

  Serial.printf("Tentando conectar a \"%s\"...\n", VESC_BT_NAME);
  if (SerialBT.connect(VESC_BT_NAME)) {
    Serial.println("Conectado ao VESC BT!");
  } else {
    Serial.println("Falha na conexão BT.");
  }
}

void loop() {
  if (SerialBT.available()) {
    Serial.write(SerialBT.read());
  }
  if (Serial.available()) {
    SerialBT.write(Serial.read());
  }
}

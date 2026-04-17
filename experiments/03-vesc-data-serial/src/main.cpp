/*
 * Experimento 03 — Ler dados do VESC via UART (Serial2)
 *
 * Objetivo: usar VescUart para solicitar COMM_GET_VALUES e
 *           imprimir rpm, duty, voltage, current no Serial Monitor.
 *           Conexão: VESC TX → GPIO16 (RX2), VESC RX → GPIO17 (TX2)
 */
#include <Arduino.h>
#include <VescUart.h>

VescUart vesc;

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, 16, 17);
  vesc.setSerialPort(&Serial2);
  Serial.println("VescUart ready on Serial2.");
}

void loop() {
  if (vesc.getVescValues()) {
    Serial.printf("RPM: %.0f | Duty: %.1f%% | V: %.2f | A: %.2f\n",
                  vesc.data.rpm,
                  vesc.data.dutyCycle * 100.0,
                  vesc.data.inpVoltage,
                  vesc.data.avgInputCurrent);
  } else {
    Serial.println("Failed to read VESC data.");
  }
  delay(500);
}

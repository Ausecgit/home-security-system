#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#include "config.h"

SPIClass spi(FSPI);
Module radioMod(LORA_SS, LORA_DIO1, LORA_RST, LORA_BUSY, spi);
SX1262 radio = SX1262(&radioMod);

uint32_t rxCount = 0;
uint32_t txCount = 0;
uint32_t errCount = 0;
unsigned long lastReport = 0;
unsigned long lastRx = 0;
bool txInProg = false;
bool rxFlag = false;

#define SerialOut Serial

void onRxFlag() {
  rxFlag = true;
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("LoRa repeater boot");

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  spi.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);

  int state = radio.begin(LORA_FREQ, LORA_BW, LORA_SF, LORA_CR,
                          RADIOLIB_SX126X_SYNC_WORD_PRIVATE, LORA_TX_POWER,
                          LORA_PREAMBLE, 0, false);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("radio.begin failed: %d\n", state);
    for (int i = 0; i < 10; i++) {
      digitalWrite(LED_PIN, HIGH); delay(200);
      digitalWrite(LED_PIN, LOW); delay(200);
    }
    return;
  }
  radio.setCRC(2);

  radio.setDio1Action(onRxFlag);

  state = radio.startReceive();
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("startReceive failed: %d\n", state);
    return;
  }

  Serial.printf("LoRa ready: %.1fMHz SF%d BW%.0f\n", LORA_FREQ, LORA_SF, LORA_BW);
  digitalWrite(LED_PIN, HIGH);
}

void loop() {
  if (rxFlag) {
    rxFlag = false;

    uint8_t buf[256];
    size_t len = 0;
    int state = radio.readData(buf, 0);
    len = radio.getPacketLength();

    if (state == RADIOLIB_ERR_NONE) {
      rxCount++;
      lastRx = millis();

      float rssi = radio.getRSSI();
      float snr = radio.getSNR();
      Serial.printf("[RX] len=%d rssi=%.1f snr=%.1f\n", len, rssi, snr);

      digitalWrite(LED_PIN, LOW);

      state = radio.transmit(buf, len);
      if (state == RADIOLIB_ERR_NONE) {
        txCount++;
        Serial.printf("[TX] relayed %d bytes\n", len);
      } else {
        errCount++;
        Serial.printf("[TX] failed: %d\n", state);
      }

      digitalWrite(LED_PIN, HIGH);

      state = radio.startReceive();
      if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[RX] restart failed: %d\n", state);
      }
    } else if (state == RADIOLIB_ERR_LORA_HEADER_DAMAGED || state == RADIOLIB_ERR_SPI_CMD_TIMEOUT) {
      errCount++;
      Serial.printf("[RX] CRC error\n");
      radio.startReceive();
    } else {
      errCount++;
      Serial.printf("[RX] error: %d\n", state);
      radio.startReceive();
    }
  }

  if (millis() - lastReport > 30000) {
    lastReport = millis();
    Serial.printf("[STAT] rx=%u tx=%u err=%u\n", rxCount, txCount, errCount);
  }
}

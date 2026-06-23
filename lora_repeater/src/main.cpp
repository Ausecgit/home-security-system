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
unsigned long lastBattCheck = 0;
bool rxFlag = false;
bool lowPower = false;
int battMv = 0;
int battPct = 0;

void onRxFlag() {
  rxFlag = true;
}

int readBatteryMv() {
  analogReadResolution(12);
  int raw = analogRead(BAT_ADC_PIN);
  float v = (float)raw / 4095.0f * 3.3f * BAT_DIVIDER * 1000.0f;
  return (int)v;
}

int battToPct(int mv) {
  if (mv >= BAT_FULL_MV) return 100;
  if (mv <= BAT_EMPTY_MV) return 0;
  return (int)((float)(mv - BAT_EMPTY_MV) / (float)(BAT_FULL_MV - BAT_EMPTY_MV) * 100.0f);
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

  battMv = readBatteryMv();
  battPct = battToPct(battMv);
  Serial.printf("LoRa ready: %.1fMHz SF%d BW%.0f | batt=%dmV %d%%\n",
                LORA_FREQ, LORA_SF, LORA_BW, battMv, battPct);
  Serial.printf("BAT_MONITOR=%d (set to 1 in config.h when battery is wired)\n", BAT_MONITOR);
  digitalWrite(LED_PIN, HIGH);
}

void loop() {
  if (rxFlag) {
    rxFlag = false;

    uint8_t buf[256];
    int state = radio.readData(buf, 0);
    size_t len = radio.getPacketLength();

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
      Serial.printf("[RX] header/CRC error\n");
      radio.startReceive();
    } else {
      errCount++;
      Serial.printf("[RX] error: %d\n", state);
      radio.startReceive();
    }
  }

  if (millis() - lastReport > 30000) {
    lastReport = millis();
    Serial.printf("[STAT] rx=%u tx=%u err=%u bat=%dmV %d%% %s\n",
                  rxCount, txCount, errCount, battMv, battPct,
                  lowPower ? "LOW" : "OK");
  }

  if (millis() - lastBattCheck > 60000 && BAT_MONITOR) {
    lastBattCheck = millis();
    battMv = readBatteryMv();
    battPct = battToPct(battMv);
    Serial.printf("[BAT] %dmV %d%%\n", battMv, battPct);

    if (battMv < BAT_CRIT_MV && !lowPower) {
      lowPower = true;
      Serial.println("[BAT] CRITICAL - entering deep sleep");
      radio.standby();
      digitalWrite(LED_PIN, LOW);
      esp_sleep_enable_timer_wakeup(60ULL * 1000000ULL);
      esp_deep_sleep_start();
    } else if (battMv < BAT_LOW_MV && !lowPower) {
      lowPower = true;
      Serial.println("[BAT] LOW - reducing TX power");
      radio.setOutputPower(10);
    } else if (battMv > BAT_LOW_MV + 100 && lowPower) {
      lowPower = false;
      Serial.println("[BAT] recovered - full power");
      radio.setOutputPower(LORA_TX_POWER);
      radio.startReceive();
    }
  }
}

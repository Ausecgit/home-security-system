#ifndef CONFIG_H
#define CONFIG_H

#define LORA_FREQ 915.0
#define LORA_SF 7
#define LORA_BW 125.0
#define LORA_CR 5
#define LORA_PREAMBLE 8
#define LORA_TX_POWER 22

#define REPEATER_ID "rep01"

#define LORA_SCK   9
#define LORA_MISO  11
#define LORA_MOSI  10
#define LORA_SS    8
#define LORA_RST   12
#define LORA_DIO1  14
#define LORA_BUSY  13

#define LED_PIN 35

#define BAT_ADC_PIN    1
#define BAT_DIVIDER    2.0f
#define BAT_FULL_MV    4200
#define BAT_EMPTY_MV   3000
#define BAT_LOW_MV     3300
#define BAT_CRIT_MV    3100
#define BAT_MONITOR    0

#endif

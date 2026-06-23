#ifndef CONFIG_H
#define CONFIG_H

#define CAM_ID "cam01"

#define WIFI_SSID "DonsTest"
#define WIFI_PASS "dontest123"

#define MQTT_HOST "192.168.4.2"
#define MQTT_PORT 1883
#define MQTT_TOPIC "home/cam/" CAM_ID "/motion"

#define STILL_ENDPOINT "http://192.168.4.2:8080/still"

#define DET_FRAMESIZE FRAMESIZE_QVGA
#define STILL_FRAMESIZE FRAMESIZE_VGA

#define BG_WINDOW_N 20
#define BG_INTERVAL_MS 30000
#define MOTION_COOLDOWN_MS 10000

#define Z_THRESHOLD 5.0f
#define MIN_BLOB_PIXELS 200
#define SMALL_BLOB_PX 1000
#define LARGE_BLOB_PX 5000

#define DENOISE_BLUR 1
#define DENOISE_ERODE 1
#define TEMPORAL_PERSISTENCE 2

#define AE_LOCK_AFTER_FRAMES 5

#define DET_LOOP_DELAY_MS 150

#define CAM_PIN_PWDN   -1
#define CAM_PIN_RESET  -1
#define CAM_PIN_XCLK   15
#define CAM_PIN_SIOD    4
#define CAM_PIN_SIOC    5
#define CAM_PIN_Y9     16
#define CAM_PIN_Y8     17
#define CAM_PIN_Y7     18
#define CAM_PIN_Y6     12
#define CAM_PIN_Y5     10
#define CAM_PIN_Y4      8
#define CAM_PIN_Y3      9
#define CAM_PIN_Y2     11
#define CAM_PIN_VSYNC    6
#define CAM_PIN_HREF     7
#define CAM_PIN_PCLK    13

#endif

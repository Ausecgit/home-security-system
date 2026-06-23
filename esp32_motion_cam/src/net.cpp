#include "net.h"
#include "config.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <HTTPClient.h>

static WiFiClient wifiClient;
static PubSubClient mqtt(wifiClient);
static unsigned long lastMqttReconnect = 0;
static bool wifiOK = false;

static void wifi_connect() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) {
    delay(200);
  }
  wifiOK = (WiFi.status() == WL_CONNECTED);
}

static bool mqtt_connect() {
  if (!wifiOK) return false;
  if (mqtt.connected()) return true;
  String id = String("cam-") + CAM_ID + "-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  if (millis() - lastMqttReconnect < 5000) return false;
  lastMqttReconnect = millis();
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setBufferSize(512);
  return mqtt.connect(id.c_str());
}

bool net_init() {
  wifi_connect();
  return wifiOK;
}

void net_loop() {
  if (WiFi.status() == WL_CONNECTED) {
    wifiOK = true;
  } else if (!wifiOK || WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
    delay(100);
    if (WiFi.status() == WL_CONNECTED) wifiOK = true;
  }
  if (wifiOK) {
    if (!mqtt.connected()) mqtt_connect();
    if (mqtt.connected()) mqtt.loop();
  }
}

bool net_connected() {
  return wifiOK && mqtt.connected();
}

bool net_publish_motion(const char* cam_id, unsigned long ts_ms,
                        int cx, int cy, int area,
                        const char* cls, uint32_t frame_id) {
  if (!mqtt.connected()) return false;
  char payload[256];
  snprintf(payload, sizeof(payload),
           "{\"cam\":\"%s\",\"t\":%lu,\"cx\":%d,\"cy\":%d,\"area\":%d,\"class\":\"%s\",\"frame\":%u}",
           cam_id, ts_ms, cx, cy, area, cls, frame_id);
  return mqtt.publish(MQTT_TOPIC, payload);
}

bool net_publish_status(const char* cam_id, const char* stage,
                        uint32_t frame_id) {
  if (!mqtt.connected()) return false;
  char topic[64];
  char payload[256];
  snprintf(topic, sizeof(topic), "home/cam/%s/status", cam_id);
  snprintf(payload, sizeof(payload),
           "{\"cam\":\"%s\",\"stage\":\"%s\",\"wifi\":%s,\"mqtt\":%s,\"frames\":%u,\"heap\":%u}",
           cam_id, stage,
           WiFi.status() == WL_CONNECTED ? "true" : "false",
           mqtt.connected() ? "true" : "false",
           frame_id, (unsigned)ESP.getFreeHeap());
  return mqtt.publish(topic, payload);
}

bool net_upload_still(uint8_t* jpg, size_t jpg_len, uint32_t frame_id) {
  if (!wifiOK) return false;
  HTTPClient http;
  http.setTimeout(5000);
  if (!http.begin(STILL_ENDPOINT)) return false;
  http.addHeader("Content-Type", "image/jpeg");
  http.addHeader("X-Frame-Id", String(frame_id));
  http.addHeader("X-Cam-Id", CAM_ID);
  int code = http.POST(jpg, jpg_len);
  http.end();
  return (code > 0);
}

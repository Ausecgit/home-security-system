#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <esp_wifi.h>
#include <HWCDC.h>
#include <dhcpserver/dhcpserver.h>
#include <lwip/ip4_addr.h>

#define AP_SSID "DonsTest"
#define AP_PASS "dontest123"
#define AP_CHANNEL 1
#define AP_MAX_CLIENTS 8

WebServer server(80);
unsigned long bootMs = 0;

void printLeases() {
  wifi_sta_list_t list;
  esp_wifi_ap_get_sta_list(&list);
  Serial.printf("[leases] clients=%d\n", list.num);
  for (int i = 0; i < list.num; i++) {
    uint8_t* m = list.sta[i].mac;
    ip4_addr_t ip;
    if (dhcp_search_ip_on_mac(m, &ip)) {
      Serial.printf("  [%d] MAC=%02X:%02X:%02X:%02X:%02X:%02X IP=%u.%u.%u.%u\n",
        i, m[0], m[1], m[2], m[3], m[4], m[5],
        ip4_addr1(&ip), ip4_addr2(&ip), ip4_addr3(&ip), ip4_addr4(&ip));
    } else {
      Serial.printf("  [%d] MAC=%02X:%02X:%02X:%02X:%02X:%02X IP=unknown\n",
        i, m[0], m[1], m[2], m[3], m[4], m[5]);
    }
  }
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta http-equiv='refresh' content='5'>";
  html += "<title>DonsTest AP</title></head><body>";
  html += "<h2>DonsTest Mock Network</h2>";
  html += "<p>Uptime: " + String((millis() - bootMs) / 1000) + " s</p>";
  html += "<p>AP IP: " + WiFi.softAPIP().toString() + "</p>";
  html += "<h3>Connected stations</h3>";
  wifi_sta_list_t list;
  esp_wifi_ap_get_sta_list(&list);
  if (list.num == 0) {
    html += "<p>None</p>";
  } else {
    html += "<table border='1' cellpadding='4'><tr><th>#</th><th>MAC</th><th>IP</th></tr>";
    for (int i = 0; i < list.num; i++) {
      char mac[18];
      snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
               list.sta[i].mac[0], list.sta[i].mac[1], list.sta[i].mac[2],
               list.sta[i].mac[3], list.sta[i].mac[4], list.sta[i].mac[5]);
      ip4_addr_t ip;
      String ipStr;
      if (dhcp_search_ip_on_mac(list.sta[i].mac, &ip)) {
        ipStr = String(ip4_addr1(&ip)) + "." + String(ip4_addr2(&ip)) + "." +
                String(ip4_addr3(&ip)) + "." + String(ip4_addr4(&ip));
      } else {
        ipStr = "unknown";
      }
      html += "<tr><td>" + String(i + 1) + "</td><td>" + String(mac) + "</td><td>" + ipStr + "</td></tr>";
    }
    html += "</table>";
  }
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);
  bootMs = millis();
  delay(200);
  Serial.println("DonsTest AP booting");

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS, AP_CHANNEL, 0, AP_MAX_CLIENTS);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP().toString());

  server.on("/", handleRoot);
  server.begin();
  Serial.println("HTTP server on :80");
}

void loop() {
  server.handleClient();
  static unsigned long lastReport = 0;
  if (millis() - lastReport > 10000) {
    lastReport = millis();
    printLeases();
  }
}

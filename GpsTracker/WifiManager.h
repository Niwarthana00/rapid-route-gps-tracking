#pragma once

#include <WiFi.h>
#include "ConfigManager.h"

static constexpr uint32_t WIFI_CONNECT_TIMEOUT = 15000UL;
static constexpr uint32_t WIFI_CHECK_INTERVAL  =  5000UL;

struct WifiState {
  unsigned long lastCheckMs    = 0;
  bool          everConnected  = false;
};

class WifiManager {
public:
  static bool begin(WifiState& s, const WifiCfg& cfg) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(cfg.ssid, cfg.password);
    Serial.printf("[WiFi] Connecting to %s", cfg.ssid);

    unsigned long t = millis();
    while (WiFi.status() != WL_CONNECTED) {
      if (millis() - t > WIFI_CONNECT_TIMEOUT) {
        Serial.println(F("\n[WiFi] TIMEOUT – continuing without WiFi"));
        return false;
      }
      delay(250);
      Serial.print('.');
    }
    Serial.printf("\n[WiFi] Connected! IP: %s\n",
                  WiFi.localIP().toString().c_str());
    s.everConnected = true;
    return true;
  }

  static void tick(WifiState& s, const WifiCfg& cfg) {
    if (millis() - s.lastCheckMs < WIFI_CHECK_INTERVAL) return;
    s.lastCheckMs = millis();

    if (WiFi.status() != WL_CONNECTED) {
      Serial.println(F("[WiFi] Disconnected – reconnecting..."));
      WiFi.disconnect();
      WiFi.begin(cfg.ssid, cfg.password);
    }
  }

  static bool connected() { return WiFi.status() == WL_CONNECTED; }
};

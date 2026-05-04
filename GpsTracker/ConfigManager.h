#pragma once

#include <ArduinoJson.h>
#include "LittleFS.h"
#include <WiFiClientSecure.h>

namespace Defaults {
  static constexpr uint32_t GPS_BAUD         = 38400;
  static constexpr uint8_t  GPS_RXD          = 16;
  static constexpr uint8_t  GPS_TXD          = 17;
  static constexpr uint32_t GPS_TIMEOUT_MS   = 5000;
  static constexpr uint32_t GPS_MIN_CHARS    = 10;
  static constexpr uint32_t DISPLAY_INTERVAL = 2000;
  static constexpr int      TZ_OFFSET_H      = 5;
  static constexpr int      TZ_OFFSET_M      = 30;
  static constexpr uint16_t MQTT_PORT        = 8883;

  static constexpr uint8_t  SEAT_PIN            = 32;
  static constexpr int      SEAT_THRESHOLD      = 500;
  static constexpr uint32_t SEAT_POLL_MS        = 500;
}

struct WifiCfg {
  char ssid[64];
  char password[64];
};

struct MqttCfg {
  char     broker[128];
  uint16_t port;
  char     username[64];
  char     password[64];
  char     topic[128];
  char     client_prefix[32];
};

struct GpsCfg {
  uint32_t baud;
  uint8_t  rxd;
  uint8_t  txd;
  uint32_t timeout_ms;
  uint32_t min_chars;
  uint32_t display_interval_ms;
};

struct TimezoneCfg {
  int offset_hours;
  int offset_minutes;
};

struct SeatCfg {
  uint8_t  pin;
  int      threshold;
  uint32_t poll_interval_ms;
};

struct AppConfig {
  WifiCfg     wifi;
  MqttCfg     mqtt;
  GpsCfg      gps;
  TimezoneCfg tz;
  SeatCfg     seat;
  bool        valid;
};

class ConfigManager {
public:
  static bool begin(AppConfig& cfg) {
    cfg.valid = false;

    if (!LittleFS.begin(true, "/littlefs", 10, "spiffs")) {
      Serial.println(F("[CFG] LittleFS mount failed!"));
      return false;
    }

    if (!loadJson(cfg)) return false;

    cfg.valid = true;
    Serial.println(F("[CFG] Config loaded OK."));
    return true;
  }

  static bool loadTlsCert(WiFiClientSecure& espClient) {
    File f = LittleFS.open("/root.crt", "r");
    if (!f) {
      Serial.println(F("[CFG] root.crt not found!"));
      return false;
    }
    bool ok = espClient.loadCACert(f, f.size());
    f.close();
    if (ok) Serial.println(F("[CFG] TLS cert loaded OK."));
    else    Serial.println(F("[CFG] TLS cert load FAILED!"));
    return ok;
  }

private:
  static bool loadJson(AppConfig& cfg) {
    File f = LittleFS.open("/config.json", "r");
    if (!f) {
      Serial.println(F("[CFG] config.json not found!"));
      return false;
    }

    StaticJsonDocument<1024> doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err) {
      Serial.print(F("[CFG] JSON parse error: "));
      Serial.println(err.c_str());
      return false;
    }

    strlcpy(cfg.wifi.ssid,      doc["wifi"]["ssid"]     | "", sizeof(cfg.wifi.ssid));
    strlcpy(cfg.wifi.password,  doc["wifi"]["password"] | "", sizeof(cfg.wifi.password));

    strlcpy(cfg.mqtt.broker,   doc["mqtt"]["broker"]        | "",             sizeof(cfg.mqtt.broker));
    cfg.mqtt.port = doc["mqtt"]["port"] | Defaults::MQTT_PORT;
    strlcpy(cfg.mqtt.username, doc["mqtt"]["username"]      | "",             sizeof(cfg.mqtt.username));
    strlcpy(cfg.mqtt.password, doc["mqtt"]["password"]      | "",             sizeof(cfg.mqtt.password));
    strlcpy(cfg.mqtt.topic,    doc["mqtt"]["topic"]         | "fleet/gps",   sizeof(cfg.mqtt.topic));
    strlcpy(cfg.mqtt.client_prefix,
            doc["mqtt"]["client_prefix"] | "FleetTracker", sizeof(cfg.mqtt.client_prefix));

    cfg.gps.baud                = doc["gps"]["baud"]                | Defaults::GPS_BAUD;
    cfg.gps.rxd                 = doc["gps"]["rxd"]                 | Defaults::GPS_RXD;
    cfg.gps.txd                 = doc["gps"]["txd"]                 | Defaults::GPS_TXD;
    cfg.gps.timeout_ms          = doc["gps"]["timeout_ms"]          | Defaults::GPS_TIMEOUT_MS;
    cfg.gps.min_chars           = doc["gps"]["min_chars"]           | Defaults::GPS_MIN_CHARS;
    cfg.gps.display_interval_ms = doc["gps"]["display_interval_ms"] | Defaults::DISPLAY_INTERVAL;

    cfg.tz.offset_hours   = doc["timezone"]["offset_hours"]   | Defaults::TZ_OFFSET_H;
    cfg.tz.offset_minutes = doc["timezone"]["offset_minutes"] | Defaults::TZ_OFFSET_M;

    cfg.seat.pin              = doc["seat"]["pin"]              | Defaults::SEAT_PIN;
    cfg.seat.threshold        = doc["seat"]["threshold"]        | Defaults::SEAT_THRESHOLD;
    cfg.seat.poll_interval_ms = doc["seat"]["poll_interval_ms"] | Defaults::SEAT_POLL_MS;

    return true;
  }
};
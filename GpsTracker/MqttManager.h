#pragma once

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <TinyGPS++.h>
#include <ArduinoJson.h>
#include "ConfigManager.h"

struct MqttState {
  WiFiClientSecure espClient;
  PubSubClient     client{espClient};
  unsigned long    lastReconnectMs   = 0;
  unsigned long    lastPublishMs     = 0;
  uint32_t         reconnectDelay    = 5000;
  bool             certLoaded        = false;
};

static constexpr uint32_t MQTT_RECONNECT_MAX = 60000UL;
static constexpr uint32_t MQTT_PUBLISH_MS    = 5000UL;

class MqttManager {
public:
  static void begin(MqttState& s, const MqttCfg& cfg) {
    s.client.setServer(cfg.broker, cfg.port);
    s.client.setBufferSize(512);
    s.client.setKeepAlive(30);
  }

  static void tick(MqttState& s, const MqttCfg& cfg,
                   TinyGPSPlus* gps = nullptr)
  {
    if (!s.client.connected()) {
      attemptReconnect(s, cfg);
      return;             
    }
    s.client.loop(); 

    if (millis() - s.lastPublishMs >= MQTT_PUBLISH_MS) {
      s.lastPublishMs += MQTT_PUBLISH_MS;
      publish(s, cfg, gps);
    }
  }

  static bool connected(const MqttState& s) { return s.client.connected(); }

private:
  static void attemptReconnect(MqttState& s, const MqttCfg& cfg) {
    if (millis() - s.lastReconnectMs < s.reconnectDelay) return;
    s.lastReconnectMs = millis();

    char clientId[48];
    snprintf(clientId, sizeof(clientId), "%s-%04X",
             cfg.client_prefix, (unsigned)esp_random() & 0xFFFF);

    Serial.printf("[MQTT] Connecting as %s ...\n", clientId);

    if (s.client.connect(clientId, cfg.username, cfg.password)) {
      Serial.println(F("[MQTT] Connected!"));
      s.reconnectDelay = 5000; 
    } else {
      Serial.printf("[MQTT] Failed (state=%d). Retry in %lu s\n",
                    s.client.state(), s.reconnectDelay / 1000);
      s.reconnectDelay = min(s.reconnectDelay * 2, MQTT_RECONNECT_MAX);
    }
  }

  static void publish(MqttState& s, const MqttCfg& cfg, TinyGPSPlus* gps) {
    StaticJsonDocument<256> doc;

    if (gps && gps->location.isValid()) {
      doc["lat"]  = serialized(String(gps->location.lat(),  6));
      doc["lng"]  = serialized(String(gps->location.lng(),  6));
      doc["spd"]  = gps->speed.isValid()    ? gps->speed.kmph()          : 0.0;
      doc["alt"]  = gps->altitude.isValid() ? gps->altitude.meters()     : 0.0;
      doc["sat"]  = gps->satellites.isValid()? (int)gps->satellites.value() : 0;
      doc["hdop"] = gps->hdop.isValid()     ? gps->hdop.hdop()           : 99.9;
      doc["fix"]  = true;
    } else {
      doc["fix"]  = false;
    }
    doc["ts"] = millis();    

    char buf[256];
    size_t len = serializeJson(doc, buf, sizeof(buf));

    if (s.client.publish(cfg.topic, (uint8_t*)buf, len, false)) {
      Serial.printf("[MQTT] Published %u bytes to %s\n", len, cfg.topic);
    } else {
      Serial.println(F("[MQTT] Publish FAILED"));
    }
  }
};

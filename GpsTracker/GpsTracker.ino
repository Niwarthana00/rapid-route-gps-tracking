#include "Emitter.h"
#include "ConfigManager.h"
#include "WifiManager.h"
#include "GpsManager.h"
#include "MqttManager.h"
#include "SeatManager.h"

#define GPS_RX_PIN 16     
#define GPS_TX_PIN 17      

static AppConfig  cfg;
static Emitter    em;
static WifiState  wifiState;
static GpsState   gpsState;
static MqttState  mqttState;
static SeatState  seatState;

static bool waitNtp() {
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  Serial.print(F("[NTP] Waiting for time sync"));

  time_t now = time(nullptr);
  unsigned long start = millis();
  while (now < 1580000000UL) {
    if (millis() - start > 20000UL) { 
      Serial.println(F("\n[NTP] Sync TIMEOUT - aborting MQTT!"));
      return false;
    }
    delay(500);
    Serial.print('.');
    now = time(nullptr);
  }

  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.printf("\n[NTP] Synced! UTC: %04d-%02d-%02d %02d:%02d:%02d\n",
                timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                timeinfo.tm_hour,       timeinfo.tm_min,      timeinfo.tm_sec);
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println(F("\n=== FleetTracker booting ==="));

  if (!ConfigManager::begin(cfg)) {
    Serial.println(F("[BOOT] Config load failed - halting."));
    while (true) delay(1000);
  }

  Serial2.begin(cfg.gps.baud, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  GpsManager::begin(gpsState, Serial2, cfg.gps);
  Serial.printf("[GPS] UART2 opened - baud %lu  RX=%d TX=%d\n",
                (unsigned long)cfg.gps.baud, GPS_RX_PIN, GPS_TX_PIN);

  SeatManager::begin(seatState, cfg.seat);

  bool wifiOk = WifiManager::begin(wifiState, cfg.wifi);

  if (wifiOk) {
    bool ntpOk = waitNtp();

    if (!ntpOk) {
      Serial.println(F("[BOOT] NTP failed - MQTT disabled (TLS needs valid time)."));
    } else if (ConfigManager::loadTlsCert(mqttState.espClient)) {
      MqttManager::begin(mqttState, cfg.mqtt);
      Serial.printf("[MQTT] Target: %s:%u  topic: %s\n",
                    cfg.mqtt.broker, cfg.mqtt.port, cfg.mqtt.topic);
    } else {
      Serial.println(F("[BOOT] TLS cert missing - MQTT disabled."));
    }
  } else {
    Serial.println(F("[BOOT] No WiFi - running GPS only."));
  }

  Serial.println(F("=== Boot complete ===\n"));
}

void loop() {
  GpsManager::drain(gpsState, Serial2);
  GpsManager::checkError(gpsState, cfg.gps, em);
  GpsManager::tick(gpsState, cfg.gps, cfg.tz, em);

  SeatManager::tick(seatState, cfg.seat);

  WifiManager::tick(wifiState, cfg.wifi);

  if (WifiManager::connected()) {
    MqttManager::tick(mqttState, cfg.mqtt, &gpsState.gps, &seatState);
  }
}
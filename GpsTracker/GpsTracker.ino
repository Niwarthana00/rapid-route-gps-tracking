#include "include/Emitter.h"
#include "include/ConfigManager.h"
#include "include/WifiManager.h"
#include "include/GpsManager.h"
#include "include/MqttManager.h"

static AppConfig      cfg;
static BluetoothSerial btSerial;
static Emitter        em(btSerial);
static WifiState      wifiState;
static GpsState       gpsState;
static MqttState      mqttState;

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println(F("\n=== FleetTracker booting ==="));


  if (!ConfigManager::begin(cfg)) {
    Serial.println(F("[BOOT] Config load failed – halting."));
    while (true) delay(1000);
  }

  btSerial.begin(cfg.ble.name);
  Serial.printf("[BT] Advertising as: %s\n", cfg.ble.name);

  GpsManager::begin(gpsState, Serial2, cfg.gps);
  Serial.printf("[GPS] UART2 opened – baud %lu  RX=%d TX=%d\n",
                (unsigned long)cfg.gps.baud, cfg.gps.rxd, cfg.gps.txd);

  bool wifiOk = WifiManager::begin(wifiState, cfg.wifi);

  if (wifiOk) {
    if (ConfigManager::loadTlsCert(mqttState.espClient)) {
      MqttManager::begin(mqttState, cfg.mqtt);
      Serial.printf("[MQTT] Target: %s:%u  topic: %s\n",
                    cfg.mqtt.broker, cfg.mqtt.port, cfg.mqtt.topic);
    } else {
      Serial.println(F("[BOOT] TLS cert missing – MQTT disabled."));
    }
  } else {
    Serial.println(F("[BOOT] No WiFi – running GPS/BT only."));
  }

  Serial.println(F("=== Boot complete ===\n"));
}

void loop() {
  GpsManager::drain(gpsState, Serial2);

  while (btSerial.hasClient() && btSerial.available() > 0)
    btSerial.read();

  GpsManager::checkError(gpsState, cfg.gps, em);

  GpsManager::tick(gpsState, cfg.gps, cfg.tz, em);

  WifiManager::tick(wifiState, cfg.wifi);

  if (WifiManager::connected()) {
    MqttManager::tick(mqttState, cfg.mqtt, &gpsState.gps);
  }
}

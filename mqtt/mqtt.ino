#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "LittleFS.h"
#include <time.h>

const char* ssid = "HONOR X5 Plus";
const char* password = "12345678";

const char* mqtt_broker = "h916186c.ala.eu-central-1.emqxsl.com";
const char* mqtt_topic = "emqx/esp32";
const char* mqtt_username = "Niwarthana";
const char* mqtt_password = "niwasath@123";
const int mqtt_port = 8883;

WiFiClientSecure espClient;
PubSubClient client(espClient);

void loadCertificate() {
  if (!LittleFS.begin(false, "/littlefs", 10, "spiffs")) {
    Serial.println("Failed to initialize LittleFS!");
    return;
  }

  File certFile = LittleFS.open("/root.crt", "r");
  if (!certFile) {
    Serial.println("Certificate file not found!");
    return;
  }

  if (espClient.loadCACert(certFile, certFile.size())) {
    Serial.println("SSL certificate loaded successfully.");
  } else {
    Serial.println("Failed to load SSL certificate!");
  }
  certFile.close();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // WiFi Connect
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");

  // NTP Time Sync
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  Serial.print("Waiting for NTP time sync...");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("\nTime synced!");

  // Certificate Load
  loadCertificate();

  client.setServer(mqtt_broker, mqtt_port);
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    String clientId = "ESP32Client-" + String(random(0xffff), HEX);

    if (client.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("Connected!");
    } else {
      Serial.print("Failed, state=");
      Serial.print(client.state());
      Serial.println(" Retrying in 5 seconds...");
      delay(5000);
    }
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  static unsigned long lastMsg = 0;
  if (millis() - lastMsg > 5000) {
    lastMsg = millis();
    if (client.publish(mqtt_topic, "Secured Message from ESP32 via LittleFS")) {
      Serial.println("Message sent successfully via SSL.");
    }
  }
}
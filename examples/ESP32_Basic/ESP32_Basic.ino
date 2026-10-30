/*
  FroqMQTT - Beispiel fuer ESP32

  Liest einen Analogwert, sendet ihn als float und empfaengt einen
  Schaltbefehl fuer ein Relais.
*/
#include <WiFi.h>
#include <Froq.h>

const char *WLAN_SSID = "MeinWLAN";
const char *WLAN_PASS = "MeinPasswort";
const int   RELAIS_PIN = 26;
const int   SENSOR_PIN = 34;

WiFiClient net;
Froq MqTT(net);

float spannung = 0;
bool  relais = false;
bool  online = true;

void setup() {
  Serial.begin(115200);
  pinMode(RELAIS_PIN, OUTPUT);

  WiFi.begin(WLAN_SSID, WLAN_PASS);
  while (WiFi.status() != WL_CONNECTED) delay(250);
  Serial.println(WiFi.localIP());

  MqTT.publish("esp32/spannung", &spannung, 0, 2);       // 2 Nachkommastellen
  MqTT.publish("esp32/status", &online, 1, "online", "offline");
  MqTT.subscribe("esp32/relais", &relais, 1);            // erwartet "ON" / "OFF"

  MqTT.lastWill("esp32/status", "offline", 1);
  MqTT.setRetry(30);
  MqTT.begin(IPAddress(192, 168, 1, 10), "FroqESP32", "user", "passwort");
}

void loop() {
  MqTT.loop();
  digitalWrite(RELAIS_PIN, relais);

  static uint32_t t = 0;
  if (millis() - t >= 2000) {
    t = millis();
    spannung = analogRead(SENSOR_PIN) * 3.3f / 4095.0f;
  }
}

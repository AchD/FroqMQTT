/*
  FroqMQTT - Beispiel fuer ESP8266 (NodeMCU, Wemos D1 mini, ESP-12E ...)

  - "test/licht"    : LED-Zustand wird bei Aenderung gesendet (EIN/AUS)
  - "test/zahl"     : Zaehler wird gesendet (float, 1 Nachkommastelle)
  - "test/sollwert" : Wert vom Broker landet in der Variable sollwert
  - "test/taste"    : EIN/AUS vom Broker schaltet die LED
*/
#include <ESP8266WiFi.h>
#include <Froq.h>

const char *WLAN_SSID = "MeinWLAN";
const char *WLAN_PASS = "MeinPasswort";

WiFiClient net;          // TCP-Verbindung
Froq MqTT(net);          // MQTT ueber diese Verbindung

bool  licht = false;
bool  taste = false;
float zahl = 0;
float sollwert = 0;

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WLAN_SSID, WLAN_PASS);
  Serial.print("WLAN verbinden");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print('.'); }
  Serial.print(" OK, IP: ");
  Serial.println(WiFi.localIP());

  // Variablen verknuepfen (vor oder nach begin() moeglich)
  MqTT.publish("test/licht", &licht, 1, "EIN", "AUS");   // bool, QoS 1, eigene Texte
  MqTT.publish("test/zahl", &zahl, 0, 1);               // float, QoS 0, 1 Nachkommastelle
  MqTT.subscribe("test/sollwert", &sollwert, 1);
  MqTT.subscribe("test/taste", &taste, 1, "EIN", "AUS");

  MqTT.lastWill("test/status", "offline");
  MqTT.setRetry(30);                                     // nach Abbruch alle 30 s neu verbinden
  MqTT.begin("test.mosquitto.org", "FroqESP8266");
  // oder mit IP und Login:
  // MqTT.begin(IPAddress(192, 168, 1, 10), "FroqESP8266", "user", "passwort");
}

void loop() {
  MqTT.loop();                 // so oft wie moeglich aufrufen, kein langes delay()!

  digitalWrite(LED_BUILTIN, taste ? LOW : HIGH);   // LED ist low-aktiv
  licht = taste;

  static uint32_t t = 0;
  if (millis() - t >= 5000) {
    t = millis();
    zahl += 0.5;
    Serial.printf("Sollwert: %.2f  Taste: %d  verbunden: %d\n",
                  sollwert, taste, MqTT.state(MqTTsessionAlive));
  }
}

/*
  FroqMQTT - Beispiel fuer Arduino Uno/Mega mit Ethernet-Shield (W5100/W5500)

  Auf AVR-Boards gilt: FROQ_MAX_ITEMS = 10, FROQ_RX_BUFFER = 128 Byte
  (siehe src/configFroq.h). Telegramme > 128 Byte werden uebersprungen.
*/
#include <SPI.h>
#include <Ethernet.h>
#include <Froq.h>

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x01 };

EthernetClient net;
Froq MqTT(net);

bool  tuer = false;      // Eingang -> Broker
bool  lampe = false;     // Broker -> Ausgang
float temperatur = 0;

void setup() {
  Serial.begin(9600);
  pinMode(2, INPUT_PULLUP);
  pinMode(7, OUTPUT);

  if (Ethernet.begin(mac) == 0) {
    Serial.println(F("DHCP fehlgeschlagen"));
    while (true) {}
  }
  Serial.println(Ethernet.localIP());

  MqTT.publish("keller/tuer", &tuer, 0, "offen", "zu");
  MqTT.publish("keller/temperatur", &temperatur, 0, 1);
  MqTT.subscribe("keller/lampe", &lampe);
  MqTT.setRetry(30);
  MqTT.begin(IPAddress(192, 168, 1, 10), "FroqUno");
}

void loop() {
  MqTT.loop();
  Ethernet.maintain();

  tuer = digitalRead(2) == HIGH;
  digitalWrite(7, lampe);

  static uint32_t t = 0;
  if (millis() - t >= 10000) {
    t = millis();
    temperatur = analogRead(A0) * 0.488f;   // z.B. LM35
  }
}

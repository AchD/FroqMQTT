# FroqMQTT

[![CI](https://github.com/AchD/FroqMQTT/actions/workflows/ci.yml/badge.svg)](https://github.com/AchD/FroqMQTT/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

**Schlanke MQTT-Library für Arduino: Variablen direkt mit MQTT-Topics verknüpfen.**
*Lightweight MQTT 3.1.1 client for Arduino that binds `bool`/`float` variables directly to topics.*

```cpp
#include <ESP8266WiFi.h>
#include <Froq.h>

WiFiClient net;
Froq MqTT(net);

bool  licht;
float sollwert;

void setup() {
  WiFi.begin("MeinWLAN", "MeinPasswort");
  while (WiFi.status() != WL_CONNECTED) delay(500);

  MqTT.publish("haus/licht", &licht, 1, "EIN", "AUS");   // wird bei Änderung gesendet
  MqTT.subscribe("haus/sollwert", &sollwert);            // wird vom Broker gesetzt
  MqTT.begin(IPAddress(192, 168, 1, 10), "MeinESP", "user", "passwort");
}

void loop() {
  MqTT.loop();
}
```

## Eigenschaften

- `publish()` sendet automatisch bei Wertänderung, `subscribe()` schreibt direkt in die Variable
- MQTT 3.1.1, QoS 0 / 1 / 2, Last Will, Keep-Alive, automatische Wiederverbindung
- verarbeitet beliebig viele Telegramme pro TCP-Paket, auch über Paketgrenzen verteilt
- fester RAM-Bedarf, kein Heap im laufenden Betrieb (ESP8266: ca. 1,6 KB, Uno: ca. 0,45 KB)
- läuft mit jedem Arduino-`Client`: ESP8266, ESP32, Ethernet-Shield, WiFiNINA, `WiFiClientSecure` (TLS)

## Installation

**Arduino IDE:** *Code → Download ZIP*, dann *Sketch → Bibliothek einbinden → .ZIP-Bibliothek hinzufügen*.

**PlatformIO:**
```ini
lib_deps = https://github.com/AchD/FroqMQTT.git
```

## Dokumentation

📖 **[Ausführliche Anleitung (Deutsch)](ANLEITUNG.md)**: Installation, alle Funktionen, Einstellungen,
Speicherbedarf, Grenzen und Fehlersuche.

Beispiele: [ESP8266](examples/ESP8266_Basic/ESP8266_Basic.ino) ·
[ESP32](examples/ESP32_Basic/ESP32_Basic.ino) ·
[Arduino + Ethernet](examples/Ethernet_Basic/Ethernet_Basic.ino)

## Kurzreferenz

| Funktion | |
|---|---|
| `begin(broker, clientID [, port] [, user, pass])` | verbinden (IP oder Hostname) |
| `publish(topic, &var [, qos] [, "EIN", "AUS"])` | bool senden |
| `publish(topic, &var [, qos [, nachkommastellen]])` | float senden |
| `subscribe(topic, &var [, qos] [, "EIN", "AUS"])` | bool/float empfangen |
| `lastWill(topic, msg [, qos])` | Last Will |
| `loop()` | zyklisch aufrufen |
| `state(MqTTsessionAlive / …Startup / …Refresh / …Abort)` | Status |

## Tests

```bash
make -C extras/test
```

Der Host-Test simuliert einen Broker, z. B. mit mehreren Telegrammen pro TCP-Segment, byteweise
geteilten Telegrammen, zu großen Paketen, QoS 0/1/2, Ping und Wiederverbindung. Die GitHub-Action
kompiliert zusätzlich die Beispiele für ESP8266, ESP32 und Arduino Uno.

## Lizenz

MIT © 2026 Achim Diedrichs. Siehe [LICENSE](LICENSE).

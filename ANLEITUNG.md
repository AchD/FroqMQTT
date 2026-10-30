# FroqMQTT – Anleitung

FroqMQTT verbindet Variablen in deinem Arduino-Sketch direkt mit Topics auf einem MQTT-Broker
(z. B. Mosquitto, ioBroker, Home Assistant, Node-RED).
Du schreibst kein Senden und kein Auswerten von Telegrammen mehr selbst:

- **publish** – ändert sich die Variable, wird der neue Wert automatisch an den Broker gesendet.
- **subscribe** – schickt der Broker einen Wert, steht er automatisch in der Variable.

Unterstützt werden `bool` und `float`, MQTT 3.1.1 mit QoS 0, 1 und 2, Last Will, Keep-Alive und
automatische Wiederverbindung.

---

## Inhalt

1. [Voraussetzungen](#1-voraussetzungen)
2. [Installation](#2-installation)
3. [Schnellstart](#3-schnellstart)
4. [Verbindung zum Broker](#4-verbindung-zum-broker)
5. [Werte senden – publish](#5-werte-senden--publish)
6. [Werte empfangen – subscribe](#6-werte-empfangen--subscribe)
7. [Verbindungsstatus abfragen](#7-verbindungsstatus-abfragen)
8. [Last Will](#8-last-will)
9. [QoS – Quality of Service](#9-qos--quality-of-service)
10. [Einstellungen](#10-einstellungen)
11. [Speicherbedarf](#11-speicherbedarf)
12. [Grenzen](#12-grenzen)
13. [Fehlersuche](#13-fehlersuche)
14. [Funktionsübersicht](#14-funktionsübersicht)

---

## 1. Voraussetzungen

| Hardware | Netzwerk-Client |
|---|---|
| ESP8266 (NodeMCU, Wemos D1, ESP-01, ESP-12) | `WiFiClient` aus `<ESP8266WiFi.h>` |
| ESP32 | `WiFiClient` aus `<WiFi.h>` |
| Arduino Uno / Mega / Nano + Ethernet-Shield | `EthernetClient` aus `<Ethernet.h>` |
| alle anderen Boards mit Arduino-`Client` | z. B. `WiFiNINA`, `WiFiClientSecure` (TLS) |

Außerdem brauchst du einen erreichbaren MQTT-Broker. Zum Ausprobieren eignet sich der öffentliche
Testbroker `test.mosquitto.org` (Port 1883, ohne Login – nicht für echte Daten verwenden).

---

## 2. Installation

### Arduino IDE

**Variante A – ZIP-Datei**
1. Auf GitHub unter *Code → Download ZIP* (oder unter *Releases*) die ZIP-Datei laden.
2. Arduino IDE: *Sketch → Bibliothek einbinden → .ZIP-Bibliothek hinzufügen …*
3. Die Beispiele findest du danach unter *Datei → Beispiele → FroqMQTT*.

**Variante B – von Hand**
Den Ordner `FroqMQTT` nach `Dokumente/Arduino/libraries/` kopieren und die IDE neu starten.

### PlatformIO

In der `platformio.ini`:

```ini
[env:esp12e]
platform = espressif8266
board = esp12e
framework = arduino
lib_deps =
    https://github.com/AchD/FroqMQTT.git
```

Oder den Ordner `FroqMQTT` in den `lib/`-Ordner deines Projekts kopieren.

### Einbinden im Sketch

```cpp
#include <Froq.h>
```

---

## 3. Schnellstart

```cpp
#include <ESP8266WiFi.h>
#include <Froq.h>

WiFiClient net;
Froq MqTT(net);

bool  licht = false;
float sollwert = 0;

void setup() {
  WiFi.begin("MeinWLAN", "MeinPasswort");
  while (WiFi.status() != WL_CONNECTED) delay(500);

  MqTT.publish("haus/licht", &licht);          // senden
  MqTT.subscribe("haus/sollwert", &sollwert);  // empfangen
  MqTT.begin("test.mosquitto.org", "MeinESP");
}

void loop() {
  MqTT.loop();          // immer aufrufen!
  licht = digitalRead(D5);
}
```

Das ist alles: Ändert sich `licht`, erscheint auf dem Broker unter `haus/licht` der Text `ON` bzw.
`OFF`. Schreibt jemand `21.5` nach `haus/sollwert`, steht in `sollwert` der Wert `21.5`.

> **Wichtig:** `MqTT.loop()` muss regelmäßig laufen. Lange `delay()`-Aufrufe in `loop()`
> verzögern das Empfangen. Besser mit `millis()` arbeiten (siehe Beispiele).

---

## 4. Verbindung zum Broker

Zuerst wird ein Netzwerk-Client erzeugt und an Froq übergeben:

```cpp
WiFiClient net;
Froq MqTT(net);
```

Die Verbindung startet mit `begin()`. Der Broker kann als IP-Adresse oder als Hostname angegeben
werden, Port und Login sind optional:

```cpp
MqTT.begin(IPAddress(192, 168, 1, 10), "ClientID");                        // Port 1883
MqTT.begin(IPAddress(192, 168, 1, 10), "ClientID", 1884);                  // anderer Port
MqTT.begin(IPAddress(192, 168, 1, 10), "ClientID", "user", "passwort");    // mit Login
MqTT.begin(IPAddress(192, 168, 1, 10), "ClientID", 1884, "user", "passwort");
MqTT.begin("broker.local", "ClientID");                                    // Hostname
MqTT.begin("broker.local", "ClientID", "user", "passwort");
```

- Die **Client-ID** muss auf dem Broker eindeutig sein. Zwei Geräte mit derselben ID werfen
  sich gegenseitig vom Broker.
- Bricht die Verbindung ab (WLAN weg, Broker neu gestartet), verbindet Froq sich automatisch
  neu. Die Wartezeit stellst du mit `setRetry()` ein (Standard 300 s).
- Nach jeder (Wieder-)Verbindung werden alle Subscribes neu angemeldet und alle
  Publish-Variablen einmal mit ihrem aktuellen Wert gesendet.
- `MqTT.end()` trennt die Verbindung sauber; danach wird nicht automatisch neu verbunden.

### Verschlüsselte Verbindung (TLS)

Froq arbeitet mit jedem `Client`, also auch mit `WiFiClientSecure`:

```cpp
WiFiClientSecure net;          // ESP8266 / ESP32
Froq MqTT(net);
...
net.setInsecure();             // oder Zertifikat setzen: net.setCACert(...)
MqTT.begin("broker.example.com", "ClientID", 8883, "user", "passwort");
```

---

## 5. Werte senden – publish

### bool

```cpp
bool pumpe;
MqTT.publish("garten/pumpe", &pumpe);                       // sendet "ON" / "OFF", QoS 0
MqTT.publish("garten/pumpe", &pumpe, 1);                    // QoS 1
MqTT.publish("garten/pumpe", &pumpe, "EIN", "AUS");         // eigene Texte
MqTT.publish("garten/pumpe", &pumpe, 1, "1", "0");          // QoS 1 + eigene Texte
```

### float

```cpp
float temp;
MqTT.publish("garten/temp", &temp);          // QoS 0, 2 Nachkommastellen -> "21.37"
MqTT.publish("garten/temp", &temp, 1);       // QoS 1
MqTT.publish("garten/temp", &temp, 0, 1);    // QoS 0, 1 Nachkommastelle -> "21.4"
```

### Wann wird gesendet?

- direkt nach dem Verbindungsaufbau (aktueller Wert),
- wenn sich der Wert ändert – frühestens aber `FROQ_MIN_MILLIS_PUBLISH` Millisekunden
  (Standard 3000 ms) nach dem letzten Senden dieser Variable. So wird der Broker bei
  schnell wechselnden Werten nicht überflutet. Der letzte Wert geht nie verloren, er wird
  nach Ablauf der Sperrzeit gesendet.

> **Tipp für Messwerte:** Ein `float`, das ständig minimal schwankt, wird alle 3 s gesendet.
> Runde den Wert selbst, wenn du weniger Telegramme willst, z. B.
> `temp = roundf(messwert * 10) / 10;`

`publish()` gibt `true` zurück, wenn die Variable angelegt wurde, und `false`, wenn
`FROQ_MAX_ITEMS` erreicht ist.

---

## 6. Werte empfangen – subscribe

```cpp
bool  heizung;
float sollwert;

MqTT.subscribe("haus/heizung", &heizung);                    // erwartet "ON" / "OFF"
MqTT.subscribe("haus/heizung", &heizung, "EIN", "AUS");      // eigene Texte
MqTT.subscribe("haus/heizung", &heizung, 1, "EIN", "AUS");   // mit QoS 1
MqTT.subscribe("haus/sollwert", &sollwert);                  // float
MqTT.subscribe("haus/sollwert", &sollwert, 2);               // float, QoS 2
```

- Bei `bool` wird die Variable nur geändert, wenn der empfangene Text **genau** dem True- bzw.
  False-Text entspricht (Groß-/Kleinschreibung beachten). Andere Texte werden ignoriert.
- Bei `float` wird der Text mit `atof()` umgewandelt. `"21.5"`, `"-3"`, `"1e3"` funktionieren.
  Komma statt Punkt (`"21,5"`) ergibt `21`.
- Mehrere Variablen dürfen dasselbe Topic abonnieren – alle werden gesetzt.
- Gespeicherte Werte (Retained) liefert der Broker direkt nach dem Subscribe, die Variable hat
  also kurz nach dem Start bereits den aktuellen Wert.

### Auf neue Werte reagieren

Froq setzt nur die Variable. Wenn du auf eine Änderung reagieren willst, merke dir den alten Wert:

```cpp
void loop() {
  MqTT.loop();
  static float alt = sollwert;
  if (sollwert != alt) {
    alt = sollwert;
    Serial.println("Neuer Sollwert!");
  }
}
```

---

## 7. Verbindungsstatus abfragen

```cpp
if (MqTT.state(MqTTsessionAlive))   { /* verbunden */ }
if (MqTT.state(MqTTsessionStartup)) { /* Verbindung / Subscribes laufen */ }
if (MqTT.state(MqTTsessionRefresh)) { /* Ping läuft */ }
if (MqTT.state(MqTTsessionAbort))   { /* getrennt, wartet auf Neuverbindung */ }
```

Zusätzlich gibt es aus Kompatibilitätsgründen die Variable `MqTT.sessionAlive` (`true` = verbunden).

---

## 8. Last Will

Der „letzte Wille“ wird vom **Broker** veröffentlicht, wenn das Gerät unerwartet verschwindet
(Strom weg, WLAN weg). Er muss **vor** `begin()` gesetzt werden:

```cpp
MqTT.lastWill("haus/esp/status", "offline");        // QoS 0
MqTT.lastWill("haus/esp/status", "offline", 1);     // QoS 1
```

Kombiniert mit einer Publish-Variable ergibt das eine Online-Anzeige:

```cpp
bool online = true;
MqTT.publish("haus/esp/status", &online, 1, "online", "offline");
MqTT.lastWill("haus/esp/status", "offline", 1);
```

---

## 9. QoS – Quality of Service

| QoS | Bedeutung | Einsatz |
|---|---|---|
| 0 | höchstens einmal, ohne Bestätigung | Messwerte, die sowieso regelmäßig kommen |
| 1 | mindestens einmal, wird bestätigt und ggf. wiederholt | Schaltbefehle, Zustände |
| 2 | genau einmal (4-Wege-Handshake) | selten nötig, langsamer |

Froq wiederholt unbestätigte Telegramme nach `setRepublish()` Sekunden (Standard 30 s) mit
gesetztem DUP-Flag. Beim Subscribe kann der Broker einen niedrigeren QoS vergeben; Froq übernimmt
diesen automatisch.

---

## 10. Einstellungen

### Zur Laufzeit (vor `begin()` aufrufen)

```cpp
MqTT.setKeepAlive(60);      // Sekunden, 0 = aus. Ohne Datenverkehr wird nach 90 % ein Ping gesendet
MqTT.setRetry(30);          // Sekunden bis zur Neuverbindung nach Abbruch
MqTT.setRepublish(30);      // Sekunden bis zur Wiederholung unbestätigter QoS-1/2-Telegramme
MqTT.setCleanSession(true); // true = Broker vergisst Subscribes beim Trennen
```

### Beim Kompilieren (`src/configFroq.h`)

| Name | ESP8266/ESP32 | andere (AVR) | Bedeutung |
|---|---|---|---|
| `FROQ_MAX_ITEMS` | 32 | 10 | Anzahl `publish()` + `subscribe()` zusammen |
| `FROQ_RX_BUFFER` | 512 | 128 | größtes empfangbares Telegramm in Byte |
| `FROQ_TX_BUFFER` | 64 | 64 | Sendepuffer; Telegramme dürfen trotzdem größer sein |
| `FROQ_PAYLOAD_MAX` | 24 | 24 | max. ausgewertete Zeichen eines float-Wertes |
| `FROQ_DEFAULT_KEEPALIVE` | 60 | 60 | Keep-Alive in s |
| `FROQ_DEFAULT_RETRY` | 300 | 300 | Neuverbindung in s |
| `FROQ_DEFAULT_STARTUP` | 60 | 60 | max. Wartezeit auf Antwort des Brokers beim Verbinden in s |
| `FROQ_DEFAULT_REPUBLISH` | 30 | 30 | Wiederholung QoS > 0 in s |
| `FROQ_DEFAULT_PORT` | 1883 | 1883 | Standard-Port |
| `FROQ_DEFAULT_CLEANSESSION` | false | false | Clean Session |
| `FROQ_DEFAULT_RETAIN` | false | false | Retain-Flag für alle Publish |
| `FROQ_DEFAULT_BOOL_STATE_TRUE` | "ON" | "ON" | Standard-Text für `true` |
| `FROQ_DEFAULT_BOOL_STATE_FALSE` | "OFF" | "OFF" | Standard-Text für `false` |
| `FROQ_DEFAULT_QoS` | 0 | 0 | Standard-QoS |
| `FROQ_DEFAULT_DECIMAL_PLACES` | 2 | 2 | Nachkommastellen bei float |
| `FROQ_MIN_MILLIS_PUBLISH` | 3000 | 3000 | Sperrzeit zwischen zwei Sendungen derselben Variable in ms |

**Ändern in PlatformIO** (empfohlen, die Library-Dateien bleiben unverändert):

```ini
build_flags = -D FROQ_MAX_ITEMS=60 -D FROQ_RX_BUFFER=1024 -D FROQ_DEFAULT_RETAIN=true
```

**Ändern in der Arduino IDE:** die Werte direkt in
`Dokumente/Arduino/libraries/FroqMQTT/src/configFroq.h` anpassen.

> **Achtung:** Ein `#define FROQ_…` im Sketch vor `#include <Froq.h>` wirkt **nicht**, weil
> `Froq.cpp` getrennt kompiliert wird und den Wert nicht sieht. Das führt im schlimmsten Fall zu
> Abstürzen.

---

## 11. Speicherbedarf

Der gesamte Speicher wird beim Anlegen des `Froq`-Objekts fest reserviert – es gibt keine
Heap-Fragmentierung im laufenden Betrieb.

| Board | Standard-Einstellung | RAM |
|---|---|---|
| ESP8266 / ESP32 | 32 Variablen, 512 Byte Empfang | ca. 1,6 KB |
| Arduino Uno / Mega | 10 Variablen, 128 Byte Empfang | ca. 0,45 KB |

Pro zusätzlicher Variable kommen auf dem ESP 28 Byte hinzu, auf AVR 20 Byte.

---

## 12. Grenzen

- **Nur `bool` und `float`.** Für `int` einfach eine `float`-Hilfsvariable mit 0 Nachkommastellen
  verwenden: `MqTT.publish("zaehler", &f, 0, 0);`
- **Keine Wildcards** (`+`, `#`) beim Subscribe – das Topic muss exakt übereinstimmen.
- **Topics und Texte werden nicht kopiert.** Topic, Client-ID, Broker-Hostname und die
  True-/False-Texte müssen während der ganzen Laufzeit gültig bleiben. String-Literale
  (`"haus/licht"`) und globale Konstanten sind richtig; ein `String` aus einer lokalen Variable
  oder `meinString.c_str()` ist falsch.
- **Telegramme größer als `FROQ_RX_BUFFER`** werden übersprungen (QoS 1/2 wird trotzdem
  bestätigt). Folgende Telegramme werden normal verarbeitet.
- **Ein Telegramm pro Topic-Wert:** Ein Publish-Item sendet immer den aktuellen Wert; Zwischenwerte
  innerhalb der Sperrzeit werden nicht einzeln gesendet.
- **MQTT 3.1.1.** MQTT 5 wird nicht unterstützt (alle gängigen Broker sprechen 3.1.1).

---

## 13. Fehlersuche

### Debug-Ausgabe einschalten

In `src/configFroq.h` die Zeile `//#define DEBUG_FROQ` aktivieren (oder `-D DEBUG_FROQ` in den
`build_flags`). Dann erscheinen auf `Serial` alle empfangenen Telegramm-Typen und Fehler.

### Mitlesen am Broker

```bash
mosquitto_sub -h 192.168.1.10 -u user -P passwort -t '#' -v
mosquitto_pub -h 192.168.1.10 -u user -P passwort -t haus/sollwert -m 21.5
```

### Häufige Probleme

| Problem | Ursache / Lösung |
|---|---|
| Verbindet nie (`MqTTsessionAbort`) | IP/Port falsch, Broker nicht erreichbar, Firewall. Login prüfen. Mit `setRetry(10)` schneller neu versuchen. |
| Verbindet, fliegt aber sofort raus | Client-ID doppelt vergeben, oder Benutzer hat keine Rechte auf das Topic. |
| bool-Variable ändert sich nicht | Text stimmt nicht exakt (`"on"` ≠ `"ON"`). Eigene Texte angeben. |
| float wird ständig gesendet | Wert schwankt – runden oder `FROQ_MIN_MILLIS_PUBLISH` erhöhen. |
| `publish()`/`subscribe()` liefert `false` | `FROQ_MAX_ITEMS` zu klein. |
| Lange Topics/Werte kommen nicht an | Telegramm größer als `FROQ_RX_BUFFER` – Puffer vergrößern. |
| Werte kommen verzögert an | `loop()` blockiert (lange `delay()`), `MqTT.loop()` wird zu selten aufgerufen. |
| Absturz nach `subscribe()` mit `String` | Topic muss dauerhaft gültig sein, siehe [Grenzen](#12-grenzen). |

---

## 14. Funktionsübersicht

| Funktion | Beschreibung |
|---|---|
| `Froq(Client &client)` | Objekt anlegen |
| `begin(broker, clientID [, port] [, user, passwort])` | verbinden (IP oder Hostname) |
| `end()` | sauber trennen |
| `loop()` | zyklisch aufrufen |
| `publish(topic, &bool [, qos] [, trueText, falseText])` | bool senden |
| `publish(topic, &float [, qos [, nachkommastellen]])` | float senden |
| `subscribe(topic, &bool [, qos] [, trueText, falseText])` | bool empfangen |
| `subscribe(topic, &float [, qos])` | float empfangen |
| `lastWill(topic, nachricht [, qos])` | Last Will (vor `begin()`) |
| `state(MqTTsession…)` | Status abfragen |
| `setKeepAlive(s)`, `setRetry(s)`, `setRepublish(s)`, `setCleanSession(b)` | Laufzeit-Einstellungen |
| `sessionAlive` | `true` = verbunden |

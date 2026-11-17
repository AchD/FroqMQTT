# Changelog

## 2.0.1 – 2026-11-17

Stabilere Verbindung bei längeren Pausen ohne Datenverkehr
(Router/WLAN trennen ruhige TCP-Verbindungen oft still).

### Verbessert
- Schlägt das Senden fehl (`write()`), wird sofort neu verbunden statt erst nach dem Ping-Timeout.
- Ping wird jetzt auch gesendet, wenn längere Zeit nichts **empfangen** wurde. Wer regelmäßig mit
  QoS 0 sendet, bemerkt eine tote Verbindung damit ebenfalls.
- Keine Antwort auf den Ping: Abbruch nach `FROQ_PING_TIMEOUT` (10 s) statt nach der vollen
  Keep-Alive-Zeit.
- Neu: `setConnectionCheck()` – meldet das Netzwerk (z. B. WLAN) „weg“, wird die MQTT-Verbindung sofort
  als getrennt markiert; ist es wieder da, wird sofort neu verbunden. Ohne Netzwerk kein blockierender
  Verbindungsversuch mehr.
- Beim Abbruch unbestätigte QoS-1/2-Telegramme werden nach der Neuverbindung mit gleicher Packet-ID und
  DUP-Flag sofort wiederholt (bei `cleanSession = false`).
- Standard für `FROQ_DEFAULT_RETRY` von 300 s auf 30 s gesenkt.

### Behoben
- Zeitstempel innerhalb von `loop()`: Wurde im selben Durchlauf gesendet oder empfangen, konnte die
  Zeitdifferenz überlaufen. Folge waren doppelte Subscribes direkt nach dem Verbinden, unnötige Pings und
  zu frühe Wiederholungen. Der Host-Test läuft jetzt zusätzlich mit einer weiterlaufenden Uhr.
- Beispiele: WLAN-Stromsparmodus aus, `setConnectionCheck()` gesetzt.

## 2.0.0 – 2026-10-30

Komplette Überarbeitung des Empfangs- und Sendeteils. Die öffentliche API ist kompatibel zu 1.3.

### Behoben
- Mehrere MQTT-Telegramme in einem TCP-Segment wurden nicht vollständig verarbeitet –
  jetzt werden alle Telegramme in einem `loop()` ausgewertet.
- Telegramme, die über mehrere TCP-Segmente verteilt ankommen, werden korrekt zusammengesetzt.
- Remaining Length > 127 Byte (Variable Length Encoding) wird beim Senden und Empfangen unterstützt.
- Nicht initialisierte Schleifenzähler bei der Topic-Suche (Werte kamen nicht an).
- PUBACK/PUBREC wird auch für nicht abonnierte Topics gesendet (Broker wiederholte endlos).
- User- und Passwort-Flag im CONNECT waren vertauscht.
- Subscribe-Packet-IDs haben den Publish-Zähler verschoben.
- Falsche Formel beim Subscribe-Timeout.
- Wiederholte QoS-Publish-Telegramme enthielten keinen Wert.
- `millis()`-Überlauf nach 49 Tagen.

### Neu
- Fester, deutlich kleinerer RAM-Bedarf (ESP8266: ca. 1,6 KB statt ca. 4,7 KB).
- Ein `write()` pro Telegramm statt Byte für Byte.
- Zu große Telegramme werden übersprungen statt den Puffer zu überschreiben.
- `publish(topic, &float, qos, nachkommastellen)`.
- `setKeepAlive()`, `setRetry()`, `setRepublish()`, `setCleanSession()`.
- `publish()`/`subscribe()` liefern `true`/`false`.
- Mehrere Froq-Instanzen (mehrere Broker) möglich.
- Kompatibel mit AVR-Boards (Uno/Mega + Ethernet).

## 1.3
- Erste Version (nicht veröffentlicht).

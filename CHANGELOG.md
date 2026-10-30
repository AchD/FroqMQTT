# Changelog

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

/*
  FroqMQTT - schlanke MQTT 3.1.1 Client-Library fuer Arduino
  https://github.com/AchD/FroqMQTT
  Copyright (c) 2026 Achim Diedrichs - MIT License (siehe LICENSE)
*/
// Froq - Konfiguration
// ---------------------------------------------------------------------------
// Werte hier aendern oder per build_flags in platformio.ini ueberschreiben,
// z.B.  build_flags = -D FROQ_MAX_ITEMS=50 -D FROQ_RX_BUFFER=1024
// ACHTUNG: NICHT per #define in main.cpp - Froq.cpp wuerde den Wert nicht sehen.
#ifndef FROQ_CONFIG_H
#define FROQ_CONFIG_H

// Speicher ------------------------------------------------------------------
#if defined(ESP8266) || defined(ESP32)
  #ifndef FROQ_MAX_ITEMS
  #define FROQ_MAX_ITEMS 32        // Anzahl publish() + subscribe() zusammen
  #endif
  #ifndef FROQ_RX_BUFFER
  #define FROQ_RX_BUFFER 512       // groesstes empfangbares MQTT-Paket (Bytes)
  #endif
#else // AVR & Co. mit wenig RAM
  #ifndef FROQ_MAX_ITEMS
  #define FROQ_MAX_ITEMS 10
  #endif
  #ifndef FROQ_RX_BUFFER
  #define FROQ_RX_BUFFER 128
  #endif
#endif
#ifndef FROQ_TX_BUFFER
#define FROQ_TX_BUFFER 64          // Sendepuffer, Pakete beliebiger Laenge werden stueckweise gesendet
#endif
#ifndef FROQ_PAYLOAD_MAX
#define FROQ_PAYLOAD_MAX 24        // max. ausgewertete Zeichen einer Float-Nutzlast
#endif

// Standardeinstellungen der MQTT-Verbindung ---------------------------------
#ifndef FROQ_DEFAULT_KEEPALIVE
#define FROQ_DEFAULT_KEEPALIVE 60        // Keep-Alive in Sekunden
#endif
#ifndef FROQ_DEFAULT_RETRY
#define FROQ_DEFAULT_RETRY 300           // Wiederverbindung nach Abbruch in Sekunden
#endif
#ifndef FROQ_DEFAULT_STARTUP
#define FROQ_DEFAULT_STARTUP 60          // max. Wartezeit auf CONNACK in Sekunden
#endif
#ifndef FROQ_DEFAULT_REPUBLISH
#define FROQ_DEFAULT_REPUBLISH 30        // erneutes Senden QoS>0 ohne Antwort in Sekunden
#endif
#ifndef FROQ_DEFAULT_PORT
#define FROQ_DEFAULT_PORT 1883
#endif
#ifndef FROQ_DEFAULT_CLEANSESSION
#define FROQ_DEFAULT_CLEANSESSION false
#endif
#ifndef FROQ_DEFAULT_RETAIN
#define FROQ_DEFAULT_RETAIN false
#endif
#ifndef FROQ_DEFAULT_BOOL_STATE_TRUE
#define FROQ_DEFAULT_BOOL_STATE_TRUE "ON"
#endif
#ifndef FROQ_DEFAULT_BOOL_STATE_FALSE
#define FROQ_DEFAULT_BOOL_STATE_FALSE "OFF"
#endif
#ifndef FROQ_DEFAULT_QoS
#define FROQ_DEFAULT_QoS 0
#endif
#ifndef FROQ_DEFAULT_DECIMAL_PLACES
#define FROQ_DEFAULT_DECIMAL_PLACES 2    // Nachkommastellen beim Senden von float
#endif
#ifndef FROQ_MIN_MILLIS_PUBLISH
#define FROQ_MIN_MILLIS_PUBLISH 3000     // min. ms zwischen zwei Publish desselben Wertes
#endif

// Zum Debuggen einkommentieren (Ausgabe auf Serial):
//#define DEBUG_FROQ

#endif

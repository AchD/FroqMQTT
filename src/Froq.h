/*
  FroqMQTT - schlanke MQTT 3.1.1 Client-Library fuer Arduino (ESP8266, ESP32, AVR + Ethernet ...)
  Version 2.0.0  -  https://github.com/AchD/FroqMQTT
  Copyright (c) 2026 Achim Diedrichs - MIT License (siehe LICENSE)

  Arduino-Variablen (bool / float) werden mit MQTT-Topics verknuepft:
    publish(...)   -> Wert wird bei Aenderung automatisch an den Broker gesendet
    subscribe(...) -> Wert wird automatisch gesetzt, wenn der Broker ein Telegramm schickt

  Beispiel:
    WiFiClient net;
    Froq MqTT(net);
    MqTT.begin(IPAddress(192,168,1,10), "Client", "user", "pass");
    MqTT.publish("haus/licht", &licht, 1, "EIN", "AUS");
    MqTT.subscribe("haus/soll", &sollwert);
    loop(): MqTT.loop();

  Topics, Client-ID, Broker-URL und True/False-Texte werden NICHT kopiert:
  Bitte String-Literale oder globale Konstanten uebergeben.
*/
#ifndef CLASS_froq
#define CLASS_froq

#include <Arduino.h>
#include <Client.h>
#include <IPAddress.h>
#include "configFroq.h"

// Statusabfrage mit state()
#define MqTTsessionAlive    1
#define MqTTsessionRefresh  2
#define MqTTsessionStartup  3
#define MqTTsessionAbort    4

class Froq
{
public:
    explicit Froq(Client &mqttClient);

    // Verbindung zum Broker (IP-Adresse oder Hostname)
    void begin(IPAddress mqttBroker, const char *clientID, uint16_t mqttPort, String userName, String password);
    void begin(IPAddress mqttBroker, const char *clientID, String userName, String password);
    void begin(IPAddress mqttBroker, const char *clientID, uint16_t mqttPort);
    void begin(IPAddress mqttBroker, const char *clientID);
    void begin(const char *mqttBroker, const char *clientID, uint16_t mqttPort, String userName, String password);
    void begin(const char *mqttBroker, const char *clientID, String userName, String password);
    void begin(const char *mqttBroker, const char *clientID, uint16_t mqttPort);
    void begin(const char *mqttBroker, const char *clientID);
    // Verbindung sauber abbauen (keine automatische Wiederverbindung)
    void end();

    // Last Will - vor begin() aufrufen
    void lastWill(String topic, String message, int QoS);
    void lastWill(String topic, String message);

    // Variable -> Broker (Rueckgabe false, wenn FROQ_MAX_ITEMS erreicht)
    bool publish(const char *topic, bool *var, byte QoS, const char *trueState, const char *falseState);
    bool publish(const char *topic, bool *var, const char *trueState, const char *falseState);
    bool publish(const char *topic, bool *var, byte QoS);
    bool publish(const char *topic, bool *var);
    bool publish(const char *topic, float *var, byte QoS, byte decimals);
    bool publish(const char *topic, float *var, byte QoS);
    bool publish(const char *topic, float *var);

    // Broker -> Variable
    bool subscribe(const char *topic, bool *var, byte QoS, const char *trueState, const char *falseState);
    bool subscribe(const char *topic, bool *var, const char *trueState, const char *falseState);
    bool subscribe(const char *topic, bool *var, byte QoS);
    bool subscribe(const char *topic, bool *var);
    bool subscribe(const char *topic, float *var, byte QoS);
    bool subscribe(const char *topic, float *var);

    // Zyklisch aufrufen (so oft wie moeglich)
    void loop();

    /* Status der Verbindung:
       state(MqTTsessionStartup) -> Verbindung wird aufgebaut / Subscribes laufen
       state(MqTTsessionAlive)   -> Verbindung ist aktiv
       state(MqTTsessionRefresh) -> Ping laeuft (Verbindung wird aufgefrischt)
       state(MqTTsessionAbort)   -> Verbindung unterbrochen, Wiederverbindung folgt */
    bool state(uint8_t askState) const;

    // Laufzeit-Einstellungen (Sekunden)
    void setKeepAlive(uint16_t s) { _keepAlive = s; }
    void setRetry(uint16_t s)     { _retryTime = s; }
    void setRepublish(uint16_t s) { _republish = s; }
    void setCleanSession(bool c)  { _cleanSession = c; }

    bool sessionAlive = false;   // kompatibel zu V1.x

private:
    // ---- Variablen-Verknuepfung: ein Eintrag pro publish()/subscribe() ----
    struct Item {
        const char *topic;
        union { bool *b; float *f; } var;
        const char *trueState;      // nur bool
        const char *falseState;     // nur bool
        union { bool b; float f; } last;   // zuletzt gesendeter Wert
        uint32_t millis;            // Zeitpunkt letztes Senden
        uint16_t packetId;          // Packet-ID fuer QoS>0 / SUBSCRIBE
        uint8_t  flags;
        uint8_t  decimals;          // nur float-publish
    };
    enum : uint8_t {
        F_FLOAT   = 0x01,   // float statt bool
        F_SUB     = 0x02,   // subscribe statt publish
        F_QOS     = 0x0C,   // Bits 2..3
        F_PENDING = 0x10,   // wartet auf PUBACK/PUBREC/PUBCOMP bzw. SUBACK
        F_REL     = 0x20,   // QoS2: PUBREC erhalten, PUBREL gesendet
        F_SYNC    = 0x40,   // publish: sofort senden / subscribe: aktiv
        F_RETAIN  = 0x80
    };
    enum : uint8_t { S_IDLE, S_CONNECTING, S_ALIVE, S_ABORT };

    Client   *_client;
    Item      _items[FROQ_MAX_ITEMS];
    uint8_t   _itemCount = 0;

    // Broker
    const char *_url = nullptr;
    IPAddress   _ip;
    uint16_t    _port = FROQ_DEFAULT_PORT;
    const char *_clientID = "";
    String      _user, _pass;
    String      _willTopic, _willMsg;
    uint8_t     _willQoS = 0;
    bool        _willRetain = FROQ_DEFAULT_RETAIN;
    bool        _cleanSession = FROQ_DEFAULT_CLEANSESSION;
    uint16_t    _keepAlive = FROQ_DEFAULT_KEEPALIVE;
    uint16_t    _retryTime = FROQ_DEFAULT_RETRY;
    uint16_t    _republish = FROQ_DEFAULT_REPUBLISH;

    // Sitzung
    uint8_t  _state = S_IDLE;
    bool     _pingOut = false;
    uint16_t _nextId = 1;
    uint32_t _stateMillis = 0;   // Zeitpunkt des letzten Zustandswechsels
    uint32_t _lastTx = 0;        // letztes gesendetes Paket (Keep-Alive)
    uint32_t _pingMillis = 0;

    // Empfang: alle Pakete im Puffer werden nacheinander verarbeitet
    uint8_t  _rx[FROQ_RX_BUFFER];
    uint16_t _rxLen = 0;
    uint32_t _rxSkip = 0;        // Restbytes eines zu grossen Pakets verwerfen

    // Senden
    uint8_t  _tx[FROQ_TX_BUFFER];
    uint8_t  _txLen = 0;

    // intern
    bool addItem(const char *topic, void *var, uint8_t flags, byte QoS,
                 const char *t, const char *f, uint8_t decimals);
    void connect();
    void abort();
    void onConnected();
    void receive();
    void parse();
    void handlePacket(uint8_t header, const uint8_t *p, uint16_t len);
    void handlePublish(uint8_t header, const uint8_t *p, uint16_t len);
    void processItems(uint32_t now);
    uint16_t nextPacketId();

    void txByte(uint8_t b);
    void txWord(uint16_t w) { txByte(w >> 8); txByte(w & 0xFF); }
    void txBytes(const char *s, uint16_t len);
    void txUTF8(const char *s, uint16_t len) { txWord(len); txBytes(s, len); }
    void txHeader(uint8_t header, uint32_t remaining);
    void txFlush();
    void sendAck(uint8_t header, uint16_t packetId);
    void sendPublish(Item &it, bool dup);
    void sendSubscribe(Item &it);
    void sendSimple(uint8_t header);
    static uint8_t formatFloat(char *buf, float v, uint8_t decimals);
};

#endif

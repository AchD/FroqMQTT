/*
  FroqMQTT - schlanke MQTT 3.1.1 Client-Library fuer Arduino
  https://github.com/AchD/FroqMQTT
  Copyright (c) 2026 Achim Diedrichs - MIT License (siehe LICENSE)
*/
#include "Froq.h"
#include <string.h>
#include <stdlib.h>

// MQTT Control Packet Types (oberes Nibble)
#define MQ_CONNECT     0x10
#define MQ_CONNACK     0x20
#define MQ_PUBLISH     0x30
#define MQ_PUBACK      0x40
#define MQ_PUBREC      0x50
#define MQ_PUBREL      0x62   // Flags 0010 vorgeschrieben
#define MQ_PUBCOMP     0x70
#define MQ_SUBSCRIBE   0x82   // Flags 0010 vorgeschrieben
#define MQ_SUBACK      0x90
#define MQ_PINGREQ     0xC0
#define MQ_PINGRESP    0xD0
#define MQ_DISCONNECT  0xE0

#ifdef DEBUG_FROQ
  #define FROQ_DBG(...) do { Serial.print(__VA_ARGS__); } while (0)
  #define FROQ_DBGLN(...) do { Serial.println(__VA_ARGS__); } while (0)
#else
  #define FROQ_DBG(...)
  #define FROQ_DBGLN(...)
#endif

Froq::Froq(Client &mqttClient) : _client(&mqttClient) {}

// =====================================================================
//  Verbindung
// =====================================================================
void Froq::begin(IPAddress b, const char *id, uint16_t port, String user, String pwd)
{
    _ip = b; _url = nullptr; _clientID = id; _port = port;
    _user = user; _pass = pwd;
    connect();
}
void Froq::begin(IPAddress b, const char *id, String user, String pwd) { begin(b, id, FROQ_DEFAULT_PORT, user, pwd); }
void Froq::begin(IPAddress b, const char *id, uint16_t port)           { begin(b, id, port, String(), String()); }
void Froq::begin(IPAddress b, const char *id)                          { begin(b, id, FROQ_DEFAULT_PORT, String(), String()); }

void Froq::begin(const char *b, const char *id, uint16_t port, String user, String pwd)
{
    _url = b; _clientID = id; _port = port;
    _user = user; _pass = pwd;
    connect();
}
void Froq::begin(const char *b, const char *id, String user, String pwd) { begin(b, id, FROQ_DEFAULT_PORT, user, pwd); }
void Froq::begin(const char *b, const char *id, uint16_t port)           { begin(b, id, port, String(), String()); }
void Froq::begin(const char *b, const char *id)                          { begin(b, id, FROQ_DEFAULT_PORT, String(), String()); }

void Froq::end()
{
    if (_state == S_ALIVE) sendSimple(MQ_DISCONNECT);
    _client->stop();
    _state = S_IDLE;
    sessionAlive = false;
    _pingOut = false;
    _rxLen = 0; _rxSkip = 0;
}

void Froq::lastWill(String topic, String message, int QoS)
{
    _willTopic = topic;
    _willMsg = message;
    _willQoS = QoS & 0x03;
    _willRetain = FROQ_DEFAULT_RETAIN;
}
void Froq::lastWill(String topic, String message) { lastWill(topic, message, FROQ_DEFAULT_QoS); }

void Froq::connect()
{
    _rxLen = 0; _rxSkip = 0; _pingOut = false;
    _stateMillis = millis();
    int ok = _url ? _client->connect(_url, _port) : _client->connect(_ip, _port);
    if (!ok) {
        FROQ_DBGLN(F("Froq: TCP-Verbindung fehlgeschlagen"));
        _client->stop();
        _state = S_ABORT;
        sessionAlive = false;
        return;
    }
    const bool will = _willTopic.length() > 0 && _willMsg.length() > 0;
    const uint16_t idLen = strlen(_clientID);
    uint8_t flags = 0;
    if (_cleanSession) flags |= 0x02;
    if (will)          flags |= 0x04 | (_willQoS << 3) | (_willRetain ? 0x20 : 0);
    if (_user.length()) flags |= 0x80;
    if (_pass.length()) flags |= 0x40;

    uint32_t rem = 10 + 2 + idLen;
    if (will)           rem += 2 + _willTopic.length() + 2 + _willMsg.length();
    if (_user.length()) rem += 2 + _user.length();
    if (_pass.length()) rem += 2 + _pass.length();

    txHeader(MQ_CONNECT, rem);
    txUTF8("MQTT", 4);
    txByte(4);                  // Protokoll-Level 3.1.1
    txByte(flags);
    txWord(_keepAlive);
    txUTF8(_clientID, idLen);
    if (will) {
        txUTF8(_willTopic.c_str(), _willTopic.length());
        txUTF8(_willMsg.c_str(), _willMsg.length());
    }
    if (_user.length()) txUTF8(_user.c_str(), _user.length());
    if (_pass.length()) txUTF8(_pass.c_str(), _pass.length());
    txFlush();
    _state = S_CONNECTING;
}

void Froq::abort()
{
    FROQ_DBGLN(F("Froq: Verbindung abgebrochen"));
    _client->stop();
    _state = S_ABORT;
    _stateMillis = millis();
    sessionAlive = false;
    _pingOut = false;
    _rxLen = 0; _rxSkip = 0;
}

void Froq::onConnected()
{
    _state = S_ALIVE;
    _stateMillis = millis();
    sessionAlive = true;
    for (uint8_t i = 0; i < _itemCount; i++) {
        Item &it = _items[i];
        it.flags &= ~(F_PENDING | F_REL);
        if (it.flags & F_SUB) {
            it.flags &= ~F_SYNC;   // muss neu abonniert werden
            sendSubscribe(it);
        } else {
            it.flags |= F_SYNC;    // aktuellen Wert einmal senden
        }
    }
}

// =====================================================================
//  Zyklische Verarbeitung
// =====================================================================
void Froq::loop()
{
    const uint32_t now = millis();

    if (_state == S_CONNECTING || _state == S_ALIVE) {
        receive();                                   // ALLE vorhandenen Pakete verarbeiten
        if (_state != S_ABORT && !_client->connected() && !_client->available()) {
            abort();
        }
    }

    switch (_state) {
    case S_CONNECTING:
        if (now - _stateMillis > (uint32_t)FROQ_DEFAULT_STARTUP * 1000UL) abort();
        break;

    case S_ALIVE:
        processItems(now);
        if (_keepAlive) {
            const uint32_t ka = (uint32_t)_keepAlive * 1000UL;
            if (_pingOut) {
                if (now - _pingMillis > ka) abort();               // keine PINGRESP
            } else if (now - _lastTx > ka - ka / 10) {             // 90 % der Keep-Alive-Zeit
                sendSimple(MQ_PINGREQ);
                _pingOut = true;
                _pingMillis = now;
            }
        }
        break;

    case S_ABORT:
        if (now - _stateMillis > (uint32_t)_retryTime * 1000UL) connect();
        break;

    default:
        break;
    }
}

bool Froq::state(uint8_t askState) const
{
    switch (askState) {
    case MqTTsessionAlive:   return _state == S_ALIVE;
    case MqTTsessionRefresh: return _pingOut;
    case MqTTsessionAbort:   return _state == S_ABORT;
    case MqTTsessionStartup:
        if (_state == S_CONNECTING) return true;
        if (_state == S_ALIVE) {
            for (uint8_t i = 0; i < _itemCount; i++)
                if ((_items[i].flags & (F_SUB | F_PENDING)) == (F_SUB | F_PENDING)) return true;
        }
        return false;
    }
    return false;
}

// Publish-Werte pruefen und senden, offene QoS-Telegramme wiederholen
void Froq::processItems(uint32_t now)
{
    const uint32_t rep = (uint32_t)_republish * 1000UL;
    for (uint8_t i = 0; i < _itemCount && _state == S_ALIVE; i++) {
        Item &it = _items[i];

        if (it.flags & F_PENDING) {                       // wartet auf Antwort
            if (now - it.millis > rep) {
                if (it.flags & F_SUB)      sendSubscribe(it);
                else if (it.flags & F_REL) { sendAck(MQ_PUBREL, it.packetId); it.millis = now; }
                else                       sendPublish(it, true);
            }
            continue;
        }
        if (it.flags & F_SUB) continue;

        bool changed;
        if (it.flags & F_FLOAT) {
            const float v = *it.var.f;
            changed = (v != it.last.f) && !(isnan(v) && isnan(it.last.f));
        } else {
            changed = *it.var.b != it.last.b;
        }
        if ((it.flags & F_SYNC) || (changed && now - it.millis >= FROQ_MIN_MILLIS_PUBLISH)) {
            if (it.flags & F_FLOAT) it.last.f = *it.var.f; else it.last.b = *it.var.b;
            it.flags &= ~F_SYNC;
            if (it.flags & F_QOS) { it.packetId = nextPacketId(); it.flags |= F_PENDING; }
            sendPublish(it, false);
        }
    }
}

// =====================================================================
//  Empfang
// =====================================================================
void Froq::receive()
{
    int avail;
    while ((avail = _client->available()) > 0) {
        if (_rxSkip) {                                   // zu grosses Paket verwerfen
            uint32_t n = (uint32_t)avail;
            if (n > _rxSkip) n = _rxSkip;
            if (n > FROQ_RX_BUFFER) n = FROQ_RX_BUFFER;
            int r = _client->read(_rx, n);
            if (r <= 0) return;
            _rxSkip -= r;
            continue;
        }
        uint16_t room = FROQ_RX_BUFFER - _rxLen;
        if (room == 0) return;                           // kann nicht auftreten (siehe parse)
        if ((uint16_t)avail < room) room = avail;
        int r = _client->read(_rx + _rxLen, room);
        if (r <= 0) return;
        _rxLen += r;
        parse();
        if (_state == S_ABORT || _state == S_IDLE) return;
    }
}

// Zerlegt den Puffer in einzelne MQTT-Pakete. Unvollstaendige Pakete bleiben
// fuer den naechsten Durchlauf im Puffer, vollstaendige werden ALLE verarbeitet.
void Froq::parse()
{
    uint16_t pos = 0;
    while (_rxLen - pos >= 2) {
        // Remaining Length (1..4 Bytes, variable Laenge)
        uint32_t rem = 0;
        uint8_t  shift = 0;
        uint16_t i = pos + 1;
        bool complete = false;
        for (uint8_t k = 0; k < 4 && i < _rxLen; k++) {
            const uint8_t b = _rx[i++];
            rem |= (uint32_t)(b & 0x7F) << shift;
            shift += 7;
            if (!(b & 0x80)) { complete = true; break; }
        }
        if (!complete) {
            if (i - pos > 4) { abort(); return; }        // Protokollfehler
            break;                                       // Laengenfeld noch unvollstaendig
        }
        const uint16_t hdrLen = i - pos;
        const uint32_t total = hdrLen + rem;

        if (total > FROQ_RX_BUFFER) {                    // passt nie in den Puffer -> ueberspringen
            FROQ_DBG(F("Froq: Paket zu gross, verworfen: ")); FROQ_DBGLN(total);
            // PUBLISH mit QoS>0 trotzdem quittieren, sonst wiederholt der Broker endlos
            const uint8_t h = _rx[pos];
            const uint8_t q = (h >> 1) & 0x03;
            const uint16_t avail = _rxLen - i;
            if ((h & 0xF0) == MQ_PUBLISH && q && avail >= 2) {
                const uint16_t tl = (_rx[i] << 8) | _rx[i + 1];
                if (avail >= (uint32_t)tl + 4) {
                    const uint16_t id = (_rx[i + 2 + tl] << 8) | _rx[i + 3 + tl];
                    sendAck(q == 1 ? MQ_PUBACK : MQ_PUBREC, id);
                }
            }
            _rxSkip = total - (_rxLen - pos);
            pos = _rxLen;
            break;
        }
        if ((uint32_t)(_rxLen - pos) < total) break;                // Rest kommt im naechsten TCP-Segment

        handlePacket(_rx[pos], _rx + pos + hdrLen, (uint16_t)rem);
        if (_state == S_ABORT || _state == S_IDLE) return;   // Puffer wurde zurueckgesetzt
        pos += total;
    }
    if (pos) {
        _rxLen -= pos;
        if (_rxLen) memmove(_rx, _rx + pos, _rxLen);
    }
}

void Froq::handlePacket(uint8_t header, const uint8_t *p, uint16_t len)
{
    const uint16_t pid = len >= 2 ? (uint16_t)((p[0] << 8) | p[1]) : 0;
    FROQ_DBG(F("Froq <- 0x")); FROQ_DBG(header, HEX); FROQ_DBG(F(" len ")); FROQ_DBGLN(len);

    switch (header & 0xF0) {
    case MQ_CONNACK:
        if (len >= 2 && p[1] == 0) onConnected();
        else { FROQ_DBGLN(F("Froq: Verbindung abgelehnt")); abort(); }
        break;

    case MQ_PUBLISH:
        handlePublish(header, p, len);
        break;

    case MQ_PUBACK:
    case MQ_PUBCOMP:
        for (uint8_t i = 0; i < _itemCount; i++) {
            Item &it = _items[i];
            if ((it.flags & (F_SUB | F_PENDING)) == F_PENDING && it.packetId == pid)
                it.flags &= ~(F_PENDING | F_REL);
        }
        break;

    case MQ_PUBREC:
        for (uint8_t i = 0; i < _itemCount; i++) {
            Item &it = _items[i];
            if ((it.flags & (F_SUB | F_PENDING)) == F_PENDING && it.packetId == pid) {
                it.flags |= F_REL;
                it.millis = millis();
            }
        }
        sendAck(MQ_PUBREL, pid);
        break;

    case (MQ_PUBREL & 0xF0):
        sendAck(MQ_PUBCOMP, pid);
        break;

    case MQ_SUBACK:
        for (uint8_t i = 0; i < _itemCount; i++) {
            Item &it = _items[i];
            if ((it.flags & (F_SUB | F_PENDING)) == (F_SUB | F_PENDING) && it.packetId == pid) {
                it.flags &= ~F_PENDING;
                if (len >= 3 && p[2] < 0x80) {
                    it.flags |= F_SYNC;                          // aktiv
                    uint8_t q = p[2] & 0x03;                     // vom Broker gewaehrter QoS
                    if (q < ((it.flags & F_QOS) >> 2)) it.flags = (it.flags & ~F_QOS) | (q << 2);
                }
            }
        }
        break;

    case MQ_PINGRESP:
        _pingOut = false;
        break;

    default:
        break;   // unbekannte Pakete ignorieren
    }
}

void Froq::handlePublish(uint8_t header, const uint8_t *p, uint16_t len)
{
    const uint8_t qos = (header >> 1) & 0x03;
    if (len < 2) return;
    const uint16_t tLen = (p[0] << 8) | p[1];
    uint16_t i = 2 + tLen;
    if (i > len) return;
    const char *topic = (const char *)(p + 2);
    uint16_t pid = 0;
    if (qos) {
        if (i + 2 > len) return;
        pid = (p[i] << 8) | p[i + 1];
        i += 2;
    }
    const char *payload = (const char *)(p + i);
    const uint16_t pLen = len - i;

    for (uint8_t n = 0; n < _itemCount; n++) {
        Item &it = _items[n];
        if (!(it.flags & F_SUB)) continue;
        if (strlen(it.topic) != tLen || memcmp(it.topic, topic, tLen) != 0) continue;

        if (it.flags & F_FLOAT) {
            char buf[FROQ_PAYLOAD_MAX + 1];
            uint16_t c = pLen > FROQ_PAYLOAD_MAX ? FROQ_PAYLOAD_MAX : pLen;
            memcpy(buf, payload, c);
            buf[c] = 0;
            *it.var.f = (float)atof(buf);
        } else {
            if (strlen(it.trueState) == pLen && memcmp(it.trueState, payload, pLen) == 0)        *it.var.b = true;
            else if (strlen(it.falseState) == pLen && memcmp(it.falseState, payload, pLen) == 0) *it.var.b = false;
        }
        // kein break: mehrere Variablen duerfen dasselbe Topic abonnieren
    }
    // Bestaetigung immer senden, auch fuer unbekannte Topics
    if (qos == 1) sendAck(MQ_PUBACK, pid);
    else if (qos == 2) sendAck(MQ_PUBREC, pid);
}

// =====================================================================
//  Senden
// =====================================================================
void Froq::txByte(uint8_t b)
{
    _tx[_txLen++] = b;
    if (_txLen >= FROQ_TX_BUFFER) txFlush();
}
void Froq::txBytes(const char *s, uint16_t len)
{
    while (len--) txByte((uint8_t)*s++);
}
void Froq::txHeader(uint8_t header, uint32_t remaining)
{
    txByte(header);
    do {
        uint8_t b = remaining & 0x7F;
        remaining >>= 7;
        if (remaining) b |= 0x80;
        txByte(b);
    } while (remaining);
}
void Froq::txFlush()
{
    if (_txLen) {
        _client->write(_tx, _txLen);   // ein Aufruf statt Byte fuer Byte
        _txLen = 0;
    }
    _lastTx = millis();
}

void Froq::sendSimple(uint8_t header)
{
    txByte(header);
    txByte(0);
    txFlush();
}

void Froq::sendAck(uint8_t header, uint16_t packetId)
{
    txByte(header);
    txByte(2);
    txWord(packetId);
    txFlush();
}

void Froq::sendSubscribe(Item &it)
{
    it.packetId = nextPacketId();
    it.flags |= F_PENDING;
    it.millis = millis();
    const uint16_t tLen = strlen(it.topic);
    txHeader(MQ_SUBSCRIBE, 2 + 2 + tLen + 1);
    txWord(it.packetId);
    txUTF8(it.topic, tLen);
    txByte((it.flags & F_QOS) >> 2);
    txFlush();
}

void Froq::sendPublish(Item &it, bool dup)
{
    char num[20];
    const char *payload;
    uint16_t pLen;
    if (it.flags & F_FLOAT) {
        pLen = formatFloat(num, it.last.f, it.decimals);
        payload = num;
    } else {
        payload = it.last.b ? it.trueState : it.falseState;
        pLen = strlen(payload);
    }
    const uint8_t qos = (it.flags & F_QOS) >> 2;
    const uint16_t tLen = strlen(it.topic);
    uint8_t header = MQ_PUBLISH | (qos << 1);
    if (dup && qos)            header |= 0x08;
    if (it.flags & F_RETAIN)   header |= 0x01;

    txHeader(header, 2 + tLen + (qos ? 2 : 0) + pLen);
    txUTF8(it.topic, tLen);
    if (qos) txWord(it.packetId);
    txBytes(payload, pLen);
    txFlush();
    it.millis = millis();
}

uint16_t Froq::nextPacketId()
{
    if (_nextId == 0) _nextId = 1;
    return _nextId++;
}

// Float -> Text ohne String/Heap (z.B. 12.35, -0.5, nan)
uint8_t Froq::formatFloat(char *buf, float v, uint8_t decimals)
{
    if (isnan(v)) { strcpy(buf, "nan"); return 3; }
    if (isinf(v)) { strcpy(buf, v < 0 ? "-inf" : "inf"); return v < 0 ? 4 : 3; }
    if (decimals > 6) decimals = 6;
    char *p = buf;
    if (v < 0) { *p++ = '-'; v = -v; }
    float rounding = 0.5f;
    for (uint8_t d = 0; d < decimals; d++) rounding /= 10.0f;
    v += rounding;
    if (v >= 4294967040.0f) { strcpy(buf, "ovf"); return 3; }
    uint32_t ip = (uint32_t)v;
    float frac = v - (float)ip;
    char tmp[11];
    uint8_t n = 0;
    do { tmp[n++] = '0' + ip % 10; ip /= 10; } while (ip);
    while (n) *p++ = tmp[--n];
    if (decimals) {
        *p++ = '.';
        while (decimals--) {
            frac *= 10.0f;
            uint8_t d = (uint8_t)frac;
            if (d > 9) d = 9;
            *p++ = '0' + d;
            frac -= d;
        }
    }
    *p = 0;
    return p - buf;
}

// =====================================================================
//  Variablen verknuepfen
// =====================================================================
bool Froq::addItem(const char *topic, void *var, uint8_t flags, byte QoS,
                   const char *t, const char *f, uint8_t decimals)
{
    if (_itemCount >= FROQ_MAX_ITEMS || !topic || !var) return false;
    Item &it = _items[_itemCount++];
    it.topic = topic;
    if (flags & F_FLOAT) { it.var.f = (float *)var; it.last.f = *it.var.f; }
    else                 { it.var.b = (bool *)var;  it.last.b = *it.var.b; }
    it.trueState = t;
    it.falseState = f;
    it.millis = 0;
    it.packetId = 0;
    it.decimals = decimals;
    it.flags = flags | ((QoS & 0x03) << 2);
    if (!(flags & F_SUB)) {
        if (FROQ_DEFAULT_RETAIN) it.flags |= F_RETAIN;
        it.flags |= F_SYNC;
    }
    // Bereits verbunden? Dann sofort abonnieren
    if ((flags & F_SUB) && _state == S_ALIVE) sendSubscribe(it);
    return true;
}

bool Froq::publish(const char *topic, bool *var, byte QoS, const char *t, const char *f)
{ return addItem(topic, var, 0, QoS, t, f, 0); }
bool Froq::publish(const char *topic, bool *var, const char *t, const char *f)
{ return publish(topic, var, FROQ_DEFAULT_QoS, t, f); }
bool Froq::publish(const char *topic, bool *var, byte QoS)
{ return publish(topic, var, QoS, FROQ_DEFAULT_BOOL_STATE_TRUE, FROQ_DEFAULT_BOOL_STATE_FALSE); }
bool Froq::publish(const char *topic, bool *var)
{ return publish(topic, var, FROQ_DEFAULT_QoS); }
bool Froq::publish(const char *topic, float *var, byte QoS, byte decimals)
{ return addItem(topic, var, F_FLOAT, QoS, nullptr, nullptr, decimals); }
bool Froq::publish(const char *topic, float *var, byte QoS)
{ return publish(topic, var, QoS, FROQ_DEFAULT_DECIMAL_PLACES); }
bool Froq::publish(const char *topic, float *var)
{ return publish(topic, var, FROQ_DEFAULT_QoS); }

bool Froq::subscribe(const char *topic, bool *var, byte QoS, const char *t, const char *f)
{ return addItem(topic, var, F_SUB, QoS, t, f, 0); }
bool Froq::subscribe(const char *topic, bool *var, const char *t, const char *f)
{ return subscribe(topic, var, FROQ_DEFAULT_QoS, t, f); }
bool Froq::subscribe(const char *topic, bool *var, byte QoS)
{ return subscribe(topic, var, QoS, FROQ_DEFAULT_BOOL_STATE_TRUE, FROQ_DEFAULT_BOOL_STATE_FALSE); }
bool Froq::subscribe(const char *topic, bool *var)
{ return subscribe(topic, var, FROQ_DEFAULT_QoS); }
bool Froq::subscribe(const char *topic, float *var, byte QoS)
{ return addItem(topic, var, F_FLOAT | F_SUB, QoS, nullptr, nullptr, 0); }
bool Froq::subscribe(const char *topic, float *var)
{ return subscribe(topic, var, FROQ_DEFAULT_QoS); }

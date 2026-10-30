// Host-Test fuer Froq mit simuliertem Broker
#include "Froq.h"
#include <deque>
#include <vector>
#include <cassert>
#include <iostream>

uint32_t g_millis = 0;
SerialStub Serial;

typedef std::vector<uint8_t> Bytes;

struct Pkt { uint8_t hdr; Bytes body; };

class MockClient : public Client {
public:
    std::deque<Bytes> segments;   // jedes Element = ein TCP-Segment
    Bytes out;
    bool isConnected = false;
    int writeCalls = 0;
    int connect(IPAddress, uint16_t) override { isConnected = true; return 1; }
    int connect(const char*, uint16_t) override { isConnected = true; return 1; }
    size_t write(const uint8_t *b, size_t n) override { writeCalls++; out.insert(out.end(), b, b + n); return n; }
    int available() override { return segments.empty() ? 0 : (int)segments.front().size(); }
    int read(uint8_t *b, size_t n) override {
        if (segments.empty()) return -1;
        Bytes &s = segments.front();
        size_t c = std::min(n, s.size());
        memcpy(b, s.data(), c);
        s.erase(s.begin(), s.begin() + c);
        if (s.empty()) segments.pop_front();
        return (int)c;
    }
    uint8_t connected() override { return isConnected; }
    void stop() override { isConnected = false; }

    // gesendete Pakete dekodieren
    std::vector<Pkt> takePackets() {
        std::vector<Pkt> r; size_t i = 0;
        while (i < out.size()) {
            Pkt p; p.hdr = out[i++];
            uint32_t len = 0, sh = 0; uint8_t b;
            do { b = out[i++]; len |= (b & 0x7F) << sh; sh += 7; } while (b & 0x80);
            p.body.assign(out.begin() + i, out.begin() + i + len); i += len;
            r.push_back(p);
        }
        out.clear();
        return r;
    }
};

static void enc(Bytes &o, uint8_t hdr, const Bytes &body) {
    o.push_back(hdr); uint32_t l = body.size();
    do { uint8_t b = l & 0x7F; l >>= 7; if (l) b |= 0x80; o.push_back(b); } while (l);
    o.insert(o.end(), body.begin(), body.end());
}
static Bytes publishPkt(const std::string &t, const std::string &pl, uint8_t qos = 0, uint16_t pid = 0) {
    Bytes body; body.push_back(t.size() >> 8); body.push_back(t.size() & 0xFF);
    body.insert(body.end(), t.begin(), t.end());
    if (qos) { body.push_back(pid >> 8); body.push_back(pid & 0xFF); }
    body.insert(body.end(), pl.begin(), pl.end());
    Bytes o; enc(o, 0x30 | (qos << 1), body); return o;
}
static Bytes ack(uint8_t hdr, uint16_t pid) { Bytes o; enc(o, hdr, {(uint8_t)(pid >> 8), (uint8_t)pid}); return o; }
static uint16_t pidOf(const Pkt &p) { return (p.body[0] << 8) | p.body[1]; }
static std::string topicOf(const Pkt &p) { uint16_t l = (p.body[0] << 8) | p.body[1]; return std::string(p.body.begin() + 2, p.body.begin() + 2 + l); }
static std::string payloadOf(const Pkt &p) {
    uint16_t l = (p.body[0] << 8) | p.body[1]; size_t i = 2 + l; if ((p.hdr >> 1) & 3) i += 2;
    return std::string(p.body.begin() + i, p.body.end());
}
static Bytes cat(std::initializer_list<Bytes> l) { Bytes r; for (auto &b : l) r.insert(r.end(), b.begin(), b.end()); return r; }

#define CHECK(c) do { if (!(c)) { std::cerr << "FEHLER Zeile " << __LINE__ << ": " #c "\n"; return 1; } } while (0)

int main() {
    MockClient net;
    Froq mq(net);
    bool licht = false, taste = false, taste2 = false;
    float zahl = 1.5f, soll = 0;

    mq.lastWill("client/died", "bye", 0);
    CHECK(mq.publish("test/licht/oben", &licht, 2, "EIN", "AUS"));
    CHECK(mq.publish("test/zahl", &zahl, 1));
    CHECK(mq.subscribe("test/sollwert", &soll, 1));
    CHECK(mq.subscribe("test/taste", &taste, 2, "EIN", "AUS"));
    CHECK(mq.subscribe("test/taste2", &taste2));

    g_millis = 1000;
    mq.begin(IPAddress(192,168,1,10), "FroQClient", "didicl", "pw");
    auto pk = net.takePackets();
    CHECK(pk.size() == 1 && pk[0].hdr == 0x10);
    CHECK(pk[0].body[7] == 0xC4);   // user+pass+will, cleanSession=false
    CHECK(mq.state(MqTTsessionStartup));

    // CONNACK alleine
    net.segments.push_back({0x20, 0x02, 0x00, 0x00});
    mq.loop();
    CHECK(mq.state(MqTTsessionAlive));
    pk = net.takePackets();
    // 3 SUBSCRIBE + 2 PUBLISH (Sync)
    int subs = 0, pubs = 0; uint16_t subPid[3]; uint16_t pubPid[2];
    for (auto &p : pk) {
        if (p.hdr == 0x82) subPid[subs++] = pidOf(p);
        if ((p.hdr & 0xF0) == 0x30) { pubPid[pubs++] = (p.body[2 + ((p.body[0]<<8)|p.body[1])] << 8) | p.body[3 + ((p.body[0]<<8)|p.body[1])]; }
    }
    CHECK(subs == 3 && pubs == 2);
    CHECK(payloadOf(pk[3]) == "AUS" || payloadOf(pk[4]) == "AUS");
    CHECK(mq.state(MqTTsessionStartup));   // SUBACKs fehlen noch

    // ===== Kernproblem: viele Telegramme in EINEM TCP-Segment =====
    std::string longPayload(200, 'x');      // Remaining Length > 127 -> 2 Byte Laengenfeld
    Bytes seg = cat({
        Bytes{0x90, 0x03, (uint8_t)(subPid[0] >> 8), (uint8_t)subPid[0], 0x01},
        Bytes{0x90, 0x03, (uint8_t)(subPid[1] >> 8), (uint8_t)subPid[1], 0x02},
        Bytes{0x90, 0x03, (uint8_t)(subPid[2] >> 8), (uint8_t)subPid[2], 0x00},
        publishPkt("test/sollwert", "21.5", 1, 77),          // retained, QoS1
        publishPkt("test/taste", "EIN", 2, 78),              // QoS2
        publishPkt("test/unbekannt", longPayload, 1, 79),    // langes Paket, fremdes Topic
        publishPkt("test/taste2", "ON"),
        ack(0x50, pubPid[0]),   // PUBREC fuer licht (QoS2)
        ack(0x40, pubPid[1]),   // PUBACK fuer zahl (QoS1)
    });
    net.segments.push_back(seg);
    int callsBefore = net.writeCalls;
    mq.loop();
    CHECK(soll == 21.5f);
    CHECK(taste == true);
    CHECK(taste2 == true);
    CHECK(!mq.state(MqTTsessionStartup));
    pk = net.takePackets();
    // erwartet: PUBACK 77, PUBREC 78, PUBACK 79, PUBREL fuer licht
    int puback = 0, pubrec = 0, pubrel = 0;
    for (auto &p : pk) {
        if (p.hdr == 0x40) puback++;
        if (p.hdr == 0x50) { pubrec++; CHECK(pidOf(p) == 78); }
        if (p.hdr == 0x62) { pubrel++; CHECK(pidOf(p) == pubPid[0]); }
    }
    CHECK(puback == 2 && pubrec == 1 && pubrel == 1);
    std::cout << "OK  9 Telegramme in einem TCP-Segment verarbeitet (" << (net.writeCalls - callsBefore) << " write-Aufrufe)\n";

    // ===== Telegramme ueber Segmentgrenzen verteilt =====
    Bytes two = cat({ publishPkt("test/sollwert", "-3.25"), Bytes{0x62, 0x02, 0x00, 78}, publishPkt("test/taste", "AUS") });
    // Byte-weise zerlegt: schlimmster Fall
    for (uint8_t b : two) net.segments.push_back({b});
    for (int i = 0; i < 40 && !net.segments.empty(); i++) mq.loop();
    CHECK(soll == -3.25f);
    CHECK(taste == false);
    pk = net.takePackets();
    CHECK(pk.size() == 1 && pk[0].hdr == 0x70 && pidOf(pk[0]) == 78);  // PUBCOMP
    std::cout << "OK  Telegramme byteweise verteilt korrekt zusammengesetzt\n";

    // ===== Zu grosses Paket (> FROQ_RX_BUFFER) wird uebersprungen, danach geht es weiter =====
    std::string huge(FROQ_RX_BUFFER + 300, 'y');
    Bytes h = cat({ publishPkt("test/riesig", huge), publishPkt("test/sollwert", "42") });
    // in 3 Segmente geteilt
    size_t a = h.size() / 3;
    net.segments.push_back(Bytes(h.begin(), h.begin() + a));
    net.segments.push_back(Bytes(h.begin() + a, h.begin() + 2 * a));
    net.segments.push_back(Bytes(h.begin() + 2 * a, h.end()));
    for (int i = 0; i < 10; i++) mq.loop();
    CHECK(soll == 42.0f);
    CHECK(mq.state(MqTTsessionAlive));
    std::cout << "OK  Uebergrosses Paket verworfen, Folgetelegramm verarbeitet\n";

    // ===== PUBCOMP fuer licht, dann Wertaenderung -> Publish =====
    net.segments.push_back(ack(0x70, pubPid[0]));
    mq.loop(); net.takePackets();
    licht = true; zahl = 12.345f;
    g_millis += 3001;
    mq.loop();
    pk = net.takePackets();
    CHECK(pk.size() == 2);
    CHECK(topicOf(pk[0]) == "test/licht/oben" && payloadOf(pk[0]) == "EIN" && ((pk[0].hdr >> 1) & 3) == 2);
    CHECK(topicOf(pk[1]) == "test/zahl" && payloadOf(pk[1]) == "12.35");
    // keine Antwort -> nach Republish-Zeit mit DUP erneut
    g_millis += FROQ_DEFAULT_REPUBLISH * 1000 + 1;
    mq.loop();
    pk = net.takePackets();
    bool sawDup = false;
    for (auto &p : pk) if ((p.hdr & 0xF8) == 0x38 && payloadOf(p).size()) sawDup = true;
    CHECK(sawDup);
    // Broker bestaetigt: QoS1 -> PUBACK, QoS2 -> PUBREC ... PUBCOMP
    for (auto &p : pk) {
        uint16_t l = (p.body[0] << 8) | p.body[1];
        uint16_t id = (p.body[2 + l] << 8) | p.body[3 + l];
        if (((p.hdr >> 1) & 3) == 1) net.segments.push_back(ack(0x40, id));
        else net.segments.push_back(cat({ack(0x50, id), ack(0x70, id)}));
    }
    mq.loop(); net.takePackets();
    std::cout << "OK  Publish bei Wertaenderung + Wiederholung mit DUP inkl. Nutzlast\n";

    // ===== Keep-Alive =====
    g_millis += FROQ_DEFAULT_KEEPALIVE * 1000;
    mq.loop();
    pk = net.takePackets();
    bool ping = false; for (auto &p : pk) if (p.hdr == 0xC0) ping = true;
    CHECK(ping && mq.state(MqTTsessionRefresh));
    net.segments.push_back({0xD0, 0x00});
    mq.loop();
    CHECK(!mq.state(MqTTsessionRefresh));
    std::cout << "OK  Ping / Pingresp\n";

    // ===== Verbindungsabbruch + Wiederverbindung =====
    net.isConnected = false;
    mq.loop();
    CHECK(mq.state(MqTTsessionAbort));
    g_millis += FROQ_DEFAULT_RETRY * 1000 + 1;
    mq.loop();
    CHECK(mq.state(MqTTsessionStartup));
    pk = net.takePackets();
    CHECK(pk.size() == 1 && pk[0].hdr == 0x10);
    std::cout << "OK  Wiederverbindung nach Abbruch\n";

    std::cout << "sizeof(Froq) Host-64bit: " << sizeof(Froq) << " Byte\n";
    std::cout << "ALLE TESTS BESTANDEN\n";
    return 0;
}

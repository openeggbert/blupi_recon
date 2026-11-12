// Small native session contract. No game layouts or platform compatibility types.
#ifndef BLUPI_NET_SESSION_H
#define BLUPI_NET_SESSION_H
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace blupi::net {
struct Endpoint { std::string host = "127.0.0.1"; uint16_t port = 28640; };
bool ParseEndpoint(const std::string&, Endpoint&);
struct Options {
    uint16_t port = 28640;
    std::vector<Endpoint> endpoints;
    bool testSync = false;
    // The Free Direct server provider (cnetwork.cpp): the room file, if set, prefills the room field
    // with the room it holds and receives the ID of a room this instance creates (tests, scripts).
    std::string roomFile;
};
void Configure(const Options&);
const Options& Configuration();
// A session found on the LAN, at an address, or in a Free Direct server's room (relay: instance is
// its id there; endpoint is unused).
struct Advertisement {
    Endpoint endpoint; std::string name; uint32_t epoch = 0;
    bool relay = false; std::array<uint8_t,16> instance{};
};
class Relay;
struct Message {
    uint32_t origin = 0, sequence = 0, tick = 0;
    bool reliable = false;
    std::vector<uint8_t> bytes;
};
struct Departure { uint32_t member = 0; bool hostLost = false; };
class Session {
public:
    Session(uint32_t protocol, uint32_t version);
    ~Session();
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    // Peers must agree on the game content (e.g. edition and multiplayer maps): set before Create/Join/Probe.
    void SetContent(uint64_t fingerprint);
    bool Create(uint16_t port, const std::string& name, const std::string& player);
    bool Join(const Advertisement&, const std::string& player, unsigned deadlineMs = 1500);
    std::vector<Advertisement> Probe(const std::vector<Endpoint>&, unsigned deadlineMs = 200);
    // LAN discovery: sessions answering on game port + 1; counts incompatible (other content) answers.
    std::vector<Advertisement> Discover(uint16_t port, unsigned deadlineMs = 300, unsigned* incompatible = nullptr);
    // The Free Direct server (net/relay.h, connected and in a room): with a relay, Create hosts the
    // session in the room, List lists the room's sessions and Join joins one of them; every message
    // travels through the server. nullptr returns to ENet (UDP). Set it before Create/Join/List.
    void UseRelay(Relay*);
    std::vector<Advertisement> List();
    void Service(unsigned waitMs = 0);
    bool Send(const void*, size_t, bool reliable);
    bool Receive(Message&);
    bool Departed(Departure&);
    void Start();
    void Close(unsigned deadlineMs = 100);
    bool Active() const;
    bool IsHost() const;
    uint32_t Id() const;
    bool Knows(uint32_t) const;
    const std::string& Error() const;
private:
    struct Impl;
    std::unique_ptr<Impl> p;
    friend void ServiceAll();
    friend bool BeginTestTick(long);
    friend bool EndTestTick(long);
};
void ServiceAll();
// Test supervisor uses BLUPI_NET_TEST_DIR and BLUPI_NET_TEST_ROLE (0/1).
// Each grant contains the previous tick's two send manifests. All waits are bounded.
bool BeginTestTick(long tick);
bool EndTestTick(long tick);
} // namespace blupi::net
#endif

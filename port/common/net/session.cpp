#include "net/session.h"
#include "net/relay.h"
#include <enet/enet.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <map>
#include <random>
#include <thread>

namespace blupi::net {
namespace {
using Clock = std::chrono::steady_clock;
constexpr uint32_t Magic = 0x4e455431, DiscoveryQuery = 0x51445442, DiscoveryAnswer = 0x41445442;
enum Kind { Hello = 1, Welcome, Refuse, Members, Data, Query, Answer, Gone };
// Envelope: magic, protocol, version, kind, epoch, origin, sequence, tick, length, content (64-bit).
constexpr size_t Header = 44, MaxQueue = 1024;
// The relay payload's first byte: the sender's reliability choice, and whether the server delivered
// the message to every member itself (route 2: no host relays it).
constexpr uint8_t RelayReliable = 1, RelayFannedOut = 2;
// The game's tick (g_timerInterval), for the link diagnostics only: a message's delay is measured
// against the tick its sender stamped on it.
constexpr double TickMs = 50;
constexpr enet_uint32 kPeerTimeoutLimit = 32, kPeerTimeoutMinMs = 5000, kPeerTimeoutMaxMs = 10000;
Options options;
uint32_t currentTick = 0;
uint32_t testSent = 0;
std::vector<Session*> sessions;
bool Init() { static bool ok = enet_initialize() == 0; return ok; }
void Put(std::vector<uint8_t>& b, size_t at, uint32_t n) {
    for (int i = 0; i < 4; ++i) b[at+i] = uint8_t(n >> (8*i));
}
uint32_t Get(const uint8_t* b, size_t at) {
    uint32_t n = 0; for (int i = 0; i < 4; ++i) n |= uint32_t(b[at+i]) << (8*i); return n;
}
// ENetBuffer's field order differs by platform (Windows: WSABUF), so it is filled by name.
ENetBuffer Buffer(void* data, size_t size) { ENetBuffer b; b.data = data; b.dataLength = size; return b; }
uint64_t Content(const uint8_t* b) { return uint64_t(Get(b,36)) | uint64_t(Get(b,40)) << 32; }
bool Text(const uint8_t* b, size_t n) { return n > 0 && n <= 100 && b[n-1] == 0 && std::memchr(b, 0, n) == b+n-1; }
}
bool ParseEndpoint(const std::string& value, Endpoint& out) {
    auto colon = value.find(':');
    if (value.empty() || (colon != std::string::npos && value.find(':', colon+1) != std::string::npos)) return false;
    out.host = value.substr(0, colon);
    if (out.host.empty()) return false;
    if (colon != std::string::npos) {
        const std::string port = value.substr(colon+1);
        if (port.empty() || port.find_first_not_of("0123456789") != std::string::npos) return false;
        unsigned long n = std::strtoul(port.c_str(), nullptr, 10);
        if (n == 0 || n > 65535) return false;
        out.port = uint16_t(n);
    }
    return true;
}
void Configure(const Options& o) { options = o; }
const Options& Configuration() { return options; }

struct Session::Impl {
    uint32_t protocol, version, epoch = 0, id = 0, nextId = 2, sequence = 0;
    uint64_t content = 0;
    ENetSocket discovery = ENET_SOCKET_NULL;   // host: LAN discovery answers, game port + 1
    uint16_t port = 0;
    ENetHost* socket = nullptr;
    ENetPeer* server = nullptr;
    // The Free Direct server (net/relay.h): set by UseRelay; every peer is then a connection of the
    // server, and relayHost the host's connection for a member.
    Relay* relay = nullptr;
    uint32_t relayHost = 0;
    bool host = false, active = false, started = false;
    std::string name, player, error;
    std::array<uint32_t,4> members{};
    // The Free Direct server: each member's connection there (Members), so that a member takes the
    // others' messages straight from the server once the game has started (route 2).
    std::array<uint32_t,4> connections{};
    struct Peer { uint32_t id = 0; unsigned count = 0; Clock::time_point since = Clock::now(); };
    // Peers by handle: an ENetPeer*, or a relay connection ID.
    std::map<uintptr_t,Peer> peers;
    static uintptr_t Handle(ENetPeer* peer) { return reinterpret_cast<uintptr_t>(peer); }
    uintptr_t Server() const { return relay ? uintptr_t(relayHost) : Handle(server); }
    std::deque<Message> incoming;
    std::deque<Departure> departures;
    std::map<uint32_t,uint32_t> received;
    uint32_t released[4] = {0,0,0,0};   // test lockstep: released sequence per origin id 1..4
    bool synced = false;                // joiner: membership counts applied once

    // Link diagnostics: how late each origin's messages arrive in play, against the ticks its sender
    // stamped on them. A message is late by how much more its arrival trails its tick than the
    // earliest arrival of the last second did; a player who stands still sends nothing and is not
    // late. Late messages are what a stalled connection delivers in a burst, and over 250 ms (the
    // game's 5 ticks of prediction, CDecor::DoNetSmooth) another player's Blupi stops and jumps.
    struct Link {
        std::deque<std::pair<double,double>> recent;   // arrival, arrival minus tick (ms)
        unsigned messages = 0, late150 = 0, late250 = 0, reported = 0;
        double worst = 0;
    };
    std::map<uint32_t,Link> links;
    Clock::time_point lastReport = Clock::now();
    void Arrived(uint32_t origin, uint32_t tick) {
        if (options.testSync || !started) return;
        const double now = std::chrono::duration<double,std::milli>(Clock::now().time_since_epoch()).count();
        const double offset = now - tick * TickMs;
        Link& link = links[origin];
        while (!link.recent.empty() && now - link.recent.front().first > 1000) link.recent.pop_front();
        double base = offset;
        for (const auto& r : link.recent) base = std::min(base, r.second);
        link.recent.push_back({now, offset});
        const double late = offset - base;
        ++link.messages;
        if (late > 150) ++link.late150;
        if (late > 250) ++link.late250;
        link.worst = std::max(link.worst, late);
    }
    void Report(bool last) {
        if (!last && Clock::now() - lastReport < std::chrono::seconds(30)) return;
        lastReport = Clock::now();
        for (auto& [origin, link] : links) {
            if (link.late150 == link.reported && !(last && link.messages)) continue;
            std::fprintf(stderr, "network: link from player %u%s: %u messages, %u over 150 ms late, "
                         "%u over 250 ms, the latest %.0f ms\n", origin, relay ? " (server)" : "",
                         link.messages, link.late150, link.late250, link.worst);
            link.reported = link.late150;
        }
    }

    std::vector<uint8_t> Packet(Kind kind, uint32_t origin, uint32_t seq, const void* body, size_t n) {
        std::vector<uint8_t> b(Header+n);
        Put(b,0,Magic); Put(b,4,protocol); Put(b,8,version); Put(b,12,kind);
        Put(b,16,epoch); Put(b,20,origin); Put(b,24,seq); Put(b,28,currentTick); Put(b,32,uint32_t(n));
        Put(b,36,uint32_t(content)); Put(b,40,uint32_t(content >> 32));
        if (n) std::memcpy(b.data()+Header,body,n);
        return b;
    }
    bool Write(uintptr_t to, const std::vector<uint8_t>& b, bool reliable = true) {
        if (relay) {
            // The server delivers everything reliably and in order; the first byte keeps the sender's
            // choice for the receiver's checks. A host writes to one member, a member to its host.
            std::vector<uint8_t> payload; payload.reserve(b.size()+1);
            payload.push_back(reliable ? 1 : 0); payload.insert(payload.end(),b.begin(),b.end());
            return host ? relay->Data(1,uint32_t(to),payload) : relay->Data(0,0,payload);
        }
        ENetPeer* peer=reinterpret_cast<ENetPeer*>(to);
        ENetPacket* packet = enet_packet_create(b.data(), b.size(), reliable ? ENET_PACKET_FLAG_RELIABLE : ENET_PACKET_FLAG_UNSEQUENCED);
        if (!packet) return false;
        if (enet_peer_send(peer, reliable ? 0 : 1, packet) < 0) { enet_packet_destroy(packet); return false; }
        return true;
    }
    void Broadcast(const std::vector<uint8_t>& b, bool reliable = true, uintptr_t except = 0) {
        for (auto& [peer, state] : peers) if (state.id && peer != except && !Write(peer,b,reliable)) Fail("reliable enqueue failed");
    }
    // A game message in play through the Free Direct server: the server gives it to every other
    // member at once (route 2), so a member's message no longer goes through the host and back,
    // two crossings of the Internet instead of four. Before the start the host relays as before:
    // who is in the session can still change, and every member must see the lobby in one order.
    bool FanOut(const std::vector<uint8_t>& b, bool reliable) {
        std::vector<uint8_t> payload; payload.reserve(b.size()+1);
        payload.push_back(uint8_t((reliable ? RelayReliable : 0) | RelayFannedOut));
        payload.insert(payload.end(),b.begin(),b.end());
        return relay->Data(2,0,payload);
    }
    void Fail(const char* why) {
        if (active) departures.push_back({1,true});
        active = false; error = why;
    }
    void Membership() {
        // Member ids, then how many data messages each has sent so far (as the host knows it): a new
        // member never receives the earlier ones, so the test lockstep must not wait for them.
        std::vector<uint8_t> b(48);
        for (unsigned i = 0; i < 4; ++i) {
            Put(b,4*i,members[i]);
            Put(b,16+4*i,members[i] == id ? sequence : members[i] ? received[members[i]] : 0);
            // Their connections on the Free Direct server (0: the host's own, or no server).
            for (const auto& [peer, state] : peers)
                if (relay && members[i] && state.id == members[i]) Put(b,32+4*i,uint32_t(peer));
        }
        Broadcast(Packet(Members,1,0,b.data(),b.size()));
    }
    void Reject(uintptr_t peer, const char* why) {
        Write(peer,Packet(Refuse,1,0,why,std::strlen(why)+1));
        if (relay) relay->Refuse(uint32_t(peer));
        else enet_peer_disconnect_later(reinterpret_cast<ENetPeer*>(peer),0);
    }
    // A member is gone (its connection closed or the server says it left): its id is freed.
    void MemberLeft(uintptr_t peer) {
        auto it=peers.find(peer);
        if (it==peers.end()) return;
        uint32_t gone=it->second.id;
        peers.erase(it);
        if (!gone) return;
        departures.push_back({gone,false});
        std::vector<uint8_t> body(4); Put(body,0,gone);
        Broadcast(Packet(Gone,1,0,body.data(),body.size()));
        for (auto& member:members) if (member==gone) member=0;
        Membership();
    }
    void Accept(uintptr_t peer, const uint8_t* b, size_t n, bool reliable, bool fannedOut = false) {
        if (n < Header || Get(b,0) != Magic || Get(b,32) != n-Header) return;
        const auto kind = Kind(Get(b,12));
        const uint32_t origin = Get(b,20), seq = Get(b,24);
        const uint8_t* body = b+Header; const size_t length = n-Header;
        if (Get(b,4) != protocol || Get(b,8) != version) {
            if (host) Reject(peer,"protocol/version mismatch");
            else if (kind == Refuse) { error = "protocol/version mismatch"; }
            return;
        }
        // Probes are answered whatever their content, so a client can tell an incompatible session.
        if (Content(b) != content && kind != Refuse && kind != Query) {
            if (host) Reject(peer,"different game edition or multiplayer maps");
            return;
        }
        if (host && (kind == Hello || kind == Query)) {
            if (!Text(body,length)) { Reject(peer,"invalid name"); return; }
            auto& state = peers[peer];
            if (state.id) return;
            if (started) { Reject(peer,"session started"); return; }
            auto slot = std::find(members.begin(),members.end(),0);
            if (slot == members.end() && kind != Query) { Reject(peer,"session full"); return; }
            uint32_t assigned = 0;
            if (kind == Hello) { assigned = nextId++; *slot = assigned; state.id = assigned; }
            std::vector<uint8_t> welcome(20+name.size()+1);
            Put(welcome,0,assigned);
            for (unsigned i=0;i<4;++i) Put(welcome,4+4*i,members[i]);
            std::memcpy(welcome.data()+20,name.c_str(),name.size()+1);
            // A welcome carries the host's current data sequence: the joiner never receives earlier
            // data, so the test ingress gate must not wait for it.
            Write(peer,Packet(kind == Query ? Answer : Welcome,1,kind == Query ? 0 : sequence,welcome.data(),welcome.size()));
            if (kind != Query) Membership();
            else if (!relay) enet_peer_disconnect_later(reinterpret_cast<ENetPeer*>(peer),0);
            return;
        }
        if (!host && kind == Refuse) {
            if (Text(body,length)) error = reinterpret_cast<const char*>(body);
            return;
        }
        if (!host && kind == Welcome && length >= 21 && length <= 120 && Text(body+20,length-20)) {
            id = Get(body,0); epoch = Get(b,16); name = reinterpret_cast<const char*>(body+20);
            for (unsigned i=0;i<4;++i) members[i] = Get(body,4+4*i);
            received[1] = released[0] = Get(b,24);
            active = id > 1 && members[0] == 1 && std::find(members.begin(),members.end(),id) != members.end();
            return;
        }
        if (Get(b,16) != epoch || !active) return;
        if (!host && kind == Members && origin == 1 && length == 48) {
            for (unsigned i=0;i<4;++i) { members[i] = Get(body,4*i); connections[i] = Get(body,32+4*i); }
            if (!synced) {
                for (unsigned i=0;i<4;++i) {
                    const uint32_t member=members[i], sent=Get(body,16+4*i);
                    if (member && member!=id && member<=4) {
                        received[member]=std::max(received[member],sent);
                        released[member-1]=std::max(released[member-1],sent);
                    }
                }
                synced=true;
            }
            return;
        }
        if (!host && kind == Gone && origin == 1 && length == 4) {
            departures.push_back({Get(body,0),false});
            return;
        }
        if (kind != Data || length < 2 || length > 200 || seq == 0) return;
        if (host) {
            auto& state = peers[peer];
            if (!state.id || origin != state.id) return;
            if (Clock::now()-state.since >= std::chrono::seconds(1)) { state.count=0; state.since=Clock::now(); }
            if (++state.count > 512) { Reject(peer,"payload rate exceeded"); return; }
        } else if (origin == id || std::find(members.begin(),members.end(),origin) == members.end()) return;
        else if (relay && peer != uintptr_t(relayHost)) {
            // Straight from another member (route 2): only its own messages, from its own connection.
            const auto slot = std::find(members.begin(),members.end(),origin) - members.begin();
            if (!fannedOut || connections[slot] == 0 || uintptr_t(connections[slot]) != peer) return;
        }
        Arrived(origin,Get(b,28));
        // A full queue (a game that does not read, e.g. suspended in the background): an unreliable
        // message, a state the next one replaces, is dropped as UDP may drop it; a reliable one takes
        // the place of the oldest unreliable one, and only a queue of reliable messages fails.
        bool keep=true;
        if (incoming.size() >= MaxQueue) {
            if (!reliable) keep=false;
            else {
                auto old=std::find_if(incoming.begin(),incoming.end(),[](const Message& m) { return !m.reliable; });
                if (old==incoming.end()) { Fail("receive queue exceeded"); return; }
                incoming.erase(old);
            }
        }
        if (keep) incoming.push_back({origin,seq,Get(b,28),reliable,std::vector<uint8_t>(body,body+length)});
        received[origin] = std::max(received[origin],seq);
        if (host && !fannedOut) Broadcast(std::vector<uint8_t>(b,b+n),reliable,peer);
    }
};

Session::Session(uint32_t protocol,uint32_t version) : p(new Impl) {
    p->protocol=protocol; p->version=version; sessions.push_back(this);
}
Session::~Session() { Close(); sessions.erase(std::remove(sessions.begin(),sessions.end(),this),sessions.end()); }
bool Session::Create(uint16_t port,const std::string& name,const std::string& player) {
    Close();
    if (!Init() || name.empty() || name.size()>99 || player.size()>19) return false;
    if (p->relay) {
        // In the server's room: no socket of its own, the server relays every message.
        if (!p->relay->Connected()) { p->error="not connected to the server"; return false; }
        RelaySession info;
        static std::mt19937 random{std::random_device{}()};
        for (auto& b:info.instance) b=uint8_t(random());
        info.maxPlayers=4; info.name=name;
        if (!p->relay->HostSession(info)) { p->error=p->relay->Error(); return false; }
        p->host=true; p->active=true; p->id=1; p->members[0]=1; p->name=name; p->player=player; p->port=port;
        p->epoch=uint32_t(Clock::now().time_since_epoch().count()) | 1u;
        return true;
    }
    ENetAddress address{ENET_HOST_ANY,port};
    p->socket=enet_host_create(&address,8,2,0,0);
    if (!p->socket) { p->error="cannot bind UDP port"; return false; }
    p->host=true; p->active=true; p->id=1; p->members[0]=1; p->name=name; p->player=player; p->port=port;
    p->epoch=uint32_t(Clock::now().time_since_epoch().count()) | 1u;
    // LAN discovery (Discover): a plain UDP socket beside the session's. Without it the session can
    // still be joined by address; a second host on the same machine shares it where the system allows.
    if (port < 65535) {
        p->discovery=enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
        ENetAddress any{ENET_HOST_ANY,uint16_t(port+1)};
        if (p->discovery!=ENET_SOCKET_NULL && (enet_socket_set_option(p->discovery,ENET_SOCKOPT_REUSEADDR,1) < 0 ||
            enet_socket_set_option(p->discovery,ENET_SOCKOPT_NONBLOCK,1) < 0 || enet_socket_bind(p->discovery,&any) < 0)) {
            enet_socket_destroy(p->discovery); p->discovery=ENET_SOCKET_NULL;
        }
    }
    return true;
}
bool Session::Join(const Advertisement& ad,const std::string& player,unsigned deadlineMs) {
    Close(); if (!Init() || player.size()>19) return false;
    if (ad.relay) {
        if (!p->relay || !p->relay->Connected()) { p->error="not connected to the server"; return false; }
        uint32_t hostConnection=0;
        if (!p->relay->JoinSession(ad.instance,hostConnection)) { p->error=p->relay->Error(); return false; }
        p->relayHost=hostConnection; p->player=player;
        p->Write(p->Server(),p->Packet(Hello,0,0,player.c_str(),player.size()+1));
        // Through the server the answer takes a round trip over the Internet.
        const auto deadline=Clock::now()+std::chrono::milliseconds(std::max(deadlineMs,10000u));
        while (!p->active && p->error.empty() && Clock::now()<deadline) { Service(2); ServiceAll(); }
        if (!p->active) { if (p->error.empty()) p->error="join timed out"; const auto error=p->error; Close(); p->error=error; return false; }
        return true;
    }
    ENetAddress address{}; address.port=ad.endpoint.port;
    if (enet_address_set_host(&address,ad.endpoint.host.c_str()) < 0) return false;
    p->socket=enet_host_create(nullptr,1,2,0,0); if (!p->socket) return false;
    p->server=enet_host_connect(p->socket,&address,2,0); if (!p->server) { Close(); return false; }
    p->player=player;
    const auto deadline=Clock::now()+std::chrono::milliseconds(deadlineMs);
    while (!p->active && p->error.empty() && Clock::now()<deadline) { Service(2); ServiceAll(); }
    if (!p->active) { if (p->error.empty()) p->error="join timed out"; const auto error=p->error; Close(); p->error=error; return false; }
    return true;
}
std::vector<Advertisement> Session::Probe(const std::vector<Endpoint>& endpoints,unsigned deadlineMs) {
    std::vector<Advertisement> found;
    if (!Init()) return found;
    for (const auto& endpoint : endpoints) {
        ENetAddress address{}; address.port=endpoint.port;
        if (enet_address_set_host(&address,endpoint.host.c_str())<0) continue;
        ENetHost* probe=enet_host_create(nullptr,1,2,0,0); if (!probe) break;
        ENetPeer* peer=enet_host_connect(probe,&address,2,0);
        const auto deadline=Clock::now()+std::chrono::milliseconds(deadlineMs);
        bool done=false;
        while (peer && !done && Clock::now()<deadline) {
            ServiceAll();
            ENetEvent event{};
            if (enet_host_service(probe,&event,1)<=0) continue;
            if (event.type==ENET_EVENT_TYPE_CONNECT) {
                const char query[]="probe";
                p->Write(Impl::Handle(peer),p->Packet(Query,0,0,query,sizeof(query)));
            } else if (event.type==ENET_EVENT_TYPE_RECEIVE) {
                const auto* b=event.packet->data; const size_t n=event.packet->dataLength;
                if (n>=Header+21 && n<=Header+120 && Get(b,0)==Magic && Get(b,4)==p->protocol && Get(b,8)==p->version && Content(b)==p->content &&
                    Get(b,12)==Answer && Get(b,32)==n-Header && Text(b+Header+20,n-Header-20))
                    found.push_back({endpoint,reinterpret_cast<const char*>(b+Header+20),Get(b,16)});
                enet_packet_destroy(event.packet); done=true;
            } else if (event.type==ENET_EVENT_TYPE_DISCONNECT) done=true;
        }
        if (peer) enet_peer_reset(peer);
        enet_host_destroy(probe);
    }
    return found;
}
void Session::SetContent(uint64_t fingerprint) { p->content = fingerprint; }
void Session::UseRelay(Relay* relay) { Close(); p->relay = relay; }
std::vector<Advertisement> Session::List() {
    // The room's sessions. The room only admits builds with the same game, edition and content
    // (the relay's compatibility identity, cnetwork.cpp), so every one of them can be joined.
    std::vector<Advertisement> found;
    if (!p->relay || !p->relay->Connected()) return found;
    std::vector<RelaySession> sessions;
    if (!p->relay->ListSessions(sessions)) { p->error=p->relay->Error(); return found; }
    for (const auto& s:sessions) {
        Advertisement ad; ad.relay=true; ad.instance=s.instance; ad.name=s.name;
        for (int i=0;i<4;++i) ad.epoch|=uint32_t(s.instance[i])<<(8*i);
        found.push_back(std::move(ad));
    }
    return found;
}
std::vector<Advertisement> Session::Discover(uint16_t port,unsigned deadlineMs,unsigned* incompatible) {
    // LAN discovery: one query to the local broadcast address and to this machine, answers collected
    // until the deadline. Joinable sessions with the same protocol, version and content are returned.
    std::vector<Advertisement> found;
    if (incompatible) *incompatible=0;
    if (!Init() || port >= 65535) return found;
    ENetSocket socket=enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
    if (socket==ENET_SOCKET_NULL) return found;
    enet_socket_set_option(socket,ENET_SOCKOPT_BROADCAST,1);
    enet_socket_set_option(socket,ENET_SOCKOPT_NONBLOCK,1);
    std::vector<uint8_t> query(12); Put(query,0,DiscoveryQuery); Put(query,4,p->protocol); Put(query,8,p->version);
    for (enet_uint32 host : {ENET_HOST_BROADCAST, enet_uint32(ENET_HOST_TO_NET_32(0x7f000001))}) {
        ENetAddress to{host,uint16_t(port+1)}; ENetBuffer buffer=Buffer(query.data(),query.size());
        enet_socket_send(socket,&to,&buffer,1);
    }
    std::vector<uint32_t> epochs;
    const auto deadline=Clock::now()+std::chrono::milliseconds(deadlineMs);
    while (Clock::now()<deadline) {
        ServiceAll();
        uint8_t answer[160]; ENetAddress from{}; ENetBuffer buffer=Buffer(answer,sizeof(answer));
        const int n=enet_socket_receive(socket,&from,&buffer,1);
        if (n<=0) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); continue; }
        if (n<34 || Get(answer,0)!=DiscoveryAnswer || Get(answer,4)!=p->protocol || Get(answer,8)!=p->version ||
            !Text(answer+32,size_t(n)-32)) continue;
        const uint64_t content=uint64_t(Get(answer,12)) | uint64_t(Get(answer,16))<<32;
        const uint32_t epoch=Get(answer,20), game=Get(answer,24);
        if (std::find(epochs.begin(),epochs.end(),epoch)!=epochs.end()) continue;
        epochs.push_back(epoch);
        if (content!=p->content) { if (incompatible) ++*incompatible; continue; }
        if (answer[28] || answer[29]>=4 || game==0 || game>65535) continue;
        char ip[64]{};
        if (enet_address_get_host_ip(&from,ip,sizeof(ip))<0) continue;
        found.push_back({{ip,uint16_t(game)},reinterpret_cast<const char*>(answer+32),epoch});
    }
    enet_socket_destroy(socket);
    return found;
}
void Session::Service(unsigned waitMs) {
    if (p->relay && (p->host || p->relayHost)) {
        p->relay->Service();
        RelayEvent e;
        bool any=false;
        while (p->relay->Next(e)) {
            any=true;
            if (e.kind==RelayEvent::PeerJoined && p->host) {
                // A pending member: let its Hello through; the session's own rules then accept or refuse it.
                p->relay->Admit(e.connection);
                p->peers.emplace(uintptr_t(e.connection),Impl::Peer{});
            } else if (e.kind==RelayEvent::PeerLeft && p->host) {
                p->MemberLeft(uintptr_t(e.connection));
            } else if (e.kind==RelayEvent::SessionEnded) {
                p->Fail(p->host ? "the server ended the session" : "host disconnected; host migration unsupported");
            } else if (e.kind==RelayEvent::Deliver && !e.payload.empty()) {
                const bool member=std::find(p->connections.begin(),p->connections.end(),e.connection)!=p->connections.end();
                if (p->host ? !p->peers.count(uintptr_t(e.connection)) : e.connection!=p->relayHost && !member) continue;
                p->Accept(uintptr_t(e.connection),e.payload.data()+1,e.payload.size()-1,(e.payload[0]&RelayReliable)!=0,
                          (e.payload[0]&RelayFannedOut)!=0);
            }
        }
        if (!any && waitMs) Pause(waitMs);
        if (p->active) p->Report(false);
        return;
    }
    if (!p->socket) return;
    if (p->host && p->discovery!=ENET_SOCKET_NULL) {
        // Answer LAN discovery queries: protocol and version must match; content is reported so the
        // asker can tell an incompatible session. Started sessions do not take new members.
        uint8_t query[64]; ENetAddress from{};
        for (unsigned count=0;count<16;++count) {
            ENetBuffer buffer=Buffer(query,sizeof(query));
            if (enet_socket_receive(p->discovery,&from,&buffer,1) != 12) break;
            if (Get(query,0)!=DiscoveryQuery || Get(query,4)!=p->protocol || Get(query,8)!=p->version) continue;
            std::vector<uint8_t> answer(32+p->name.size()+1);
            Put(answer,0,DiscoveryAnswer); Put(answer,4,p->protocol); Put(answer,8,p->version);
            Put(answer,12,uint32_t(p->content)); Put(answer,16,uint32_t(p->content>>32)); Put(answer,20,p->epoch);
            Put(answer,24,p->port);
            answer[28]=p->started; answer[29]=uint8_t(std::count_if(p->members.begin(),p->members.end(),[](uint32_t m){return m!=0;}));
            std::memcpy(answer.data()+32,p->name.c_str(),p->name.size()+1);
            ENetBuffer reply=Buffer(answer.data(),answer.size());
            enet_socket_send(p->discovery,&from,&reply,1);
        }
    }
    ENetEvent event{};
    for (unsigned count=0;count<512 && enet_host_service(p->socket,&event,count ? 0 : waitMs)>0;++count) {
        if (event.type==ENET_EVENT_TYPE_CONNECT) {
            // A peer is given up after 5 to 10 s without acknowledgements (ENet's default limit of 32
            // round trips): a few lost datagrams in a row on a poor link must not end a game, and a
            // member that is gone is still noticed within seconds. (Before, 1.5 to 3 s dropped members
            // on links losing a tenth of their datagrams: net_sb1_impaired.)
            enet_peer_timeout(event.peer,kPeerTimeoutLimit,kPeerTimeoutMinMs,kPeerTimeoutMaxMs);
            if (p->host) p->peers.emplace(Impl::Handle(event.peer),Impl::Peer{});
            else p->Write(Impl::Handle(event.peer),p->Packet(Hello,0,0,p->player.c_str(),p->player.size()+1));
        } else if (event.type==ENET_EVENT_TYPE_RECEIVE) {
            p->Accept(Impl::Handle(event.peer),event.packet->data,event.packet->dataLength,(event.packet->flags & ENET_PACKET_FLAG_RELIABLE)!=0);
            enet_packet_destroy(event.packet);
        } else if (event.type==ENET_EVENT_TYPE_DISCONNECT) {
            if (p->host) p->MemberLeft(Impl::Handle(event.peer));
            else p->Fail("host disconnected; host migration unsupported");
        }
    }
    enet_host_flush(p->socket);
    if (p->active) p->Report(false);
}
bool Session::Send(const void* data,size_t n,bool reliable) {
    if (!p->active || n<2 || n>200 || !data) return false;
    reliable = reliable || options.testSync;
    auto b=p->Packet(Data,p->id,++p->sequence,data,n);
    if (options.testSync) testSent=p->sequence;
    if (p->relay && p->started) { if (!p->FanOut(b,reliable)) { p->Fail("send enqueue failed"); return false; } }
    else if (p->host) p->Broadcast(b,reliable);
    else if (!p->Write(p->Server(),b,reliable)) { p->Fail("send enqueue failed"); return false; }
    if (p->socket) enet_host_flush(p->socket);
    return p->active;
}
bool Session::Receive(Message& message) {
    Service();
    if (options.testSync) {
        std::stable_sort(p->incoming.begin(),p->incoming.end(),[](const Message& a,const Message& b) {
            if (a.tick!=b.tick) return a.tick<b.tick;
            if (a.origin!=b.origin) return a.origin<b.origin;
            return a.sequence<b.sequence;
        });
    }
    if (p->incoming.empty()) return false;
    const auto& first=p->incoming.front();
    if (options.testSync && (first.tick>=currentTick || first.origin>4 || first.sequence>p->released[first.origin-1])) return false;
    message=std::move(p->incoming.front()); p->incoming.pop_front(); return true;
}
bool Session::Departed(Departure& d) { if (p->departures.empty()) return false; d=p->departures.front(); p->departures.pop_front(); return true; }
void Session::Start() { p->started=true; }
void Session::Close(unsigned deadlineMs) {
    p->Report(true);   // also after the host has gone
    // Leaving a session in the server's room keeps the connection and the room (cnetwork.cpp owns them).
    if (p->relay && p->relay->Connected()) {
        if (p->host) p->relay->CloseSession();
        else if (p->relayHost) p->relay->LeaveSession();
    }
    // Whatever the server still sends about the old session (a late SessionEnded after a refusal, a
    // peer's last message) must not reach the next one: Create and Join start with Close.
    if (p->relay) { p->relay->Service(); p->relay->DropEvents(); }
    if (p->socket) {
        if (p->host) for (auto& [peer,state]:p->peers) enet_peer_disconnect_later(reinterpret_cast<ENetPeer*>(peer),0);
        else if (p->server) enet_peer_disconnect_later(p->server,0);
        // enet_peer_disconnect lowers connectedPeers as soon as the disconnect is queued, before it
        // is sent: wait until every peer has acknowledged it (state DISCONNECTED), within the deadline.
        auto pending=[this] {
            for (size_t i=0;i<p->socket->peerCount;++i)
                if (p->socket->peers[i].state!=ENET_PEER_STATE_DISCONNECTED) return true;
            return false;
        };
        const auto deadline=Clock::now()+std::chrono::milliseconds(deadlineMs);
        while (pending() && Clock::now()<deadline) { Service(1); ServiceAll(); }
        enet_host_flush(p->socket);
        enet_host_destroy(p->socket);
    }
    if (p->discovery!=ENET_SOCKET_NULL) enet_socket_destroy(p->discovery);
    const auto protocol=p->protocol,version=p->version; const uint64_t content=p->content; Relay* relay=p->relay;
    p.reset(new Impl); p->protocol=protocol; p->version=version; p->content=content; p->relay=relay;
}
bool Session::Active() const { return p->active; }
bool Session::IsHost() const { return p->host; }
uint32_t Session::Id() const { return p->id; }
bool Session::Knows(uint32_t id) const { return id && std::find(p->members.begin(),p->members.end(),id)!=p->members.end(); }
const std::string& Session::Error() const { return p->error; }
void ServiceAll() { for (auto* s:sessions) s->Service(); }

namespace {
std::string ControlPath(const char* kind,long tick) {
    const char* dir=std::getenv("BLUPI_NET_TEST_DIR"); const char* role=std::getenv("BLUPI_NET_TEST_ROLE");
    if (!dir || !role || role[0]<'0' || role[0]>'3' || role[1]) return {};
    return std::string(dir)+"/"+role+"-"+kind+"-"+std::to_string(tick);
}
}
bool BeginTestTick(long tick) {
    currentTick=uint32_t(tick);   // the envelope's tick: the test lockstep's, and the link diagnostics'
    if (!options.testSync) return true;
    const auto path=ControlPath("grant",tick); if (path.empty()) return false;
    // Longer than a peer timeout (kPeerTimeoutMaxMs): the supervisor holds a tick back that long
    // when it lets a departure be detected.
    const auto deadline=Clock::now()+std::chrono::seconds(30);
    // The grant lists how many data messages each member id 1..N had sent by the end of the
    // previous tick (N = the supervisor's peer count, at most 4).
    uint32_t counts[4]{};
    for (;;) {
        ServiceAll();
        std::ifstream file(path);
        unsigned n=0;
        while (n<4 && file >> counts[n]) ++n;
        if (n>=2) break;
        if (Clock::now()>=deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    currentTick=uint32_t(tick);
    for (auto* s:sessions) {
        auto& p=*s->p;
        for (uint32_t origin=1;origin<=4;++origin) {
            // Only from members still in the session: what a departed member sent after the session
            // dropped it never arrives.
            auto member=[&p,origin] { return std::find(p.members.begin(),p.members.end(),origin)!=p.members.end(); };
            while (p.active && origin!=p.id && member() && p.received[origin]<counts[origin-1]) {
                ServiceAll(); if (Clock::now()>=deadline) return false;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            p.released[origin-1]=counts[origin-1];
        }
    }
    return true;
}
bool EndTestTick(long tick) {
    if (!options.testSync) return true;
    ServiceAll();
    uint32_t sent=testSent;
    for (auto* s:sessions) sent=std::max(sent,s->p->sequence);
    const auto path=ControlPath("done",tick); if (path.empty()) return false;
    std::ofstream file(path+".tmp"); file << sent << '\n'; file.close();
    std::error_code error; std::filesystem::rename(path+".tmp",path,error); return !error;
}
} // namespace blupi::net

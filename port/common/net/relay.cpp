// Free Direct server client (relay.h). Message layouts: ../free-direct/docs/directplay-relay-protocol.md
// (little-endian integers; a string is a u16 byte count and its UTF-8 bytes).
#include "net/relay.h"
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifndef __EMSCRIPTEN__
#include <enet/enet.h>
#else
#include <emscripten.h>
EM_JS_DEPS(blupi_relay_rtc, "$stringToUTF8,$lengthBytesUTF8,$UTF8ToString");
#endif

namespace blupi::net {
namespace {
using Clock = std::chrono::steady_clock;
constexpr uint32_t kMagic = 0x4C524446;   // "FDRL"
constexpr uint16_t kVersion = 1;
enum : uint8_t {
    Hello = 0x01, CreateRoomMsg = 0x02, JoinRoomMsg = 0x03, ListSessionsMsg = 0x04, HostSessionMsg = 0x05,
    CloseSessionMsg = 0x06, JoinSessionMsg = 0x07, LeaveSessionMsg = 0x08, AdmitMsg = 0x09, RefuseMsg = 0x0A,
    DataMsg = 0x0B,
    Welcome = 0x81, ErrorMsg = 0x82, RoomCreated = 0x83, RoomJoined = 0x84, SessionList = 0x85,
    SessionHosted = 0x86, SessionJoined = 0x87, PeerJoinedMsg = 0x88, PeerLeftMsg = 0x89,
    SessionEndedMsg = 0x8A, DeliverMsg = 0x8B,
    RtcOfferMsg = 0x0C, RtcAnswerMsg = 0x8C,   // WebRTC signalling (browsers' datagrams)
};
// Datagrams (the protocol's "Datagrams"; ../free-direct/src/relay/RelayDatagram.hpp).
constexpr uint32_t kDatagramMagic = 0x55524446;   // "FDRU"
constexpr uint8_t kWelcomeFlagDatagrams = 0x02, kWelcomeFlagWebRtc = 0x04;
constexpr auto kWebRtcTimeout = std::chrono::milliseconds(10000);
constexpr size_t kMaxDatagram = 1200;
constexpr uint8_t DatagramBind = 0x01, DatagramData = 0x02, DatagramBindAck = 0x81, DatagramDeliver = 0x82;
constexpr auto kBindRetry = std::chrono::milliseconds(250), kBindTimeout = std::chrono::milliseconds(3000);
constexpr auto kKeepAlive = std::chrono::seconds(15);
uint32_t Get32(const uint8_t* p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; }
// The host of a ws:// or wss:// URL (ServerUrl's form), without brackets.
std::string UrlHost(const std::string& url)
{
    size_t at = url.find("://");
    at = at == std::string::npos ? 0 : at + 3;
    if (at < url.size() && url[at] == '[') {
        const size_t end = url.find(']', at);
        return end == std::string::npos ? std::string() : url.substr(at + 1, end - at - 1);
    }
    const size_t end = url.find_first_of(":/", at);
    return url.substr(at, end == std::string::npos ? std::string::npos : end - at);
}
// The application GUID the port's sessions carry (the server does not interpret it).
constexpr std::array<uint8_t, 16> kApplication = {0x42, 0x4c, 0x55, 0x50, 0x49, 0x2d, 0x53, 0x44,
                                                  0x4c, 0x33, 0x2d, 0x50, 0x4f, 0x52, 0x54, 0x01};

struct Writer {
    std::vector<uint8_t> b;
    explicit Writer(uint8_t type) { b.push_back(type); }
    Writer& U8(uint8_t v) { b.push_back(v); return *this; }
    Writer& U16(uint16_t v) { b.push_back(uint8_t(v)); b.push_back(uint8_t(v >> 8)); return *this; }
    Writer& U32(uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back(uint8_t(v >> (8 * i))); return *this; }
    Writer& Str(const std::string& s) { U16(uint16_t(s.size())); b.insert(b.end(), s.begin(), s.end()); return *this; }
    Writer& Bytes(const uint8_t* p, size_t n) { b.insert(b.end(), p, p + n); return *this; }
};
struct Reader {
    const std::vector<uint8_t>& b;
    size_t at = 1;
    bool ok = true;
    explicit Reader(const std::vector<uint8_t>& m) : b(m) {}
    bool Need(size_t n) { if (at + n > b.size()) ok = false; return ok; }
    uint8_t U8() { if (!Need(1)) return 0; return b[at++]; }
    uint16_t U16() { if (!Need(2)) return 0; uint16_t v = uint16_t(b[at] | b[at + 1] << 8); at += 2; return v; }
    uint32_t U32() {
        if (!Need(4)) return 0;
        uint32_t v = 0;
        for (int i = 0; i < 4; ++i) v |= uint32_t(b[at + i]) << (8 * i);
        at += 4;
        return v;
    }
    std::string Str() {
        const uint16_t n = U16();
        if (!Need(n)) return {};
        std::string s(b.begin() + at, b.begin() + at + n);
        at += n;
        return s;
    }
    void Guid(std::array<uint8_t, 16>& g) { if (Need(16)) { std::memcpy(g.data(), b.data() + at, 16); at += 16; } }
};
} // namespace

struct Relay::Datagrams {
#ifndef __EMSCRIPTEN__
    ENetSocket socket = ENET_SOCKET_NULL;
    ENetAddress server{};
#else
    bool offerSent = false;
#endif
    std::array<uint8_t, 16> token{};
    enum class State { Binding, Bound, Off } state = State::Binding;
    Clock::time_point started{}, lastBind{};
    std::vector<uint8_t> Header(uint8_t type) const {
        std::vector<uint8_t> b;
        for (int i = 0; i < 4; ++i) b.push_back(uint8_t(kDatagramMagic >> (8 * i)));
        b.insert(b.end(), token.begin(), token.end());
        b.push_back(type);
        return b;
    }
    bool Send(const std::vector<uint8_t>& d) {
#ifndef __EMSCRIPTEN__
        ENetBuffer buffer;
        buffer.data = const_cast<uint8_t*>(d.data());
        buffer.dataLength = d.size();
        return enet_socket_send(socket, &server, &buffer, 1) == int(d.size());
#else
        // The page's data channel (StartDatagrams): unordered, without retransmissions.
        return EM_ASM_INT({
            var R = Module.blupiRtc;
            if (!R || !R.dc || R.dc.readyState !== 'open') return 0;
            try { R.dc.send(HEAPU8.slice($0, $0 + $1)); R.sent++; return 1; } catch (e) { return 0; }
        }, d.data(), int(d.size())) != 0;
#endif
    }
    ~Datagrams() {
#ifndef __EMSCRIPTEN__
        if (socket != ENET_SOCKET_NULL) enet_socket_destroy(socket);
#else
        EM_ASM({
            var R = Module.blupiRtc;
            if (R && R.pc) { try { R.pc.close(); } catch (e) {} }
            Module.blupiRtc = null;
        });
#endif
    }
};

Relay::Relay() = default;
Relay::~Relay() = default;

bool Relay::DatagramsOn() const { return datagrams && datagrams->state == Datagrams::State::Bound; }

void Relay::StartDatagrams(uint16_t port, const std::array<uint8_t, 16>& token)
{
    datagrams.reset();
#ifndef __EMSCRIPTEN__
    // ENet's sockets: IPv4 only, as the port's WebSocket over them.
    auto d = std::make_unique<Datagrams>();
    const std::string host = UrlHost(url);
    if (port == 0 || host.empty() || enet_address_set_host(&d->server, host.c_str()) != 0) return;
    d->server.port = port;
    d->socket = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
    if (d->socket == ENET_SOCKET_NULL) return;
    ENetAddress any{};
    any.host = ENET_HOST_ANY;
    any.port = 0;
    if (enet_socket_bind(d->socket, &any) < 0 || enet_socket_set_option(d->socket, ENET_SOCKOPT_NONBLOCK, 1) < 0) return;
    d->token = token;
    d->started = Clock::now();
    datagrams = std::move(d);
    ServiceDatagrams();   // the first Bind
#else
    // A browser: a WebRTC peer connection to the server with one data channel, unordered and
    // without retransmissions, so a lost datagram is lost as over UDP. Its offer, complete with the
    // candidates, goes to the server as RtcOffer (ServiceDatagrams); the answer comes back as
    // RtcAnswer (Handle). The server sends BindAck over the channel when it opens.
    (void)port;
    const bool ok = EM_ASM_INT({
        if (typeof RTCPeerConnection === 'undefined') return 0;
        // (No commas outside parentheses: EM_ASM's code is a macro argument.)
        var R = Module.blupiRtc = {};
        R.pc = null; R.dc = null; R.offer = null; R.inbox = []; R.failed = false;
        R.sent = 0; R.received = 0;   // datagrams, for port/tools/web_check.mjs
        try {
            R.pc = new RTCPeerConnection({iceServers: []});
            var options = {};
            options.ordered = false;
            options.maxRetransmits = 0;
            R.dc = R.pc.createDataChannel('datagrams', options);
            R.dc.binaryType = 'arraybuffer';
            R.dc.onmessage = function(e) { if (e.data instanceof ArrayBuffer) { R.inbox.push(e.data); R.received++; } };
            R.pc.onicegatheringstatechange = function() {
                if (R.pc.iceGatheringState === 'complete' && R.offer === null) R.offer = R.pc.localDescription.sdp;
            };
            R.pc.createOffer().then(function(o) { return R.pc.setLocalDescription(o); })
                .catch(function() { R.failed = true; });
        } catch (e) { R.failed = true; }
        return 1;
    }) != 0;
    if (!ok) return;
    auto d = std::make_unique<Datagrams>();
    d->token = token;
    d->started = Clock::now();
    datagrams = std::move(d);
#endif
}

void Relay::ServiceDatagrams()
{
    if (!datagrams) return;
    Datagrams& d = *datagrams;
    auto deliver = [this, &d](const uint8_t* buffer, size_t n) {
        if (n < 5 || n > kMaxDatagram || Get32(buffer) != kDatagramMagic) return;
        if (buffer[4] == DatagramBindAck && d.state == Datagrams::State::Binding) {
            d.state = Datagrams::State::Bound;
#ifdef __EMSCRIPTEN__
            std::fprintf(stderr, "network: server datagrams on (WebRTC)\n");
#else
            std::fprintf(stderr, "network: server datagrams on (UDP port %u)\n", unsigned(d.server.port));
#endif
        } else if (buffer[4] == DatagramDeliver && n >= 9) {
            RelayEvent e;
            e.kind = RelayEvent::Deliver;
            e.connection = Get32(buffer + 5);
            e.payload.assign(buffer + 9, buffer + n);
            events.push_back(std::move(e));
        }
    };
    // Read in every state: the server sends datagrams once it has a Bind (or an open channel),
    // even if its BindAck never came back.
#ifndef __EMSCRIPTEN__
    uint8_t buffer[kMaxDatagram + 1];
    for (int i = 0; i < 256; ++i) {
        ENetAddress from{};
        ENetBuffer b;
        b.data = buffer;
        b.dataLength = sizeof(buffer);
        const int n = enet_socket_receive(d.socket, &from, &b, 1);
        if (n <= 0) break;
        if (from.host != d.server.host || from.port != d.server.port) continue;
        deliver(buffer, size_t(n));
    }
#else
    for (int i = 0; i < 256; ++i) {
        const int n = EM_ASM_INT({
            var R = Module.blupiRtc;
            return R && R.inbox.length ? R.inbox[0].byteLength : -1;
        });
        if (n < 0) break;
        std::vector<uint8_t> buffer(size_t(n) + 1);
        EM_ASM({ HEAPU8.set(new Uint8Array(Module.blupiRtc.inbox.shift()), $0); }, buffer.data());
        deliver(buffer.data(), size_t(n));
    }
    if (!d.offerSent && d.state == Datagrams::State::Binding) {
        const int length = EM_ASM_INT({
            var R = Module.blupiRtc;
            return R && R.offer !== null ? lengthBytesUTF8(R.offer) : (R && R.failed ? -2 : -1);
        });
        if (length == -2) d.started = Clock::time_point{};   // no offer: give up at once (below)
        if (length >= 0) {
            std::vector<uint8_t> sdp(size_t(length) + 1);
            EM_ASM({ stringToUTF8(Module.blupiRtc.offer, $0, $1); }, sdp.data(), length + 1);
            Writer w(RtcOfferMsg);
            w.Bytes(sdp.data(), size_t(length));
            d.offerSent = socket.Send(w.b);
        }
    }
#endif
    const auto now = Clock::now();
#ifndef __EMSCRIPTEN__
    const auto timeout = Clock::duration(kBindTimeout);
#else
    const auto timeout = Clock::duration(kWebRtcTimeout);
#endif
    if (d.state == Datagrams::State::Binding && now - d.started > timeout) {
        d.state = Datagrams::State::Off;
        std::fprintf(stderr, "network: server datagrams off: no answer; everything over the WebSocket\n");
    }
#ifndef __EMSCRIPTEN__
    if (d.state == Datagrams::State::Off) return;
    if (now - d.lastBind >= (d.state == Datagrams::State::Binding ? Clock::duration(kBindRetry) : Clock::duration(kKeepAlive))) {
        d.lastBind = now;   // a Bind again keeps a NAT's mapping open
        d.Send(d.Header(DatagramBind));
    }
#endif
}

bool RoomId(const std::string& typed, std::string& id)
{
    static const char* alphabet = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
    std::string out;
    for (char ch : typed) {
        char c = (char)std::toupper((unsigned char)ch);
        if (c == ' ' || c == '-' || c == '\t') continue;
        if (c == 'O') c = '0';
        if (c == 'I' || c == 'L') c = '1';
        if (!std::strchr(alphabet, c) || c == '\0') return false;
        out += c;
    }
    if (out.size() != 20) return false;
    id = out;
    return true;
}

std::string RoomIdText(const std::string& id)
{
    std::string out;
    for (size_t i = 0; i < id.size(); ++i) {
        if (i != 0 && i % 5 == 0) out += '-';
        out += id[i];
    }
    return out;
}

namespace {
// A link's query value (?name= or &name=), percent-decoded; empty if the link has none.
std::string LinkValue(const std::string& text, const std::string& name)
{
    const std::string key = name + "=";
    for (size_t at = text.find(key); at != std::string::npos; at = text.find(key, at + 1)) {
        if (at == 0 || (text[at - 1] != '?' && text[at - 1] != '&')) continue;
        std::string value;
        for (size_t i = at + key.size(); i < text.size(); ++i) {
            const char c = text[i];
            if (c == '&' || c == '#' || std::isspace((unsigned char)c)) break;
            if (c == '%' && i + 2 < text.size() && std::isxdigit((unsigned char)text[i + 1]) &&
                std::isxdigit((unsigned char)text[i + 2])) {
                value += char(std::strtol(text.substr(i + 1, 2).c_str(), nullptr, 16));
                i += 2;
            } else {
                value += c == '+' ? ' ' : c;
            }
        }
        return value;
    }
    return {};
}

// A word of a sentence is a room ID only in the server's own form (five groups of five), so that
// no ordinary word of twenty letters is taken for one.
bool WrittenRoomId(const std::string& word, std::string& id)
{
    if (word.size() != 23 || word[5] != '-' || word[11] != '-' || word[17] != '-') return false;
    return RoomId(word, id);
}
} // namespace

bool Invite(const std::string& text, std::string& server, std::string& room)
{
    server.clear();
    room.clear();
    std::string id;
    const std::string linkServer = LinkValue(text, "server"), linkRoom = LinkValue(text, "room");
    if (!linkServer.empty() || !linkRoom.empty()) {
        for (char c : linkServer) if (!std::isspace((unsigned char)c)) server += c;
        if (RoomId(linkRoom, id)) room = RoomIdText(id);
        return !server.empty() || !room.empty();
    }
    std::string trimmed = text;
    while (!trimmed.empty() && std::isspace((unsigned char)trimmed.back())) trimmed.pop_back();
    while (!trimmed.empty() && std::isspace((unsigned char)trimmed.front())) trimmed.erase(trimmed.begin());
    if (RoomId(trimmed, id)) {   // the ID alone, however it was typed
        room = RoomIdText(id);
        return true;
    }
    // A word of a sentence; words end at anything but letters, digits and dashes.
    std::string word;
    for (size_t i = 0; i <= text.size(); ++i) {
        const char c = i < text.size() ? text[i] : ' ';
        if (std::isalnum((unsigned char)c) || c == '-') {
            word += c;
            continue;
        }
        if (WrittenRoomId(word, id)) {
            room = RoomIdText(id);
            return true;
        }
        word.clear();
    }
    return false;
}

bool Relay::Handle(const std::vector<uint8_t>& message, uint8_t want, std::vector<uint8_t>* reply)
{
    if (message.empty()) return false;
    const uint8_t type = message[0];
    Reader r(message);
    if (type == want && reply) { *reply = message; return true; }
    switch (type) {
    case PeerJoinedMsg: { RelayEvent e; e.kind = RelayEvent::PeerJoined; e.connection = r.U32(); if (r.ok) events.push_back(e); break; }
    case PeerLeftMsg: { RelayEvent e; e.kind = RelayEvent::PeerLeft; e.connection = r.U32(); if (r.ok) events.push_back(e); break; }
    case SessionEndedMsg: { RelayEvent e; e.kind = RelayEvent::SessionEnded; e.reason = r.U8(); if (r.ok) events.push_back(e); break; }
    case DeliverMsg: {
        RelayEvent e; e.kind = RelayEvent::Deliver; e.connection = r.U32();
        if (r.ok) { e.payload.assign(message.begin() + 5, message.end()); events.push_back(std::move(e)); }
        break;
    }
    case RtcAnswerMsg: {
#ifdef __EMSCRIPTEN__
        // The server's answer to the data channel's offer (StartDatagrams).
        std::string sdp(message.begin() + 1, message.end());
        EM_ASM({
            var R = Module.blupiRtc;
            if (!R || !R.pc) return;
            var answer = {};
            answer.type = 'answer';
            answer.sdp = UTF8ToString($0);
            R.pc.setRemoteDescription(answer).catch(function() { R.failed = true; });
        }, sdp.c_str());
#endif
        break;
    }
    case ErrorMsg: {
        const uint8_t request = r.U8(); r.U16(); const std::string text = r.Str();
        error = text.empty() ? "the server refused the request" : text;
        if (reply && request != 0 && want != 0) { reply->clear(); return true; }   // the answer to this request
        break;
    }
    default: break;   // a late answer to an earlier request: nothing waits for it any more
    }
    return false;
}

bool Relay::Request(const std::vector<uint8_t>& message, uint8_t answer, std::vector<uint8_t>& reply, unsigned ms)
{
    reply.clear();
    if (socket.Status() != WebSocket::State::Open || !socket.Send(message)) {
        if (error.empty()) error = socket.Error().empty() ? "not connected to the server" : socket.Error();
        return false;
    }
    const auto deadline = Clock::now() + std::chrono::milliseconds(ms);
    std::vector<uint8_t> in;
    while (Clock::now() < deadline) {
        while (socket.Receive(in))
            if (Handle(in, answer, &reply)) return !reply.empty();
        if (socket.Status() != WebSocket::State::Open) {
            error = socket.Error().empty() ? "the connection to the server broke" : socket.Error();
            connected = false;
            return false;
        }
        Pause(1);
    }
    error = "the server does not answer";
    return false;
}

bool Relay::Connect(const std::string& address, const std::string& password, const std::string& compat)
{
    Disconnect();
    error.clear();
    url = address;
    if (!socket.Open(address)) { error = socket.Error(); return false; }
    const auto deadline = Clock::now() + std::chrono::seconds(10);
    while (socket.Status() == WebSocket::State::Connecting && Clock::now() < deadline) { socket.Service(); Pause(1); }
    if (socket.Status() != WebSocket::State::Open) {
        error = socket.Error().empty() ? "cannot reach the server" : socket.Error();
        socket.Close();
        return false;
    }
    std::vector<uint8_t> reply;
    if (!Request(Writer(Hello).U32(kMagic).U16(kVersion).Str(compat).Str(password).Str("blupi-sdl3").b, Welcome, reply)) {
        socket.Close();
        return false;
    }
    Reader r(reply);
    r.U16();
    connection = r.U32();
    const uint8_t flags = r.U8();
    connected = r.ok;
    if (!connected) { error = "the server's answer is not understood"; socket.Close(); return false; }
    if (flags & (kWelcomeFlagDatagrams | kWelcomeFlagWebRtc)) {
        // The server's UDP port (0: none) and this connection's token (a server before datagrams
        // sends neither). A browser has no UDP: it asks for a WebRTC data channel instead.
        const uint16_t port = r.U16();
        std::array<uint8_t, 16> token{};
        for (auto& byte : token) byte = r.U8();
#ifdef __EMSCRIPTEN__
        if (r.ok && (flags & kWelcomeFlagWebRtc)) StartDatagrams(0, token);
#else
        if (r.ok && (flags & kWelcomeFlagDatagrams)) StartDatagrams(port, token);
#endif
    }
    return connected;
}

bool Relay::CreateRoom()
{
    std::vector<uint8_t> reply;
    if (!Request(Writer(CreateRoomMsg).b, RoomCreated, reply)) return false;
    Reader r(reply);
    room = r.Str();
    return r.ok && !room.empty();
}

bool Relay::JoinRoom(const std::string& id)
{
    std::vector<uint8_t> reply;
    if (!Request(Writer(JoinRoomMsg).Str(id).b, RoomJoined, reply)) return false;
    Reader r(reply);
    room = r.Str();
    return r.ok;
}

bool Relay::ListSessions(std::vector<RelaySession>& out)
{
    out.clear();
    const uint32_t request = nextRequest++;
    std::vector<uint8_t> reply;
    // An answer to an earlier, abandoned list carries another request ID: wait for this one.
    const auto deadline = Clock::now() + std::chrono::seconds(8);
    while (Clock::now() < deadline) {
        if (!Request(Writer(ListSessionsMsg).U32(request).b, SessionList, reply)) return false;
        Reader r(reply);
        if (r.U32() != request) continue;
        const uint16_t count = r.U16();
        for (uint16_t i = 0; i < count && r.ok; ++i) {
            RelaySession s;
            std::array<uint8_t, 16> application{};
            r.Guid(s.instance); r.Guid(application);
            s.maxPlayers = r.U32(); s.players = r.U32(); s.name = r.Str();
            if (r.ok) out.push_back(s);
        }
        if (!r.ok) { out.clear(); error = "the server's session list is not understood"; }
        return r.ok;
    }
    error = "the server does not answer";
    return false;
}

bool Relay::HostSession(const RelaySession& info)
{
    std::vector<uint8_t> reply;
    return Request(Writer(HostSessionMsg).Bytes(info.instance.data(), 16).Bytes(kApplication.data(), 16)
                       .U32(info.maxPlayers).U32(1).Str(info.name).b, SessionHosted, reply);
}

bool Relay::JoinSession(const std::array<uint8_t, 16>& instance, uint32_t& host)
{
    std::vector<uint8_t> reply;
    if (!Request(Writer(JoinSessionMsg).Bytes(instance.data(), 16).b, SessionJoined, reply)) return false;
    Reader r(reply);
    host = r.U32();
    return r.ok;
}

// The next four do not wait for an answer and ignore a failed send: when the connection is gone,
// the server ends the session or drops the member itself, and Service() reports the broken connection.
void Relay::CloseSession() { if (connected) socket.Send(Writer(CloseSessionMsg).b); }
void Relay::LeaveSession() { if (connected) socket.Send(Writer(LeaveSessionMsg).b); }
void Relay::Admit(uint32_t member) { if (connected) socket.Send(Writer(AdmitMsg).U32(member).b); }
void Relay::Refuse(uint32_t member) { if (connected) socket.Send(Writer(RefuseMsg).U32(member).b); }

bool Relay::Data(uint8_t route, uint32_t target, const std::vector<uint8_t>& payload, bool reliable)
{
    if (!connected) return false;
    if (!reliable && DatagramsOn() && 4 + 16 + 1 + 1 + 4 + payload.size() <= kMaxDatagram) {
        std::vector<uint8_t> d = datagrams->Header(DatagramData);
        d.push_back(route);
        for (int i = 0; i < 4; ++i) d.push_back(uint8_t(target >> (8 * i)));
        d.insert(d.end(), payload.begin(), payload.end());
        if (datagrams->Send(d)) return true;
    }
    Writer w(DataMsg);
    w.U8(route).U32(target).Bytes(payload.data(), payload.size());
    return socket.Send(w.b);
}

void Relay::Service()
{
    if (!connected) return;
    std::vector<uint8_t> in;
    while (socket.Receive(in)) Handle(in, 0, nullptr);
    ServiceDatagrams();
    if (socket.Status() != WebSocket::State::Open) {
        connected = false;
        error = socket.Error().empty() ? "the connection to the server broke" : socket.Error();
        RelayEvent e; e.kind = RelayEvent::SessionEnded; e.reason = 2;
        events.push_back(e);
    }
}

bool Relay::Next(RelayEvent& event)
{
    if (events.empty()) return false;
    event = std::move(events.front());
    events.pop_front();
    return true;
}

void Relay::Disconnect()
{
    datagrams.reset();
    socket.Close();
    connected = false;
    connection = 0;
    room.clear();
    events.clear();
}
} // namespace blupi::net

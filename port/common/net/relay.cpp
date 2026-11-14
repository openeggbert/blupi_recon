// Free Direct server client (relay.h). Message layouts: ../free-direct/docs/directplay-relay-protocol.md
// (little-endian integers; a string is a u16 byte count and its UTF-8 bytes).
#include "net/relay.h"
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstring>

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
};
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
    connected = r.ok;
    if (!connected) { error = "the server's answer is not understood"; socket.Close(); }
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

bool Relay::Data(uint8_t route, uint32_t target, const std::vector<uint8_t>& payload)
{
    if (!connected) return false;
    Writer w(DataMsg);
    w.U8(route).U32(target).Bytes(payload.data(), payload.size());
    return socket.Send(w.b);
}

void Relay::Service()
{
    if (!connected) return;
    std::vector<uint8_t> in;
    while (socket.Receive(in)) Handle(in, 0, nullptr);
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
    socket.Close();
    connected = false;
    connection = 0;
    room.clear();
    events.clear();
}
} // namespace blupi::net

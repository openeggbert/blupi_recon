// WebSocket client (websocket.h): RFC 6455, client side, binary messages. Native: ws:// over ENet's
// portable TCP sockets, non-blocking and polled. Browser: emscripten's binding of the page's WebSocket.
#include "net/websocket.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <random>
#include <thread>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/websocket.h>
#else
#include <enet/enet.h>
#endif

namespace blupi::net {
namespace {
constexpr uint16_t kRelayPort = 51325;   // free-direct-server's default port
constexpr size_t kMaxMessage = 65536;    // the relay's limit is 8192

std::string Lower(std::string s)
{
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

bool StartsWith(const std::string& s, const char* prefix)
{
    return Lower(s.substr(0, std::strlen(prefix))) == prefix;
}
} // namespace

bool ServerUrl(const std::string& typed, bool secureDefault, std::string& url)
{
    size_t a = typed.find_first_not_of(" \t"), b = typed.find_last_not_of(" \t");
    if (a == std::string::npos) return false;
    std::string text = typed.substr(a, b - a + 1), scheme, rest;
    bool explicitScheme = true;
    if (StartsWith(text, "ws://")) { scheme = "ws"; rest = text.substr(5); }
    else if (StartsWith(text, "wss://")) { scheme = "wss"; rest = text.substr(6); }
    else if (text.find("://") != std::string::npos) return false;
    else { scheme = secureDefault ? "wss" : "ws"; rest = text; explicitScheme = false; }
    size_t slash = rest.find('/');
    std::string hostPort = rest.substr(0, slash), path = slash == std::string::npos ? "/" : rest.substr(slash);
    for (char c : path) if ((unsigned char)c <= ' ' || c == '#') return false;
    size_t colon = hostPort.rfind(':');
    std::string host = hostPort.substr(0, colon);
    // A URL without a port means its scheme's port (a TLS proxy on 443); a bare address the server's.
    unsigned long port = explicitScheme ? (scheme == "wss" ? 443 : 80) : kRelayPort;
    if (colon != std::string::npos) {
        std::string digits = hostPort.substr(colon + 1);
        if (digits.empty() || digits.size() > 5 || digits.find_first_not_of("0123456789") != std::string::npos) return false;
        port = std::strtoul(digits.c_str(), nullptr, 10);
        if (port == 0 || port > 65535) return false;
    }
    if (host.empty() || host.size() > 253) return false;
    for (char c : host)
        if (!std::isalnum((unsigned char)c) && c != '.' && c != '-') return false;
    url = scheme + "://" + Lower(host) + ":" + std::to_string(port) + path;
    return true;
}

#ifdef __EMSCRIPTEN__

struct WebSocket::Impl {
    EMSCRIPTEN_WEBSOCKET_T socket = 0;
    State state = State::Closed;
    std::string error;
    std::deque<std::vector<uint8_t>> messages;
};

namespace {
EM_BOOL OnOpen(int, const EmscriptenWebSocketOpenEvent*, void* user)
{
    static_cast<WebSocket::Impl*>(user)->state = WebSocket::State::Open;
    return EM_TRUE;
}
EM_BOOL OnMessage(int, const EmscriptenWebSocketMessageEvent* e, void* user)
{
    auto* p = static_cast<WebSocket::Impl*>(user);
    if (e->isText || e->numBytes > kMaxMessage) { p->state = WebSocket::State::Failed; p->error = "unexpected message from the server"; return EM_TRUE; }
    p->messages.emplace_back(e->data, e->data + e->numBytes);
    return EM_TRUE;
}
EM_BOOL OnError(int, const EmscriptenWebSocketErrorEvent*, void* user)
{
    auto* p = static_cast<WebSocket::Impl*>(user);
    if (p->state != WebSocket::State::Closed) { p->state = WebSocket::State::Failed; if (p->error.empty()) p->error = "cannot reach the server"; }
    return EM_TRUE;
}
EM_BOOL OnClose(int, const EmscriptenWebSocketCloseEvent*, void* user)
{
    auto* p = static_cast<WebSocket::Impl*>(user);
    if (p->state == WebSocket::State::Open) { p->state = WebSocket::State::Closed; p->error = "the server closed the connection"; }
    else if (p->state == WebSocket::State::Connecting) { p->state = WebSocket::State::Failed; p->error = "cannot reach the server"; }
    return EM_TRUE;
}
} // namespace

WebSocket::WebSocket() : p(new Impl) {}
WebSocket::~WebSocket() { Close(); }

bool WebSocket::Open(const std::string& url)
{
    Close();
    p->error.clear();
    if (!emscripten_websocket_is_supported()) { p->state = State::Failed; p->error = "this browser has no WebSocket"; return false; }
    EmscriptenWebSocketCreateAttributes attributes;
    emscripten_websocket_init_create_attributes(&attributes);
    attributes.url = url.c_str();
    attributes.protocols = nullptr;
    attributes.createOnMainThread = EM_TRUE;
    p->socket = emscripten_websocket_new(&attributes);
    if (p->socket <= 0) { p->socket = 0; p->state = State::Failed; p->error = "the browser refused the address"; return false; }
    emscripten_websocket_set_onopen_callback(p->socket, p.get(), OnOpen);
    emscripten_websocket_set_onmessage_callback(p->socket, p.get(), OnMessage);
    emscripten_websocket_set_onerror_callback(p->socket, p.get(), OnError);
    emscripten_websocket_set_onclose_callback(p->socket, p.get(), OnClose);
    p->state = State::Connecting;
    return true;
}

void WebSocket::Service() {}

bool WebSocket::Send(const std::vector<uint8_t>& message)
{
    if (p->state != State::Open) return false;
    // The binding takes a non-const pointer but does not write through it.
    return emscripten_websocket_send_binary(p->socket, const_cast<uint8_t*>(message.data()), message.size()) ==
           EMSCRIPTEN_RESULT_SUCCESS;
}

void WebSocket::Close()
{
    if (p->socket) {
        emscripten_websocket_close(p->socket, 1000, "");
        emscripten_websocket_delete(p->socket);
        p->socket = 0;
    }
    p->state = State::Closed;
    p->messages.clear();
}

void Pause(unsigned ms) { emscripten_sleep(ms); }
bool SecurePage() { return EM_ASM_INT({ return location.protocol === 'https:' ? 1 : 0; }) != 0; }

#else  // native

namespace {
using Clock = std::chrono::steady_clock;

// SHA-1 (FIPS 180-4), only for the handshake's Sec-WebSocket-Accept check.
std::string Sha1(const std::string& text)
{
    uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    std::vector<uint8_t> m(text.begin(), text.end());
    const uint64_t bits = uint64_t(m.size()) * 8;
    m.push_back(0x80);
    while (m.size() % 64 != 56) m.push_back(0);
    for (int i = 7; i >= 0; --i) m.push_back(uint8_t(bits >> (8 * i)));
    auto rol = [](uint32_t v, int n) { return (v << n) | (v >> (32 - n)); };
    for (size_t block = 0; block < m.size(); block += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; ++i)
            w[i] = uint32_t(m[block + 4 * i]) << 24 | uint32_t(m[block + 4 * i + 1]) << 16 |
                   uint32_t(m[block + 4 * i + 2]) << 8 | uint32_t(m[block + 4 * i + 3]);
        for (int i = 16; i < 80; ++i) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; ++i) {
            uint32_t f, k;
            if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999; }
            else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
            else { f = b ^ c ^ d; k = 0xCA62C1D6; }
            const uint32_t t = rol(a, 5) + f + e + k + w[i];
            e = d; d = c; c = rol(b, 30); b = a; a = t;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
    }
    std::string digest;
    for (uint32_t v : h) for (int i = 3; i >= 0; --i) digest += char(uint8_t(v >> (8 * i)));
    return digest;
}

std::string Base64(const std::string& bytes)
{
    static const char* abc = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    for (size_t i = 0; i < bytes.size(); i += 3) {
        uint32_t v = uint32_t(uint8_t(bytes[i])) << 16;
        if (i + 1 < bytes.size()) v |= uint32_t(uint8_t(bytes[i + 1])) << 8;
        if (i + 2 < bytes.size()) v |= uint8_t(bytes[i + 2]);
        out += abc[(v >> 18) & 63];
        out += abc[(v >> 12) & 63];
        out += i + 1 < bytes.size() ? abc[(v >> 6) & 63] : '=';
        out += i + 2 < bytes.size() ? abc[v & 63] : '=';
    }
    return out;
}

std::mt19937& Random()
{
    static std::mt19937 random{std::random_device{}()};
    return random;
}
} // namespace

struct WebSocket::Impl {
    State state = State::Closed;
    enum class Step { Connect, Handshake, Frames } step = Step::Connect;
    std::string error, host, path, key;
    uint16_t port = 0;
    ENetSocket socket = ENET_SOCKET_NULL;
    std::vector<uint8_t> in, out, partial;
    std::deque<std::vector<uint8_t>> messages;
    Clock::time_point deadline;

    void Fail(const std::string& why)
    {
        if (state == State::Connecting || state == State::Open) { state = State::Failed; error = why; }
        Drop();
    }
    void Drop()
    {
        if (socket != ENET_SOCKET_NULL) { enet_socket_destroy(socket); socket = ENET_SOCKET_NULL; }
    }
    void Frame(uint8_t opcode, const uint8_t* data, size_t size)
    {
        out.push_back(uint8_t(0x80 | opcode));
        if (size < 126) out.push_back(uint8_t(0x80 | size));
        else if (size < 65536) { out.push_back(0x80 | 126); out.push_back(uint8_t(size >> 8)); out.push_back(uint8_t(size)); }
        else { out.push_back(0x80 | 127); for (int i = 7; i >= 0; --i) out.push_back(uint8_t(uint64_t(size) >> (8 * i))); }
        uint8_t mask[4];
        for (uint8_t& m : mask) m = uint8_t(Random()());
        out.insert(out.end(), mask, mask + 4);
        for (size_t i = 0; i < size; ++i) out.push_back(uint8_t(data[i] ^ mask[i % 4]));
    }
    bool Flush()
    {
        while (!out.empty() && socket != ENET_SOCKET_NULL) {
            ENetBuffer buffer;
            buffer.data = out.data();
            buffer.dataLength = out.size();
            const int sent = enet_socket_send(socket, nullptr, &buffer, 1);
            if (sent < 0) { Fail("the connection to the server broke"); return false; }
            if (sent == 0) break;
            out.erase(out.begin(), out.begin() + sent);
        }
        return true;
    }
    void Read()
    {
        for (int round = 0; round < 64 && socket != ENET_SOCKET_NULL; ++round) {
            uint8_t chunk[4096];
            ENetBuffer buffer;
            buffer.data = chunk;
            buffer.dataLength = sizeof(chunk);
            int n = enet_socket_receive(socket, nullptr, &buffer, 1);
            if (n == 0) {
                // ENet reports both "nothing yet" and an orderly close as 0. A socket that is readable
                // and still yields nothing has been closed by the server; one read more tells it from
                // data that arrived just after the first read.
                enet_uint32 condition = ENET_SOCKET_WAIT_RECEIVE;
                if (enet_socket_wait(socket, &condition, 0) != 0 || !(condition & ENET_SOCKET_WAIT_RECEIVE)) return;
                n = enet_socket_receive(socket, nullptr, &buffer, 1);
                if (n == 0) {
                    if (state == State::Open) { state = State::Closed; error = "the server closed the connection"; Drop(); }
                    else Fail("the server closed the connection");
                    return;
                }
            }
            if (n < 0) { Fail("the connection to the server broke"); return; }
            in.insert(in.end(), chunk, chunk + n);
            if (in.size() > 4 * kMaxMessage) { Fail("too much data from the server"); return; }
        }
    }
    void Handshake()
    {
        static const char end[] = "\r\n\r\n";
        auto at = std::search(in.begin(), in.end(), end, end + 4);
        if (at == in.end()) {
            if (in.size() > 8192) Fail("the server did not answer as a WebSocket server");
            return;
        }
        std::string head(in.begin(), at);
        in.erase(in.begin(), at + 4);
        std::string lower = Lower(head);
        if (lower.compare(0, 9, "http/1.1 ") != 0 || lower.compare(9, 3, "101") != 0) {
            Fail("the server did not accept the connection (" + head.substr(0, head.find('\r')) + ")");
            return;
        }
        const std::string expected = Base64(Sha1(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"));
        size_t field = lower.find("\r\nsec-websocket-accept:");
        if (field == std::string::npos) { Fail("the server did not answer as a WebSocket server"); return; }
        size_t value = head.find_first_not_of(" \t", field + 23);
        size_t stop = head.find('\r', value);
        if (value == std::string::npos || head.substr(value, stop - value) != expected) {
            Fail("the server did not answer as a WebSocket server");
            return;
        }
        step = Step::Frames;
        state = State::Open;
    }
    void Frames()
    {
        while (socket != ENET_SOCKET_NULL && in.size() >= 2) {
            const uint8_t b0 = in[0], b1 = in[1];
            if (b1 & 0x80) { Fail("the server masked a frame"); return; }
            uint64_t length = b1 & 0x7f;
            size_t header = 2;
            if (length == 126) {
                if (in.size() < 4) return;
                length = uint64_t(in[2]) << 8 | in[3];
                header = 4;
            } else if (length == 127) {
                if (in.size() < 10) return;
                length = 0;
                for (int i = 0; i < 8; ++i) length = length << 8 | in[2 + i];
                header = 10;
            }
            if (length > kMaxMessage) { Fail("too large a message from the server"); return; }
            if (in.size() < header + length) return;
            std::vector<uint8_t> payload(in.begin() + header, in.begin() + header + size_t(length));
            in.erase(in.begin(), in.begin() + header + size_t(length));
            const uint8_t opcode = b0 & 0x0f;
            const bool fin = (b0 & 0x80) != 0;
            if (opcode == 0x9) { Frame(0xA, payload.data(), payload.size()); continue; }   // ping -> pong
            if (opcode == 0xA) continue;
            if (opcode == 0x8) {
                Frame(0x8, payload.data(), std::min<size_t>(payload.size(), 2));
                Flush();
                state = State::Closed;
                error = "the server closed the connection";
                Drop();
                return;
            }
            if (opcode == 0x1) { Fail("unexpected text message from the server"); return; }
            if (opcode == 0x2) partial = std::move(payload);
            else if (opcode == 0x0) {
                partial.insert(partial.end(), payload.begin(), payload.end());
                if (partial.size() > kMaxMessage) { Fail("too large a message from the server"); return; }
            } else { Fail("unknown frame from the server"); return; }
            if (fin) messages.push_back(std::move(partial)), partial.clear();
        }
    }
};

WebSocket::WebSocket() : p(new Impl) {}
WebSocket::~WebSocket() { Close(); }

bool WebSocket::Open(const std::string& url)
{
    Close();
    p->error.clear();
    p->state = State::Failed;
    static const bool initialised = enet_initialize() == 0;
    if (!initialised) { p->error = "no network"; return false; }
    if (StartsWith(url, "wss://")) { p->error = "wss:// needs the web version; this build speaks ws:// only"; return false; }
    if (!StartsWith(url, "ws://")) { p->error = "not a server address"; return false; }
    std::string rest = url.substr(5);
    size_t slash = rest.find('/');
    std::string hostPort = rest.substr(0, slash);
    p->path = slash == std::string::npos ? "/" : rest.substr(slash);
    size_t colon = hostPort.rfind(':');
    p->host = hostPort.substr(0, colon);
    p->port = colon == std::string::npos ? 80 : uint16_t(std::strtoul(hostPort.c_str() + colon + 1, nullptr, 10));
    ENetAddress address{};
    address.port = p->port;
    if (enet_address_set_host(&address, p->host.c_str()) < 0) { p->error = "unknown server name " + p->host; return false; }
    p->socket = enet_socket_create(ENET_SOCKET_TYPE_STREAM);
    if (p->socket == ENET_SOCKET_NULL) { p->error = "cannot open a connection"; return false; }
    enet_socket_set_option(p->socket, ENET_SOCKOPT_NONBLOCK, 1);
    enet_socket_set_option(p->socket, ENET_SOCKOPT_NODELAY, 1);
    if (enet_socket_connect(p->socket, &address) < 0) { p->Drop(); p->error = "cannot reach the server"; return false; }
    std::string nonce;
    for (int i = 0; i < 16; ++i) nonce += char(uint8_t(Random()()));
    p->key = Base64(nonce);
    p->step = Impl::Step::Connect;
    p->state = State::Connecting;
    p->deadline = Clock::now() + std::chrono::seconds(8);
    return true;
}

void WebSocket::Service()
{
    if (p->socket == ENET_SOCKET_NULL) return;
    if (p->step == Impl::Step::Connect) {
        enet_uint32 condition = ENET_SOCKET_WAIT_SEND;
        if (enet_socket_wait(p->socket, &condition, 0) < 0) { p->Fail("cannot reach the server"); return; }
        if (!(condition & ENET_SOCKET_WAIT_SEND)) {
            if (Clock::now() >= p->deadline) p->Fail("the server does not answer");
            return;
        }
        int pending = 0;
        if (enet_socket_get_option(p->socket, ENET_SOCKOPT_ERROR, &pending) < 0 || pending != 0) {
            p->Fail("cannot reach the server");
            return;
        }
        std::string request = "GET " + p->path + " HTTP/1.1\r\nHost: " + p->host + ":" + std::to_string(p->port) +
                              "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: " + p->key +
                              "\r\nSec-WebSocket-Version: 13\r\nUser-Agent: blupi-sdl3\r\n\r\n";
        p->out.insert(p->out.end(), request.begin(), request.end());
        p->step = Impl::Step::Handshake;
    }
    if (!p->Flush()) return;
    p->Read();
    if (p->step == Impl::Step::Handshake && p->socket != ENET_SOCKET_NULL) {
        p->Handshake();
        if (p->step == Impl::Step::Handshake && Clock::now() >= p->deadline) p->Fail("the server does not answer");
    }
    if (p->step == Impl::Step::Frames) p->Frames();
    p->Flush();
}

bool WebSocket::Send(const std::vector<uint8_t>& message)
{
    if (p->state != State::Open || message.size() > kMaxMessage) return false;
    p->Frame(0x2, message.data(), message.size());
    return p->Flush() && p->state == State::Open;
}

void WebSocket::Close()
{
    if (p->socket != ENET_SOCKET_NULL && p->state == State::Open) {
        p->Frame(0x8, nullptr, 0);
        p->Flush();
    }
    p->Drop();
    p->state = State::Closed;
    p->in.clear();
    p->out.clear();
    p->partial.clear();
    p->messages.clear();
}

void Pause(unsigned ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
bool SecurePage() { return false; }

#endif

WebSocket::State WebSocket::Status() const { return p->state; }
const std::string& WebSocket::Error() const { return p->error; }

bool WebSocket::Receive(std::vector<uint8_t>& message)
{
    Service();
    if (p->messages.empty()) return false;
    message = std::move(p->messages.front());
    p->messages.pop_front();
    return true;
}
} // namespace blupi::net

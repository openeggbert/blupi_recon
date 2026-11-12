// TLS under the native WebSocket client (net/websocket.cpp): wss:// addresses of a Free Direct server
// on the desktop and Android, through Mbed TLS 3.6 (port/third_party/README.md). Builds with
// BLUPI_TLS (the default) have it; without it, Built() is false and wss:// is refused. A browser
// has its own TLS and never uses this.
//
// Non-blocking, like the plain socket under it: the WebSocket client polls it from the game loop.
// The certificate must be valid for the typed server name and trusted by:
//   - SSL_CERT_FILE / SSL_CERT_DIR (OpenSSL's names), if either is set;
//   - else on Linux the distribution's CA bundle, on Android the system's CA directory;
//   - else on Windows the system's own check of the chain and the name (CertGetCertificateChain,
//     which also fetches root updates), made before anything is sent.
#ifndef BLUPI_NET_TLS_H
#define BLUPI_NET_TLS_H
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace blupi::net {
class Tls {
public:
    // The connection under TLS. Send: bytes sent, 0 if it would block, kBroken. Receive: bytes read,
    // 0 if nothing has arrived, kClosed when the server closed the connection, kBroken.
    struct Io {
        virtual int Send(const uint8_t* data, size_t size) = 0;
        virtual int Receive(uint8_t* data, size_t size) = 0;
        virtual ~Io() = default;
    };
    static constexpr int kClosed = -1, kBroken = -2;

    static bool Built();
    Tls();
    ~Tls();
    Tls(const Tls&) = delete;
    Tls& operator=(const Tls&) = delete;
    // Starts a handshake with host (the name the certificate must be valid for) over io.
    bool Start(const std::string& host, Io& io, std::string& error);
    // 1 when the handshake is done and the certificate checked, 0 not yet, -1 failed (error says why).
    int Handshake(std::string& error);
    // Bytes written, 0 if it would block, -1 failed. After a 0, the next call must start with the
    // same bytes and be at least as long (the record waiting to be sent holds them).
    int Write(const uint8_t* data, size_t size, std::string& error);
    // Bytes read, 0 if nothing has arrived, kClosed when the server closed the connection, kBroken.
    int Read(uint8_t* data, size_t size, std::string& error);
    // Says goodbye (close_notify) if the connection is up, then forgets it.
    void Stop();
    struct Impl;
private:
    std::unique_ptr<Impl> p;
};
} // namespace blupi::net
#endif

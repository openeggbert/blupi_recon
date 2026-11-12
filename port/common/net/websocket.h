// A small WebSocket client for the Free Direct server's relay (net/relay.cpp, port/docs/MULTIPLAYER.md):
// binary messages only. On the desktop and Android it speaks ws:// over ENet's sockets, polled from
// the game loop without threads; in a browser it is the browser's WebSocket (ws:// and wss://).
#ifndef BLUPI_NET_WEBSOCKET_H
#define BLUPI_NET_WEBSOCKET_H
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace blupi::net {
// A server address as players type it: a host name or IP, optionally :port, or a ws:// / wss:// URL.
// Fills a ws:// or wss:// URL with the port (default 51325) and the path; false if it is not one.
// A plain address becomes wss:// when secureDefault is set (a page served over https://).
bool ServerUrl(const std::string& text, bool secureDefault, std::string& url);

class WebSocket {
public:
    enum class State { Closed, Connecting, Open, Failed };
    WebSocket();
    ~WebSocket();
    WebSocket(const WebSocket&) = delete;
    WebSocket& operator=(const WebSocket&) = delete;
    bool Open(const std::string& url);        // starts connecting; Service() completes it
    void Service();                           // polls the connection (cheap; call every frame)
    State Status() const;
    bool Send(const std::vector<uint8_t>& message);
    bool Receive(std::vector<uint8_t>& message);   // the next whole message, if any
    void Close();
    const std::string& Error() const;
    struct Impl;
private:
    std::unique_ptr<Impl> p;
};
// Waits about ms milliseconds, letting a browser run its event loop meanwhile.
void Pause(unsigned ms);
// A page served over https:// may only open wss:// connections (false outside a browser).
bool SecurePage();
} // namespace blupi::net
#endif

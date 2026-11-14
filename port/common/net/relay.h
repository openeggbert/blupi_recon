// Client of the Free Direct server (free-direct-server, ../free-direct/docs/directplay-relay-protocol.md):
// a WebSocket connection to it, a room (a secret ID: a private LAN on the Internet) and the sessions
// in it. net/session.cpp carries its own envelope (Hello, Welcome, Data ...) as the payload of the
// relay's Data messages, exactly as it does over ENet; the server never interprets it.
#ifndef BLUPI_NET_RELAY_H
#define BLUPI_NET_RELAY_H
#include "net/websocket.h"
#include <array>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace blupi::net {
struct RelaySession {
    std::array<uint8_t, 16> instance{};   // the session's id in the room
    uint32_t maxPlayers = 0, players = 0;
    std::string name;
};
struct RelayEvent {
    enum Kind { PeerJoined, PeerLeft, SessionEnded, Deliver } kind = Deliver;
    uint32_t connection = 0;           // the member (PeerJoined/PeerLeft) or the sender (Deliver)
    uint8_t reason = 0;                // SessionEnded: 0 host left, 1 refused, 2 server shutting down
    std::vector<uint8_t> payload;      // Deliver
};
class Relay {
public:
    Relay();
    ~Relay();
    // compat names the game, edition and content: the server only lets a room's creator's kind in.
    bool Connect(const std::string& url, const std::string& password, const std::string& compat);
    bool CreateRoom();                         // then Room() is its new ID
    bool JoinRoom(const std::string& id);
    bool ListSessions(std::vector<RelaySession>& out);
    bool HostSession(const RelaySession& info);
    bool JoinSession(const std::array<uint8_t, 16>& instance, uint32_t& host);
    void CloseSession();                       // the host stops hosting
    void LeaveSession();                       // a member leaves
    void Admit(uint32_t member);
    void Refuse(uint32_t member);
    // Routes: 0 to the host, 1 to one member (target), 2 to all others. A message that need not
    // arrive (reliable false) goes as a datagram when the server takes them (below).
    bool Data(uint8_t route, uint32_t target, const std::vector<uint8_t>& payload, bool reliable = true);
    // Datagrams on: the server acknowledged this program's UDP address.
    bool DatagramsOn() const;
    void Service();                            // reads messages; events queue up
    bool Next(RelayEvent& event);
    // Drops what is queued: events of a session that has been left (session.cpp, Close).
    void DropEvents() { events.clear(); }
    void Disconnect();
    bool Connected() const { return connected; }
    const std::string& Room() const { return room; }
    const std::string& Url() const { return url; }
    uint32_t Connection() const { return connection; }
    const std::string& Error() const { return error; }
private:
    bool Request(const std::vector<uint8_t>& message, uint8_t answer, std::vector<uint8_t>& reply, unsigned ms = 8000);
    bool Handle(const std::vector<uint8_t>& message, uint8_t want, std::vector<uint8_t>* reply);
    WebSocket socket;
    bool connected = false;
    uint32_t connection = 0, nextRequest = 1;
    std::string url, room, error;
    std::deque<RelayEvent> events;
    // Datagrams beside the WebSocket (../free-direct/src/relay/RelayDatagram.hpp, the protocol's
    // "Datagrams"): when Welcome offers them, a UDP socket binds this program's address to its
    // connection; messages that need not arrive then go by UDP, where a lost packet loses only itself
    // instead of holding back everything behind it, as it does over TCP. Everything else, and
    // everything when UDP does not get through, stays on the WebSocket. Not in a browser (no UDP).
    struct Datagrams;
    std::unique_ptr<Datagrams> datagrams;
    void StartDatagrams(uint16_t port, const std::array<uint8_t, 16>& token);
    void ServiceDatagrams();
};
// Room IDs as the server writes them, read the way the server reads them (case, spaces, dashes,
// O for 0, I and L for 1 ignored): false if the text cannot be one.
bool RoomId(const std::string& typed, std::string& id);
// A room ID as the server writes it: groups of five, joined by dashes.
std::string RoomIdText(const std::string& id);
// Pasted text on the server screen: a link with ?server= and ?room= (the web page's,
// port/web/loader.js) gives both; otherwise a room ID alone (as RoomId reads it) or one word of a
// sentence that is a room ID. room is then RoomIdText's form, server only from a link. False if
// neither is in it.
bool Invite(const std::string& text, std::string& server, std::string& room);
} // namespace blupi::net
#endif

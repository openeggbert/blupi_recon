// Native replacement of src/network.cpp and games/sb2/src/network.cpp, compiled in each game's
// historical context (its def.h, decor.h, network.h; game/network_port.h names its wire rules). The
// DirectPlay public contract stays intact; UDP sessions do not interoperate with DirectPlay and never
// migrate hosts.
#include "def.h"
#include "network.h"
#include "decor.h"
#include "event.h"
#include "net/game_protocol.h"
#include "game/network_port.h"
#include "game/port_game.h"
#include "game/member_access.h"
#include "net/session.h"
#include "net/relay.h"
#include "net/websocket.h"
#include "ui/address_events.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

// The wire format is the historical in-memory layout of these structures, which the original code
// fills by field and by byte offset (event.cpp, decmove.cpp): little-endian, natural alignment, signed
// char (-fsigned-char, CMakeLists.txt). Windows, Linux and Android (x86-64, ARM64) all agree; a target
// that did not would fail here instead of speaking a different protocol.
static_assert(sizeof(short) == 2 && sizeof(int) == 4 && sizeof(DPID) == 4, "wire integer widths");
static_assert(char(-1) < 0, "the historical context needs signed char");
#if !defined(_WIN32) && !(defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#error "the native multiplayer wire format is little-endian"
#endif
static_assert(sizeof(NetMessage) == 8 && offsetof(NetMessage, data2) == 2 &&
              offsetof(NetMessage, data3) == 4 && offsetof(NetMessage, data4) == 6, "NetMessage layout");
static_assert(sizeof(NetPacket) == 60 && offsetof(NetPacket, data1) == 2 && offsetof(NetPacket, time) == 4 &&
              offsetof(NetPacket, blupiPosX) == 8 && offsetof(NetPacket, blupiChannel) == 16 &&
              offsetof(NetPacket, nbMessages) == 19 && offsetof(NetPacket, messages) == 20, "NetPacket layout");
static_assert(sizeof(NetPlayer) == 32 && offsetof(NetPlayer, dpid) == 4 && offsetof(NetPlayer, team) == 8 &&
              offsetof(NetPlayer, name) == 10, "NetPlayer layout (LOBBY carries four)");
#include <deque>
#include <map>
#include <memory>
#include <set>

extern CDecor* g_pDecor;
extern CEvent* g_pEvent;

// What every peer of a session must share, on any platform: the game, the edition and the
// multiplayer maps (SB1 data/world101..110.blp, SB2 world201..212.blp: the lobby's map choices),
// read through the game's file system exactly as the game will load them. FNV-1a, 64 bits; carried
// in every session envelope.
static uint64_t ContentFingerprint()
{
	uint64_t hash = 1469598103934665603ULL;
	auto mix = [&hash](const void* data, size_t size) {
		const uint8_t* bytes = static_cast<const uint8_t*>(data);
		for (size_t i = 0; i < size; i++) { hash ^= bytes[i]; hash *= 1099511628211ULL; }
	};
	const blupi::net::game::Rules& rules = PortNetworkRules();
	const std::string game = std::string(rules.game) + "/" + PortEdition();   // "sb1/blupi", ...
	mix(game.c_str(), game.size() + 1);
	for (int map = rules.worldBase + 1; map <= rules.worldBase + rules.maps; map++)
	{
		char name[32];
		snprintf(name, sizeof(name), "data\\world%.3d.blp", map);
		uint64_t size = 0;
		if (FILE* file = fopen(name, "rb"))
		{
			uint8_t buffer[4096];
			size_t n;
			while ((n = fread(buffer, 1, sizeof(buffer), file)) > 0) { mix(buffer, n); size += n; }
			fclose(file);
		}
		mix(&size, sizeof(size));
	}
	return hash;
}
namespace {
namespace wire = blupi::net::game;
namespace native = blupi::net;
#define NET_DECOR_MEMBERS(X) X(m_moveObject) X(m_decor) X(m_netBlupiPos) X(m_netBlupiIcon)
BLUPI_MEMBER_ACCESS(NetDecor, CDecor, NET_DECOR_MEMBERS)
struct State {
    native::Session session{PortNetworkRules().protocol,wire::Version};
    bool provider=false, started=false, terminal=false;
    // The provider chosen in the list: the local network, an address typed on the address screen
    // (Internet), or a Free Direct server and a room on it.
    enum class Kind { Lan, Address, Server } kind=Kind::Lan;
    uint64_t content=0;
    std::vector<native::Advertisement> listed;
    std::deque<native::Message> lobby,gameplay;
    std::set<std::string> logged;
    std::map<std::string,unsigned> rejected;
    std::set<uint32_t> departed;
};
std::map<CNetwork*,std::unique_ptr<State>> states;
// The server address entered on the port's address screen for the Internet provider.
bool internetSet=false;
native::Endpoint internetEndpoint;
// The Free Direct server provider's connection: one per program, kept while that provider stays
// chosen, so its room outlives the sessions played in it.
native::Relay relay;
long tick=0;
FILE* audit=nullptr;
State& Get(CNetwork* n) { return *states.at(n); }
void Note(const char* text) { fprintf(stderr,"network: %s\n",text); if (audit) { fprintf(audit,"%ld %s\n",tick,text); fflush(audit); } }
void Reject(State& s,const char* reason) {
    ++s.rejected[reason];
    if (s.logged.insert(reason).second) { char b[160]; snprintf(b,sizeof(b),"rejected %s",reason); Note(b); }
}
wire::Context Context(CNetwork* n,uint32_t origin,bool reliable,bool trusted=false) {
    State& s=Get(n); wire::Context c; c.rules=&PortNetworkRules();
    c.origin=origin; c.local=n->m_dpid; c.receiverHost=n->IsHost(); c.started=s.started;
    c.reliable=reliable; c.trusted=trusted;
    for (int i=0;i<4;++i) {
        const auto& p=n->m_players[i]; c.players[i]={p.bIsPresent!=0,p.ready!=0,p.dpid,p.team};
    }
    c.known=[&s](uint32_t id) { return s.session.Knows(id); };
    c.liveLift=[](int i) {
        if (!g_pDecor || i<0 || i>=200) return false;
        const auto& o=(g_pDecor->*NetDecor::m_moveObject)[i];
        return o.type!=0 && (o.posStart.x!=o.posEnd.x || o.posStart.y!=o.posEnd.y);
    };
    return c;
}
void Packet(const char* direction,const native::Message& m) {
    if (!audit) return;
    fprintf(audit,"%ld %s %u %u",tick,direction,m.origin,m.sequence);
    for (uint8_t b:m.bytes) fprintf(audit," %02x",unsigned(b));
    std::fputc('\n',audit); fflush(audit);
}
void Pump(CNetwork* n) {
    auto& s=Get(n); s.session.Service();
    native::Departure departure;
    while (s.session.Departed(departure)) {
        if (departure.hostLost) {
            // Host migration is unsupported: the star topology ends with its host. The game sees the
            // original departure paths, generated by this trusted layer (sequence 0): in the lobby the
            // host's LEAVE with host = TRUE (event.cpp closes the session), in play an ABANDON for the
            // host and every other remote member (decmove.cpp removes them; one player left wins).
            Note("host disconnected; host migration unsupported");
            s.terminal=true;
            if (!s.started) {
                native::Message m; m.origin=1; m.reliable=true; m.bytes.assign(12,0);
                m.bytes[0]=12; m.bytes[1]=3; m.bytes[4]=1; m.bytes[8]=1;
                s.lobby.push_back(std::move(m));
            } else for (const auto& p:n->m_players) {
                if (!p.bIsPresent || p.dpid==n->m_dpid || s.departed.count(p.dpid)) continue;
                native::Message m; m.origin=p.dpid; m.reliable=true; m.bytes={4,8,0,0};
                s.gameplay.push_back(std::move(m));
            }
        } else if (!s.departed.count(departure.member)) {
            native::Message m; m.origin=departure.member; m.reliable=true;
            if (s.started) m.bytes={4,8,0,0};
            else { m.bytes.resize(12); m.bytes[0]=12; m.bytes[1]=3; for (int i=0;i<4;++i) m.bytes[4+i]=uint8_t(m.origin>>(8*i)); }
            // Trusted service control retains identity until the original
            // LEAVE/ABANDON consumer removes the member from its game table.
            (s.started ? s.gameplay : s.lobby).push_back(std::move(m));
        }
    }
    native::Message m;
    while (s.session.Receive(m)) {
        auto kind=wire::Classify(m.bytes.data(),m.bytes.size());
        if (kind==wire::Class::Invalid) { Reject(s,"type/length"); continue; }
        auto& queue=kind==wire::Class::Lobby ? s.lobby : s.gameplay;
        if (queue.size()>=1024) { Note("adapter receive queue exceeded"); s.session.Close(); s.terminal=true; break; }
        queue.push_back(std::move(m));
    }
}
}
// The audit is a host file outside the game's virtual file system: the real fopen.
#pragma push_macro("fopen")
#undef fopen
void PortNetworkConfigure(const std::string& path) {
    if (audit) fclose(audit);
    audit=path.empty() ? nullptr : fopen(path.c_str(),"w");
}
#pragma pop_macro("fopen")
void PortNetworkTick(long frame) {
    tick=frame;
    for (auto& [n,state]:states) {
        Pump(n);
        if (audit && state->session.Active()) {
            fprintf(audit,"%ld state %u",tick,g_pEvent ? g_pEvent->GetPhase() : 0);
            for (const auto& p:n->m_players) if (p.bIsPresent)
                fprintf(audit," member=%u,%d,%d,%s",p.dpid,p.team,p.ready,p.name);
            std::fputc('\n',audit); fflush(audit);
        }
    }
}
CNetwork::CNetwork() {
    m_pDP=nullptr; m_dpid=0; m_bHost=FALSE;
    m_providers={0,nullptr}; m_sessions={0,nullptr}; m_unknown={0,nullptr};
    memset(m_players,0,sizeof(m_players));
    states.emplace(this,std::make_unique<State>());
}
CNetwork::~CNetwork() {
    Close(); FreeProviderList(); FreeSessionList(); FreeUnknownList(); states.erase(this);
    if (states.empty() && audit) { fclose(audit); audit=nullptr; }
}
// The providers, as DirectPlay listed its service providers: the local network (LAN
// discovery and the --net-join addresses); the Internet entry, which asks for a server
// address on the port's address screen as DirectPlay's TCP/IP provider asked in a dialog;
// and a Free Direct server with its rooms (port additions; port/docs/MULTIPLAYER.md).
// A browser has no UDP: there the server is the only one (port/docs/WEB.md).
namespace {
#ifdef __EMSCRIPTEN__
const State::Kind kProviders[]={State::Kind::Server};
#else
const State::Kind kProviders[]={State::Kind::Lan,State::Kind::Address,State::Kind::Server};
#endif
constexpr int kProviderCount=int(sizeof(kProviders)/sizeof(kProviders[0]));
const char* ProviderName(State::Kind kind) {
    static std::string text;
    switch (kind) {
    case State::Kind::Lan: return "Native network (UDP)";
    case State::Kind::Address: text=PortLanguageText(9025,"Internet (enter an address)"); return text.c_str();
    default: text=PortLanguageText(9030,"Internet through a Free Direct server"); return text.c_str();
    }
}
// The relay's compatibility identity: the server lets into a room only the kind of build that
// created it (game, edition, wire protocol and the multiplayer content of ContentFingerprint).
std::string Compat(uint64_t content) {
    const blupi::net::game::Rules& rules=PortNetworkRules();
    char text[160];
    const std::string edition=PortEdition();
    snprintf(text,sizeof(text),"blupi-sdl3/%s/%s/%u.%u/%016llx",rules.game,edition.c_str(),unsigned(rules.protocol),
             unsigned(wire::Version),(unsigned long long)content);
    return text;
}
}
BOOL CNetwork::EnumProviders() {
    FreeProviderList();
    m_providers.pList=(NamedGUID(*)[MAXSESSION])std::calloc(MAXSESSION,sizeof(NamedGUID));
    if (!m_providers.pList) return FALSE;
    m_providers.nb=kProviderCount;
    for (int i=0;i<kProviderCount;++i)
        snprintf((*m_providers.pList)[i].name,sizeof((*m_providers.pList)[i].name),"%s",ProviderName(kProviders[i]));
    return TRUE;
}
int CNetwork::GetNbProviders() { return m_providers.nb; }
char* CNetwork::GetProviderName(int i) { return i<0 || i>=m_providers.nb ? nullptr : (*m_providers.pList)[i].name; }
BOOL CNetwork::CreateProvider(int i) {
    if (i<0 || i>=kProviderCount || m_providers.nb!=kProviderCount) return FALSE;
    auto& s=Get(this); s.provider=true; s.kind=kProviders[i];
    s.content=ContentFingerprint(); s.session.SetContent(s.content);
    if (s.kind==State::Kind::Server) {
        s.session.UseRelay(&relay);
        PortServerRequest();
    } else {
        s.session.UseRelay(nullptr);
        relay.Disconnect();
        if (s.kind==State::Kind::Address) { internetSet=false; PortAddressRequest(); }
    }
    static const char* names[]={"Native network (UDP)","Internet (address)","Free Direct server"};
    char note[96]; snprintf(note,sizeof(note),"provider %s; content %016llx",names[int(s.kind)],(unsigned long long)s.content);
    Note(note); return TRUE;
}
const std::string& PortNetworkRoomFile() { return native::Configuration().roomFile; }
// The server screen's OK (address_screen.cpp): connects to the server unless already connected to
// it, then enters the room (an empty room creates one; its ID goes to created). An error message
// in English, or NULL.
const char* PortNetworkSetServer(const char* server,const char* room,std::string* created) {
    static std::string error;
    std::string url,id;
    if (!server || !native::ServerUrl(server,native::SecurePage(),url)) {
        error=PortLanguageText(9037,"This is not a server address."); return error.c_str();
    }
    const bool create=!room || !*room;
    if (!create && !native::RoomId(room,id)) { error=PortLanguageText(9038,"This is not a room ID."); return error.c_str(); }
    uint64_t content=0;
    for (auto& [n,state]:states) if (state->kind==State::Kind::Server) content=state->content;
    if (!relay.Connected() || relay.Url()!=url) {
        const char* password=getenv("BLUPI_NET_PASSWORD");
        if (!relay.Connect(url,password ? password : "",Compat(content))) { error=relay.Error(); Note(("server: "+error).c_str()); return error.c_str(); }
        Note(("server "+url).c_str());
    }
    if (create ? !relay.CreateRoom() : !relay.JoinRoom(id)) { error=relay.Error(); Note(("server: "+error).c_str()); return error.c_str(); }
    if (created) *created=create ? relay.Room() : std::string();
    Note(create ? "server room created" : "server room joined");
    const std::string& file=native::Configuration().roomFile;
    if (create && !file.empty()) {
        // The test supervisor's way to pass the room to the other instances: written whole, then renamed.
#pragma push_macro("fopen")
#undef fopen
        if (FILE* f=fopen((file+".tmp").c_str(),"w")) { fprintf(f,"%s\n",relay.Room().c_str()); fclose(f); std::rename((file+".tmp").c_str(),file.c_str()); }
#pragma pop_macro("fopen")
    }
    return nullptr;
}
bool PortNetworkSetAddress(const char* text) {
    native::Endpoint endpoint; endpoint.port=native::Configuration().port;   // the game port unless given
    if (!text || !native::ParseEndpoint(text,endpoint)) return false;
    internetEndpoint=endpoint; internetSet=true;
    char note[160]; snprintf(note,sizeof(note),"Internet address %s:%u",endpoint.host.c_str(),unsigned(endpoint.port)); Note(note);
    return true;
}
void CNetwork::FreeProviderList() { std::free(m_providers.pList); m_providers={0,nullptr}; }
BOOL CNetwork::EnumSessions() {
    FreeSessionList(); auto& s=Get(this); if (!s.provider) return FALSE;
    if (s.kind==State::Kind::Server) {
        // The sessions in the server's room; nothing before the server screen has entered one.
        if (relay.Connected()) s.listed=s.session.List();
        m_sessions.pList=(NamedGUID(*)[MAXSESSION])std::calloc(MAXSESSION,sizeof(NamedGUID));
        if (!m_sessions.pList) return FALSE;
        for (const auto& ad:s.listed) {
            if (m_sessions.nb>=MAXSESSION) break;
            snprintf((*m_sessions.pList)[m_sessions.nb++].name,100,"%s",ad.name.c_str());
        }
        Note(m_sessions.nb ? "enumerated session" : "enumerated no sessions"); return TRUE;
    }
    auto endpoints=native::Configuration().endpoints;
    endpoints.insert(endpoints.begin(),{"127.0.0.1",native::Configuration().port});
    if (s.kind==State::Kind::Address && internetSet) endpoints.push_back(internetEndpoint);
    // Sessions on the LAN (discovery) and at the configured addresses (--net-join, this machine),
    // each listed once.
    unsigned incompatible=0;
    s.listed=s.session.Discover(native::Configuration().port,300,&incompatible);
    for (auto& ad:s.session.Probe(endpoints)) {
        bool known=false;
        for (const auto& other:s.listed) known=known || other.epoch==ad.epoch;
        if (!known) s.listed.push_back(std::move(ad));
    }
    if (incompatible) Note("ignored a session of another edition or with other multiplayer maps");
    m_sessions.pList=(NamedGUID(*)[MAXSESSION])std::calloc(MAXSESSION,sizeof(NamedGUID));
    if (!m_sessions.pList) return FALSE;
    for (const auto& ad:s.listed) {
        if (m_sessions.nb>=MAXSESSION) break;
        snprintf((*m_sessions.pList)[m_sessions.nb++].name,100,"%s",ad.name.c_str());
    }
    Note(m_sessions.nb ? "enumerated session" : "enumerated no sessions"); return TRUE;
}
int CNetwork::GetNbSessions() { return m_sessions.nb; }
char* CNetwork::GetSessionName(int i) { return i<0 || i>=m_sessions.nb ? nullptr : (*m_sessions.pList)[i].name; }
BOOL CNetwork::JoinSession(int i,char* player) {
    auto& s=Get(this); if (!s.provider || !player || i<0 || i>=m_sessions.nb) return FALSE;
    if (!s.session.Join(s.listed[i],player)) { Note(s.session.Error().c_str()); return FALSE; }
    m_dpid=s.session.Id(); m_bHost=FALSE; s.started=false; s.terminal=false; s.departed.clear(); Note("joined session"); return TRUE;
}
void CNetwork::FreeSessionList() { std::free(m_sessions.pList); m_sessions={0,nullptr}; Get(this).listed.clear(); }
BOOL CNetwork::CreateSession(char* name,char* player) {
    auto& s=Get(this); if (!s.provider || !name || !player) return FALSE;
    if (!s.session.Create(native::Configuration().port,name,player)) { Note(s.session.Error().c_str()); return FALSE; }
    m_dpid=s.session.Id(); m_bHost=TRUE; s.started=false; s.terminal=false; s.departed.clear(); Note("created session"); return TRUE;
}
BOOL CNetwork::Send(LPVOID data,DWORD size,DWORD flags) {
    auto& s=Get(this); if (!s.session.Active() || !data || size>200) return FALSE;
    native::Message m; m.origin=m_dpid; m.reliable=flags!=0;
    m.bytes.assign(static_cast<uint8_t*>(data),static_cast<uint8_t*>(data)+size);
    auto c=Context(this,m_dpid,m.reliable);
    if (m.bytes.size()==28 && m.bytes[1]==2) c.receiverHost=true;
    if (const char* reason=wire::Validate(m.bytes.data(),m.bytes.size(),c)) { Reject(s,reason); return FALSE; }
    wire::Canonicalise(m.bytes.data(),m.bytes.size()); Packet("send",m);
    if (!s.session.Send(m.bytes.data(),m.bytes.size(),m.reliable)) return FALSE;
    if (size==2 && m.bytes[1]==7) { s.started=true; s.session.Start(); Note("START"); }
    if ((size==12 && m.bytes[1]==3) || (size==4 && (m.bytes[1]==8 || m.bytes[1]==9))) s.departed.insert(m_dpid);
    return TRUE;
}
BOOL CNetwork::Receive(LPVOID dest,DWORD size,LPDWORD player) {
    if (!dest || !player || (size!=200 && size!=60)) return FALSE;
    auto& s=Get(this); Pump(this);
    auto& queue=size==200 ? s.lobby : s.gameplay;
    while (!queue.empty()) {
        auto m=std::move(queue.front()); queue.pop_front();
        const bool trusted=m.sequence==0; // Only local service control has sequence zero.
        auto c=Context(this,m.origin,m.reliable,trusted);
        if (const char* reason=wire::Validate(m.bytes.data(),m.bytes.size(),c)) { Reject(s,reason); continue; }
        memset(dest,0,size); memcpy(dest,m.bytes.data(),m.bytes.size());
        *player=DWORD(-1);
        for (int i=0;i<4;++i) if (m_players[i].bIsPresent && m_players[i].dpid==m.origin) *player=i;
        if (m.bytes[1]==7 && size==200) { s.started=true; s.session.Start(); }
        if ((m.bytes[1]==3 && size==200) || (size==60 && (m.bytes[1]==8 || m.bytes[1]==9))) s.departed.insert(m.origin);
        Packet("receive",m); return TRUE;
    }
    return FALSE;
}
BOOL CNetwork::Close() {
    auto& s=Get(this); s.session.Close(); s.lobby.clear(); s.gameplay.clear(); s.started=false; s.departed.clear();
    // Cached IsHost/DPID remain valid through the original caller's cleanup.
    return TRUE;
}
void CNetwork::FreeUnknownList() { std::free(m_unknown.pList); m_unknown={0,nullptr}; }
BOOL CNetwork::IsHost() { return m_bHost; }
void TraceErrorDP(HRESULT) {}

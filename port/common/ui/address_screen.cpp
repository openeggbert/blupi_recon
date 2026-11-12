// Server address screen (port addition, port/docs/MULTIPLAYER.md): the provider list's
// Internet entry asks for the host to search, as DirectPlay's TCP/IP service provider
// did with a dialog of its own while the game enumerated sessions (the game released
// the mouse around that call: CEvent::NetEnumSessions). It is a presentation-layer
// screen like the Display options (options_screen.cpp): drawn over the session screen's
// background, with the game's buttons, fonts and click sound. While it is open the game
// gets no clicks or keys; on OK it hands the address to the network adapter and posts
// the session screen's own refresh, as its refresh button does. The game itself keeps
// running untouched.
//
// The Free Direct server entry uses the same screen with two fields, the server and a
// room on it (empty: a new room). After a new room it shows the room's ID to pass on.

#include "def.h"
#include "pixmap.h"
#include "sound.h"
#include "decor.h"
#include "button.h"
#include "text.h"
#include "misc.h"

#include "game/port_game.h"
#include "platform/host.h"
#include "ui/address_events.h"
#include "ui/settings.h"

#include <cstdio>
#include <cstring>
#include <string>

bool PortNetworkSetAddress(const char* text);   // common/net/cnetwork.cpp
const char* PortNetworkSetServer(const char* server, const char* room, std::string* created);
const std::string& PortNetworkRoomFile();
unsigned PortNetworkServerPort();   // the port a plain address gets (443 on an https:// page)

namespace
{
enum : UINT
{
	MSG_OK = WM_USER + 0x7F40,
	MSG_CANCEL,
	MSG_LAST,
};

// Button sprites and places of the session screen's own Join and Cancel buttons
// (the phase table of src/event.cpp: icons 0x2d and 0x24 at 0x78/0x1e5, 0x183).
constexpr int kIconOk = 0x12, kIconCancel = 0x24;
constexpr int kMaxLength = 63;
// The server screen's fields (their lines; a click there selects the field).
constexpr int kServerY = 0x82, kRoomY = 0xbe, kFieldHeight = 20;

enum class Mode { Address, Server, Room };

CPixmap*      s_pixmap = NULL;
CSound*       s_sound = NULL;
PortSettings* s_settings = NULL;
void        (*s_save)() = NULL;
bool          s_requested = false;
bool          s_serverRequested = false;
bool          s_open = false;
bool          s_invalid = false;   // the last OK held no usable address
Mode          s_mode = Mode::Address;
std::string   s_text;              // the address (Address mode)
std::string   s_server, s_room;    // the server and the room (Server mode)
int           s_focus = 0;         // Server mode: 0 the server, 1 the room
std::string   s_error;             // Server mode: why the last OK did not work
std::string   s_created;           // Room mode: the new room's ID
CButton       s_ok, s_cancel;

void Text(int x, int y, unsigned id, const char* english, int font)
{
	std::string text = PortLanguageText(id, english);
	DrawTextLeft(s_pixmap, POINT{x, y}, &text[0], font);
}

void Sprite(POINT pos, bool overButton)
{
	int sprite = overButton ? SPRITE_POINTER : SPRITE_ARROW;
	s_pixmap->SetMousePosSprite(pos, sprite, FALSE);
	ChangeSprite(sprite);
}

void Close()
{
	s_open = false;
	s_invalid = false;
	s_error.clear();
}

void Saved()
{
	if (s_save) s_save();
}

void Refresh()
{
	Close();
	PostMessage(NULL, WM_PHASE_DP_REFRESH, 0, 0);   // the session screen's refresh button
}

void ConfirmAddress()
{
	// An empty field searches the local network only, as DirectPlay's dialog did.
	if (!s_text.empty() && !PortNetworkSetAddress(s_text.c_str()))
	{
		s_invalid = true;
		return;
	}
	if (s_settings != NULL && s_settings->netAddress != s_text)
	{
		s_settings->netAddress = s_text;
		Saved();
	}
	Refresh();
}

void ConfirmServer()
{
	std::string created;
	const char* error = PortNetworkSetServer(s_server.c_str(), s_room.c_str(), &created);
	if (error != NULL)
	{
		s_error = error;
		return;
	}
	if (s_settings != NULL)
	{
		s_settings->netServer = s_server;
		s_settings->netRoom = created.empty() ? s_room : created;
		Saved();
	}
	if (created.empty())
	{
		Refresh();
		return;
	}
	s_created = created;
	s_room = created;
	s_error.clear();
	s_mode = Mode::Room;
	blupi::host::share_room(created.c_str(), s_server.c_str());
}

void Confirm()
{
	if (s_mode == Mode::Address) ConfirmAddress();
	else if (s_mode == Mode::Server) ConfirmServer();
	else Refresh();
}

std::string& Field()
{
	if (s_mode == Mode::Address) return s_text;
	return s_focus == 0 ? s_server : s_room;
}

bool Allowed(int c)
{
	const bool alnum = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
	if (s_mode == Mode::Server && s_focus == 1) return alnum || c == '-';   // a room ID
	// Host names, IPv4 addresses and a port; for a server also a ws:// or wss:// URL and its path.
	return alnum || c == '.' || c == '-' || c == ':' || (s_mode == Mode::Server && c == '/');
}

// The room file of --net-room-file (tests): a room another instance created.
std::string RoomFromFile()
{
	const std::string& path = PortNetworkRoomFile();
	if (path.empty()) return std::string();
#pragma push_macro("fopen")
#undef fopen
	FILE* file = fopen(path.c_str(), "r");
#pragma pop_macro("fopen")
	if (file == NULL) return std::string();
	char line[64] = {0};
	if (fgets(line, sizeof(line), file) == NULL) line[0] = 0;
	fclose(file);
	std::string room(line);
	while (!room.empty() && (room.back() == '\n' || room.back() == '\r' || room.back() == ' ')) room.pop_back();
	return room;
}
}  // namespace

void PortAddressInit(CPixmap* pixmap, CSound* sound, PortSettings* settings, void (*save)())
{
	s_pixmap = pixmap;
	s_sound = sound;
	s_settings = settings;
	s_save = save;
	int icon = kIconOk;
	s_ok.Create(NULL, pixmap, sound, POINT{0x78, 0x183}, 0, FALSE, MSG_OK);
	s_ok.SetIconMenu(&icon, 1);
	icon = kIconCancel;
	s_cancel.Create(NULL, pixmap, sound, POINT{0x1e5, 0x183}, 0, FALSE, MSG_CANCEL);
	s_cancel.SetIconMenu(&icon, 1);
}

void PortAddressRequest()
{
	s_requested = true;
	s_serverRequested = false;
}

void PortServerRequest()
{
	s_serverRequested = true;
	s_requested = false;
}

void PortAddressPhase(unsigned int phase)
{
	if (phase == WM_PHASE_SESSION)
	{
		if ((s_requested || s_serverRequested) && s_pixmap != NULL)
		{
			s_mode = s_requested ? Mode::Address : Mode::Server;
			s_requested = s_serverRequested = false;
			s_open = true;
			s_invalid = false;
			s_error.clear();
			s_text = s_settings != NULL ? s_settings->netAddress : std::string();
			s_server = s_settings != NULL ? s_settings->netServer : std::string();
			const std::string fromFile = RoomFromFile();
			s_room = !fromFile.empty() ? fromFile : s_settings != NULL ? s_settings->netRoom : std::string();
			s_focus = s_server.empty() ? 0 : 1;
		}
		return;
	}
	// Any other screen: the request is used up (the provider list's DP_DOSERVICE goes
	// straight to the session screen) and the screen is closed.
	if (phase != WM_PHASE_SERVICE) s_requested = s_serverRequested = false;
	Close();
}

bool PortAddressOpen()
{
	return s_open;
}

bool PortAddressMouse(UINT message, WPARAM wParam, LPARAM lParam)
{
	if (!s_open) return false;
	const POINT pos = ConvLongToPos(lParam);
	s_ok.TreatEvent(message, wParam, lParam);
	if (s_mode != Mode::Room) s_cancel.TreatEvent(message, wParam, lParam);
	if (s_mode == Mode::Server && message == WM_LBUTTONDOWN && pos.x >= 0x78 && pos.x < LXIMAGE - 0x78)
	{
		if (pos.y >= kServerY - 4 && pos.y < kServerY + kFieldHeight) s_focus = 0;
		if (pos.y >= kRoomY - 4 && pos.y < kRoomY + kFieldHeight) s_focus = 1;
	}
	Sprite(pos, s_ok.MouseOnButton(pos) || (s_mode != Mode::Room && s_cancel.MouseOnButton(pos)));
	return true;
}

bool PortAddressKey(UINT message, WPARAM wParam)
{
	if (!s_open) return false;
	if (message == WM_KEYDOWN)
	{
		if (wParam == VK_RETURN) Confirm();
		else if (wParam == VK_ESCAPE) { if (s_mode == Mode::Room) Refresh(); else Close(); }
		else if (s_mode == Mode::Server && (wParam == VK_TAB || wParam == VK_UP || wParam == VK_DOWN))
			s_focus = 1 - s_focus;
		else if (wParam == VK_BACK && s_mode != Mode::Room && !Field().empty())
		{
			Field().pop_back();
			s_invalid = false;
			s_error.clear();
		}
	}
	else if (message == WM_CHAR && s_mode != Mode::Room)
	{
		const int c = (int)(unsigned char)wParam;
		if (Allowed(c) && (int)Field().size() < kMaxLength)
		{
			Field() += (char)c;
			s_invalid = false;
			s_error.clear();
		}
	}
	return true;   // no keys for the game's screen underneath
}

bool PortAddressCommand(UINT message)
{
	if (message < MSG_OK || message >= MSG_LAST) return false;
	if (!s_open) return true;
	if (message == MSG_OK) Confirm();
	else if (s_mode != Mode::Room) Close();
	return true;
}

namespace
{
void Centred(int y, const std::string& text, int font)
{
	std::string t = text;
	DrawTextLeft(s_pixmap, POINT{LXIMAGE / 2 - GetTextWidth(&t[0], 0) / 2, y}, &t[0], font);
}

void DrawAddress()
{
	Centred(36, PortLanguageText(9020, "Internet game"), FONTGOLD);
	Text(0x78, 0x6e, 9021, "Address of the computer that created the game:", FONTWHITE);
	std::string field = s_text + "_";
	DrawTextLeft(s_pixmap, POINT{0x78 + 20, 0x96}, &field[0], FONTWHITE);
	Text(0x78, 0xd2, 9022, "A name or a number, with :port if it is not 28640.", FONTLITTLE);
	Text(0x78, 0xe6, 9023, "Leave it empty to search the local network only.", FONTLITTLE);
	if (s_invalid) Text(0x78, 0x10e, 9024, "This is not an address.", FONTRED);
	s_ok.Draw();
	s_cancel.Draw();
}

void DrawServer()
{
	Centred(36, PortLanguageText(9031, "Internet game through a server"), FONTGOLD);
	Text(0x78, kServerY - 20, 9032, "Free Direct server:", FONTWHITE);
	std::string server = s_server + (s_focus == 0 ? "_" : "");
	DrawTextLeft(s_pixmap, POINT{0x78 + 20, kServerY}, &server[0], FONTWHITE);
	Text(0x78, kRoomY - 20, 9033, "Room (empty for a new one):", FONTWHITE);
	std::string room = s_room + (s_focus == 1 ? "_" : "");
	DrawTextLeft(s_pixmap, POINT{0x78 + 20, kRoomY}, &room[0], FONTWHITE);
	// The text is a language pack's: its "{port}" is replaced, it is never a format string.
	std::string hint = PortLanguageText(9034, "A name or a number, with :port if it is not {port}.");
	const size_t at = hint.find("{port}");
	if (at != std::string::npos) hint.replace(at, 6, std::to_string(PortNetworkServerPort()));
	DrawTextLeft(s_pixmap, POINT{0x78, 0xe6}, &hint[0], FONTLITTLE);
	Text(0x78, 0xfa, 9035, "To join friends, type the ID of the room they created.", FONTLITTLE);
	if (!s_error.empty())
	{
		std::string error = s_error;
		DrawTextLeft(s_pixmap, POINT{0x78, 0x116}, &error[0], FONTRED);
	}
	s_ok.Draw();
	s_cancel.Draw();
}

void DrawRoom()
{
	Centred(36, PortLanguageText(9036, "New room"), FONTGOLD);
	Text(0x78, 0x6e, 9039, "Give this room ID to the other players:", FONTWHITE);
	Centred(0x96, s_created, FONTGOLD);
	Text(0x78, 0xd2, 9040, "It is on the clipboard. They choose the same server", FONTLITTLE);
	Text(0x78, 0xe6, 9041, "and type the ID; the room's games show in the list.", FONTLITTLE);
	s_ok.Draw();
}
}  // namespace

void PortAddressDraw()
{
	if (!s_open) return;
	if (s_mode == Mode::Address) DrawAddress();
	else if (s_mode == Mode::Server) DrawServer();
	else DrawRoom();
}

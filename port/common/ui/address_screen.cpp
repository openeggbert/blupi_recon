// Server address screen (port addition, port/docs/MULTIPLAYER.md): the provider list's
// Internet entry asks for the host to search, as DirectPlay's TCP/IP service provider
// did with a dialog of its own while the game enumerated sessions (the game released
// the mouse around that call: CEvent::NetEnumSessions). It is a presentation-layer
// screen like the Display options (options_screen.cpp): drawn over the session screen's
// background, with the game's buttons, fonts and click sound. While it is open the game
// gets no clicks or keys; on OK it hands the address to the network adapter and posts
// the session screen's own refresh, as its refresh button does. The game itself keeps
// running untouched.

#include "def.h"
#include "pixmap.h"
#include "sound.h"
#include "decor.h"
#include "button.h"
#include "text.h"
#include "misc.h"

#include "game/port_game.h"
#include "ui/address_events.h"
#include "ui/settings.h"

#include <cstring>
#include <string>

bool PortNetworkSetAddress(const char* text);   // common/net/cnetwork.cpp

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

CPixmap*      s_pixmap = NULL;
CSound*       s_sound = NULL;
PortSettings* s_settings = NULL;
void        (*s_save)() = NULL;
bool          s_requested = false;
bool          s_open = false;
bool          s_invalid = false;   // the last OK held no usable address
std::string   s_text;
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
}

void Confirm()
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
		if (s_save) s_save();
	}
	Close();
	PostMessage(NULL, WM_PHASE_DP_REFRESH, 0, 0);   // the session screen's refresh button
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
}

void PortAddressPhase(unsigned int phase)
{
	if (phase == WM_PHASE_SESSION)
	{
		if (s_requested && s_pixmap != NULL)
		{
			s_requested = false;
			s_open = true;
			s_invalid = false;
			s_text = s_settings != NULL ? s_settings->netAddress : std::string();
		}
		return;
	}
	// Any other screen: the request is used up (the provider list's DP_DOSERVICE goes
	// straight to the session screen) and the screen is closed.
	if (phase != WM_PHASE_SERVICE) s_requested = false;
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
	s_cancel.TreatEvent(message, wParam, lParam);
	Sprite(pos, s_ok.MouseOnButton(pos) || s_cancel.MouseOnButton(pos));
	return true;
}

bool PortAddressKey(UINT message, WPARAM wParam)
{
	if (!s_open) return false;
	if (message == WM_KEYDOWN)
	{
		if (wParam == VK_RETURN) Confirm();
		else if (wParam == VK_ESCAPE) Close();
		else if (wParam == VK_BACK && !s_text.empty()) { s_text.pop_back(); s_invalid = false; }
	}
	else if (message == WM_CHAR)
	{
		// Host names, IPv4 addresses and a port: letters, digits, '.', '-', ':'.
		const int c = (int)(unsigned char)wParam;
		const bool allowed = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
							 c == '.' || c == '-' || c == ':';
		if (allowed && (int)s_text.size() < kMaxLength) { s_text += (char)c; s_invalid = false; }
	}
	return true;   // no keys for the game's screen underneath
}

bool PortAddressCommand(UINT message)
{
	if (message < MSG_OK || message >= MSG_LAST) return false;
	if (!s_open) return true;
	if (message == MSG_OK) Confirm();
	else Close();
	return true;
}

void PortAddressDraw()
{
	if (!s_open) return;
	std::string title = PortLanguageText(9020, "Internet game");
	DrawTextLeft(s_pixmap, POINT{LXIMAGE / 2 - GetTextWidth(&title[0], 0) / 2, 36}, &title[0], FONTGOLD);
	Text(0x78, 0x6e, 9021, "Address of the computer that created the game:", FONTWHITE);
	std::string field = s_text + "_";
	DrawTextLeft(s_pixmap, POINT{0x78 + 20, 0x96}, &field[0], FONTWHITE);
	Text(0x78, 0xd2, 9022, "A name or a number, with :port if it is not 28640.", FONTLITTLE);
	Text(0x78, 0xe6, 9023, "Leave it empty to search the local network only.", FONTLITTLE);
	if (s_invalid) Text(0x78, 0x10e, 9024, "This is not an address.", FONTRED);
	s_ok.Draw();
	s_cancel.Draw();
}

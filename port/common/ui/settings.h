// Port settings: the presentation options the original does not have
// (port/docs/ROADMAP.md stage 9). Stored as a game-specific JSON file in the user
// directory, changed on the "Display options" screen (common/ui/options_screen.cpp)
// or overridden for one run by command-line options. None of them changes the
// simulation. The file also remembers where the game's files are.
#ifndef BLUPI_GAME_SETTINGS_H
#define BLUPI_GAME_SETTINGS_H

#include <string>

struct PortSettings
{
	bool widescreen = false;    // view width from the window shape, 4:3 to 21:9
	bool interpolate = false;   // smooth motion between ticks
	bool displayRate = true;    // present at the display rate (false: once per tick)
	bool fullscreen = false;
	bool integerScale = false;  // scale by whole factors only
	int  scale = 2;             // initial window size, times 640x480
	std::string dataFolder;     // the game's files, as chosen on the first run (desktop)
	std::string language;       // language pack code; empty: the installed language
	// The gamepad as the keyboard while the game is set to keyboard input (README,
	// "Gamepads"): each button's action, one of jump, fire, pause, escape, enter,
	// left, right, up, down or none; with gamepadArrows the d-pad and the left
	// stick are the arrow keys.
	std::string gamepadA = "jump", gamepadB = "fire", gamepadX = "fire", gamepadY = "none";
	std::string gamepadStart = "pause", gamepadBack = "escape";
	std::string gamepadLeftShoulder = "none", gamepadRightShoulder = "none";
	bool gamepadArrows = true;
	std::string netAddress;     // the last server address of the Internet provider (multiplayer)
	std::string netServer;      // the last Free Direct server (multiplayer through a server)
	std::string netRoom;        // and the room last used there
	// Higher-resolution images (port/docs/HD.md): the presentation at hdScale times 640x480 (1: off),
	// with the images of the pack folder hdPack where it has them (empty: the originals enlarged).
	int  hdScale = 1;
	std::string hdPack;
};

namespace blupi::host { struct GamepadKeys; }
// The virtual keys of the gamepad settings (an unknown action is none).
void PortGamepadKeys(const PortSettings& settings, blupi::host::GamepadKeys& keys);

// The settings of one run: everything the file holds (window, language, gamepad keys, server
// address, data folder), with the values this run's command-line options set taken from given.
struct PortSettingsGiven
{
	bool widescreen = false, interpolate = false, displayRate = false, fullscreen = false;
	bool scale = false, integerScale = false, language = false;
	bool netServer = false, netRoom = false;
	bool hdScale = false, hdPack = false;
};
PortSettings PortSettingsMerge(const PortSettings& file, const PortSettings& given, const PortSettingsGiven& set);

// path is a host path (the user directory). Missing file or keys keep the
// values already in settings.
bool PortSettingsLoad(const char* path, PortSettings& settings);
bool PortSettingsSave(const char* path, const PortSettings& settings);

#endif

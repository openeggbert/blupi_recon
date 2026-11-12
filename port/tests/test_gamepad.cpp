// The winmm joystick calls the game makes (CEvent::ReadInput, the settings
// screen's device list) answered from an SDL gamepad: a virtual gamepad stands
// in for real hardware.

#include "windows.h"
#include "mmsystem.h"

#include "platform/host.h"

#include <SDL3/SDL.h>

#include <stdio.h>
#include <string.h>

#include <vector>

static int failures = 0;

static void check(bool ok, const char* what)
{
	printf("%s %s\n", ok ? "ok  " : "FAIL", what);
	if (!ok) failures++;
}

// Lets SDL deliver the virtual device's events to the host.
static void pump(blupi::host::Event* last = NULL)
{
	blupi::host::Event e;
	SDL_UpdateJoysticks();
	while (blupi::host::poll_event(e))
	{
		if (last) *last = e;
	}
}

int main()
{
	blupi::host::Options options;
	options.headless = true;
	options.gamepads = true;
	if (!blupi::host::init("test", 640, 480, options)) { printf("host init failed\n"); return 1; }

	check(joyGetNumDevs() == 0, "no device before a gamepad is connected");

	SDL_VirtualJoystickDesc desc;
	SDL_INIT_INTERFACE(&desc);
	desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
	desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
	desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
	desc.name = "Virtual Pad";
	SDL_JoystickID id = SDL_AttachVirtualJoystick(&desc);
	check(id != 0, "virtual gamepad attached");
	SDL_Joystick* joy = SDL_OpenJoystick(id);
	pump();

	check(joyGetNumDevs() == 1, "one device listed");
	JOYCAPS caps;
	check(joyGetDevCaps(0, &caps, sizeof(caps)) == JOYERR_NOERROR, "device 0 has capabilities");
	check(strcmp(caps.szPname, "Virtual Pad") == 0, "device name as the settings screen shows it");
	check(joyGetDevCaps(1, &caps, sizeof(caps)) != JOYERR_NOERROR, "no device 1");

	JOYINFOEX info;
	memset(&info, 0, sizeof(info));
	check(joyGetPosEx(0, &info) == JOYERR_NOERROR, "position of device 0");
	check(info.dwXpos >= 16384 && info.dwXpos <= 49152 && info.dwYpos >= 16384 && info.dwYpos <= 49152,
		  "centred stick reads as no direction (game thresholds 16384/49152)");
	check(info.dwButtons == 0, "no button");

	// Stick left and up, then the d-pad right and down.
	SDL_SetJoystickVirtualAxis(joy, SDL_GAMEPAD_AXIS_LEFTX, -32768);
	SDL_SetJoystickVirtualAxis(joy, SDL_GAMEPAD_AXIS_LEFTY, -32768);
	pump();
	joyGetPosEx(0, &info);
	check(info.dwXpos < 16384 && info.dwYpos < 16384, "stick left/up: below 16384");
	SDL_SetJoystickVirtualAxis(joy, SDL_GAMEPAD_AXIS_LEFTX, 0);
	SDL_SetJoystickVirtualAxis(joy, SDL_GAMEPAD_AXIS_LEFTY, 0);
	SDL_SetJoystickVirtualButton(joy, SDL_GAMEPAD_BUTTON_DPAD_RIGHT, true);
	SDL_SetJoystickVirtualButton(joy, SDL_GAMEPAD_BUTTON_DPAD_DOWN, true);
	pump();
	joyGetPosEx(0, &info);
	check(info.dwXpos > 49152 && info.dwYpos > 49152, "d-pad right/down: above 49152");
	SDL_SetJoystickVirtualButton(joy, SDL_GAMEPAD_BUTTON_DPAD_RIGHT, false);
	SDL_SetJoystickVirtualButton(joy, SDL_GAMEPAD_BUTTON_DPAD_DOWN, false);

	// Face buttons are joystick buttons 1 to 4.
	const struct { int button; DWORD joy; const char* what; } faces[] =
	{
		{ SDL_GAMEPAD_BUTTON_SOUTH, JOY_BUTTON1, "south (A) is button 1 (jump)" },
		{ SDL_GAMEPAD_BUTTON_EAST,  JOY_BUTTON2, "east (B) is button 2" },
		{ SDL_GAMEPAD_BUTTON_WEST,  JOY_BUTTON3, "west (X) is button 3" },
		{ SDL_GAMEPAD_BUTTON_NORTH, JOY_BUTTON4, "north (Y) is button 4 (fire)" },
	};
	for (const auto& f : faces)
	{
		SDL_SetJoystickVirtualButton(joy, f.button, true);
		pump();
		joyGetPosEx(0, &info);
		check(info.dwButtons == f.joy && info.dwButtonNumber == 1, f.what);
		SDL_SetJoystickVirtualButton(joy, f.button, false);
		pump();
	}

	// Start is the Escape key.
	blupi::host::Event last;
	last.type = blupi::host::EventType::Quit;
	SDL_SetJoystickVirtualButton(joy, SDL_GAMEPAD_BUTTON_START, true);
	pump(&last);
	check(last.type == blupi::host::EventType::KeyDown && last.key == 0x1B, "Start presses Escape");
	SDL_SetJoystickVirtualButton(joy, SDL_GAMEPAD_BUTTON_START, false);
	pump(&last);
	check(last.type == blupi::host::EventType::KeyUp && last.key == 0x1B, "Start released releases Escape");

	// The gamepad as the keyboard (set_gamepad_keys): the default mapping of the settings.
	blupi::host::GamepadKeys keys;
	keys.south = 0x11; keys.east = 0x20; keys.west = 0x20; keys.start = 0x13; keys.back = 0x1B;
	blupi::host::set_gamepad_keys(&keys);
	auto events = [&]() {
		std::vector<blupi::host::Event> list;
		blupi::host::Event e;
		SDL_UpdateJoysticks();
		while (blupi::host::poll_event(e)) list.push_back(e);
		return list;
	};
	auto only = [](const std::vector<blupi::host::Event>& list, blupi::host::EventType type, int key) {
		return list.size() == 1 && list[0].type == type && list[0].key == key;
	};
	using T = blupi::host::EventType;
	SDL_SetJoystickVirtualButton(joy, SDL_GAMEPAD_BUTTON_SOUTH, true);
	check(only(events(), T::KeyDown, 0x11), "keyboard mode: A presses Ctrl (jump)");
	SDL_SetJoystickVirtualButton(joy, SDL_GAMEPAD_BUTTON_SOUTH, false);
	check(only(events(), T::KeyUp, 0x11), "keyboard mode: A released releases Ctrl");
	joyGetPosEx(0, &info);
	check(info.dwButtons == 0, "the joystick reading is unchanged by the keyboard mode");

	// B and X both fire: Space is pressed once and released with the last.
	SDL_SetJoystickVirtualButton(joy, SDL_GAMEPAD_BUTTON_EAST, true);
	check(only(events(), T::KeyDown, 0x20), "B presses Space (fire)");
	SDL_SetJoystickVirtualButton(joy, SDL_GAMEPAD_BUTTON_WEST, true);
	check(events().empty(), "X while B holds Space: no second press");
	SDL_SetJoystickVirtualButton(joy, SDL_GAMEPAD_BUTTON_EAST, false);
	check(events().empty(), "B released while X holds Space: still held");
	SDL_SetJoystickVirtualButton(joy, SDL_GAMEPAD_BUTTON_WEST, false);
	check(only(events(), T::KeyUp, 0x20), "the last of B and X released releases Space");

	// The d-pad and the stick as the arrow keys, sharing them like the buttons.
	SDL_SetJoystickVirtualButton(joy, SDL_GAMEPAD_BUTTON_DPAD_LEFT, true);
	check(only(events(), T::KeyDown, 0x25), "d-pad left presses the left arrow");
	SDL_SetJoystickVirtualAxis(joy, SDL_GAMEPAD_AXIS_LEFTX, -32768);
	check(events().empty(), "stick left while the d-pad holds the left arrow: no second press");
	SDL_SetJoystickVirtualButton(joy, SDL_GAMEPAD_BUTTON_DPAD_LEFT, false);
	check(events().empty(), "d-pad released while the stick holds left: still held");
	SDL_SetJoystickVirtualAxis(joy, SDL_GAMEPAD_AXIS_LEFTX, -14000);
	check(events().empty(), "stick between the thresholds keeps the arrow (hysteresis)");
	SDL_SetJoystickVirtualAxis(joy, SDL_GAMEPAD_AXIS_LEFTX, 0);
	check(only(events(), T::KeyUp, 0x25), "stick centred releases the left arrow");
	SDL_SetJoystickVirtualAxis(joy, SDL_GAMEPAD_AXIS_LEFTY, 32767);
	check(only(events(), T::KeyDown, 0x28), "stick down presses the down arrow");
	SDL_SetJoystickVirtualAxis(joy, SDL_GAMEPAD_AXIS_LEFTY, 0);
	check(only(events(), T::KeyUp, 0x28), "stick centred releases the down arrow");

	// Start pauses and Back leaves in this mode.
	SDL_SetJoystickVirtualButton(joy, SDL_GAMEPAD_BUTTON_START, true);
	check(only(events(), T::KeyDown, 0x13), "keyboard mode: Start presses Pause");
	SDL_SetJoystickVirtualButton(joy, SDL_GAMEPAD_BUTTON_START, false);
	check(only(events(), T::KeyUp, 0x13), "keyboard mode: Start released releases Pause");
	SDL_SetJoystickVirtualButton(joy, SDL_GAMEPAD_BUTTON_BACK, true);
	check(only(events(), T::KeyDown, 0x1B), "keyboard mode: Back presses Escape");
	SDL_SetJoystickVirtualButton(joy, SDL_GAMEPAD_BUTTON_BACK, false);
	check(only(events(), T::KeyUp, 0x1B), "keyboard mode: Back released releases Escape");

	// Turning the mode off (the game switched to the joystick) releases what the pad holds.
	SDL_SetJoystickVirtualButton(joy, SDL_GAMEPAD_BUTTON_SOUTH, true);
	check(only(events(), T::KeyDown, 0x11), "A held before the mode is turned off");
	blupi::host::set_gamepad_keys(nullptr);
	check(only(events(), T::KeyUp, 0x11), "turning the mode off releases Ctrl");
	SDL_SetJoystickVirtualButton(joy, SDL_GAMEPAD_BUTTON_SOUTH, false);
	check(events().empty(), "A released after the mode is off: no key event");
	SDL_SetJoystickVirtualButton(joy, SDL_GAMEPAD_BUTTON_START, true);
	check(only(events(), T::KeyDown, 0x1B), "mode off: Start is Escape again");
	SDL_SetJoystickVirtualButton(joy, SDL_GAMEPAD_BUTTON_START, false);
	check(only(events(), T::KeyUp, 0x1B), "mode off: Start released releases Escape");

	// Unplugged.
	SDL_CloseJoystick(joy);
	SDL_DetachVirtualJoystick(id);
	pump();
	check(joyGetNumDevs() == 0, "no device after unplugging");
	check(joyGetPosEx(0, &info) == JOYERR_UNPLUGGED, "reading it reports JOYERR_UNPLUGGED");

	blupi::host::shutdown();
	printf("%s\n", failures == 0 ? "all checks passed" : "FAILED");
	return failures == 0 ? 0 : 1;
}

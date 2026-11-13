// SDL3 host: window, presentation of the 640x480 logical frame, input and
// time. Free of Win32 names; common/app/launcher.cpp turns host events into the
// window messages the historical CEvent code expects.
//
// Keys are reported with Win32 virtual-key numbering because that is the
// game's own input vocabulary (CEvent::TreatEvent switches on it); the mapping
// table from SDL keycodes lives in host.cpp.
#ifndef BLUPI_PLATFORM_HOST_H
#define BLUPI_PLATFORM_HOST_H

#include "platform/cursor.h"

#include <cstdint>
#include <string>
#include <vector>

namespace blupi::host
{
struct Options
{
    bool headless = false;    // no window, no audio device, no waiting (automated tests)
    bool gamepads = false;    // headless only: gamepad events anyway (tests)
    bool fullscreen = false;  // borderless desktop fullscreen, letterboxed
    int  scale = 2;           // initial window size = logical size * scale
    bool integerScale = false;
    int  viewWidth = 0;       // widescreen: presented width (> logical width); the game's
                              // frame is centred in it and mouse positions stay frame-relative
    bool autoWide = false;    // widescreen: view width from the window shape (4:3 to 21:9)
    const unsigned char* iconPng = nullptr;  // the game's window icon (PNG bytes, kept alive
    unsigned iconPngSize = 0;                // by the game); nullptr: the built-in res/icon.png
};

// Widest automatic view: 21:9 at the frame height of 480.
constexpr int kMaxViewWidth = 1120;

enum class EventType
{
    Quit,
    FocusGained,
    FocusLost,
    KeyDown,      // key = virtual-key code, repeat set for auto-repeat
    KeyUp,
    Text,         // ch = one cp1252 character (from SDL text input)
    MouseMove,    // x, y logical; inside = within the 640x480 frame
    MouseDown,    // button: 1 left, 2 right
    MouseUp,
};

struct Event
{
    EventType type;
    int  key = 0;
    bool repeat = false;
    int  ch = 0;
    int  x = 0, y = 0;
    bool inside = true;
    int  button = 0;
    bool shift = false, control = false;
    bool leftDown = false, rightDown = false;  // button state after the event
};

bool init(const char* title, int logicalWidth, int logicalHeight, const Options& options);
void shutdown();
bool headless();

// Wait at most timeoutNs for an event; returns false on timeout.
bool wait_event(Event& event, uint64_t timeoutNs);
bool poll_event(Event& event);
uint64_t now_ns();

// Presents a full logical frame (0xAARRGGBB, logical size, tightly packed).
// width x logical height; ignored unless width is the current view width.
// With a frame scale (higher-resolution images), the frame is that many times
// larger on each axis: width is the view width times the scale.
void present(const uint32_t* argb, int width);
// The presented frame's resolution, times the logical size (1 to 4); the
// logical coordinates (mouse, layout) stay as they are.
void set_frame_scale(int scale);
// True when present() waits for the display refresh (vsync).
bool presents_with_vsync();
void toggle_fullscreen();
void set_fullscreen(bool on);
bool fullscreen();
void set_integer_scale(bool on);
// Widescreen: automatic (window shape), or a fixed view width (the logical
// width turns it off). view_width() is the width present() expects now.
void set_auto_wide(bool on);
void set_view_width(int width);
int  view_width();
// Gamepads (SDL gamepad API), numbered in the order they were connected;
// none in headless runs. Axes -32768..32767 (left stick, the d-pad overrides
// it); buttons: bit 0 south (A), 1 east (B), 2 west (X), 3 north (Y). Start
// arrives as the Escape key.
int  gamepad_count();
bool gamepad_name(int index, char* name, int size);   // in the game's code page
bool gamepad_state(int index, int& x, int& y, unsigned& buttons);
// The gamepad as the keyboard (port addition, for a game set to keyboard input):
// while set, every gamepad's buttons arrive as KeyDown/KeyUp of these virtual keys
// (0: none), and with arrows the d-pad and the left stick as the arrow keys. While
// not set, only Start arrives (as Escape, above) and the game reads the pad through
// its own joystick option. Turning it off releases every key the pad holds.
struct GamepadKeys
{
    int south = 0, east = 0, west = 0, north = 0;   // A, B, X, Y
    int start = 0, back = 0;
    int leftShoulder = 0, rightShoulder = 0;
    bool arrows = true;
};
void set_gamepad_keys(const GamepadKeys* keys);   // nullptr: off

// On-screen touch controls (touch screens: Android, platform/touch.h): the
// play controls while the game is played, the scroll pad in the editor.
enum class TouchMode { Off, Play, Editor };
void set_touch_mode(TouchMode mode);
// The editor pad's buttons (left, right, up, down), released and pressed:
// size x size 0xAARRGGBB images.
void set_touch_editor_images(const std::vector<uint32_t> released[4], const std::vector<uint32_t> pressed[4],
                             int size);
// A touch-screen platform (Android): always full screen, controls on screen,
// no cursor sprite.
bool touch_screen();

// Where the game's frame lies in the view (default: centred); mouse positions
// are reported relative to it.
void set_frame_x(int x);

// Logical mouse position (may lie outside the frame, like GetCursorPos).
void get_mouse(int& x, int& y);
void warp_mouse(int x, int y);
void show_system_cursor(bool show);
// MouseType 2 and 3: the original's Windows cursor (platform/cursor.h), enlarged with the frame
// (an integer factor of the window's height over the frame's, as the 640x480 full screen of
// the original showed it); a system cursor where the platform has no custom cursors.
void set_game_cursor(GameCursor cursor);

// Called around every wait for platform events (poll_event, wait_event): the multiplayer keep-alive
// (net/keepalive.h) releases the game loop's lock there.
void set_event_wait_hooks(void (*before)(), void (*after)());
// The app goes to the background (true) or comes back (false): SDL's lifecycle events, which SDL
// delivers on the game thread inside the event wait (Android, where the wait then blocks until the
// app is back).
void set_background_handler(void (*handler)(bool background));
// Tests: the background as Android has it, for `ms` milliseconds: the game thread inside an event
// wait, the handler told (unless `bare`, which shows what happens without it).
void simulate_background(unsigned ms, bool bare);

void set_title(const char* title);
void message_box(const char* title, const char* text);
// A new multiplayer room to pass on (the server screen): on the clipboard, and in a browser also
// to the page (Module.blupiRoomCreated(room, server), port/web/loader.js), which offers a link.
void share_room(const char* room, const char* server);
std::string base_path();   // directory of the executable, with trailing separator
// Per-user writable directory of a program (SDL_GetPrefPath), with trailing separator.
std::string pref_path(const char* org, const char* app);
// Extra places for the game data on this platform (Android: the app storage).
std::vector<std::string> data_dirs();
// Asks the player for a folder: a message with "Choose folder..." and "Quit",
// then the system's folder dialog. Empty when the player quits or cancels, or
// where there is no dialog (headless runs, Android).
std::string choose_folder(const char* title, const char* text);
// The folders directly inside dir (full paths).
std::vector<std::string> subdirectories(const std::string& dir);
// Text input (and so the on-screen keyboard on Android). Where it is "on
// demand", the port turns it on only while the game asks for typed text.
void set_text_input(bool on);
bool text_input_on_demand();

// Diagnostics (SDL log; debug messages are shown with --verbose).
void log_debug(const char* text);
void log_info(const char* text);
void set_verbose(bool verbose);
}  // namespace blupi::host

#endif

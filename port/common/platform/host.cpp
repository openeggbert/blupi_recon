#include "platform/host.h"

#include "platform/icon.h"
#include "platform/touch.h"

#include <SDL3/SDL.h>
#ifdef SDL_PLATFORM_EMSCRIPTEN
#include <emscripten.h>
#endif
#ifdef SDL_PLATFORM_ANDROID
#include <jni.h>
#endif
#ifdef SDL_PLATFORM_WINDOWS
#include <io.h>
#include <sys/stat.h>
#endif

#include <deque>
#include <map>
#include <utility>
#include <vector>

namespace blupi::host
{
namespace
{
SDL_Window*   g_window = nullptr;
SDL_Renderer* g_renderer = nullptr;
SDL_Texture*  g_texture = nullptr;
SDL_Cursor*   g_cursor = nullptr;
bool g_gameCursorSet = false;               // set_game_cursor: the original's cursor, MouseType 2/3
GameCursor g_gameCursor = GameCursor::Arrow;
int g_gameCursorScale = 0;                  // its enlargement when it was made
int cursor_scale();
void apply_game_cursor();
void (*g_waitBefore)() = nullptr;           // set_event_wait_hooks
void (*g_waitAfter)() = nullptr;
void (*g_background)(bool) = nullptr;       // set_background_handler
struct WaitScope
{
    WaitScope() { if (g_waitBefore) g_waitBefore(); }
    ~WaitScope() { if (g_waitAfter) g_waitAfter(); }
};
bool SDLCALL lifecycle_watch(void*, SDL_Event* ev)
{
    if (g_background && ev->type == SDL_EVENT_WILL_ENTER_BACKGROUND) g_background(true);
    if (g_background && ev->type == SDL_EVENT_DID_ENTER_FOREGROUND) g_background(false);
    return true;
}
int  g_width = 0, g_height = 0;   // the game's frame
int  g_viewWidth = 0, g_frameX = 0; // the presented view; the frame's x in it (widescreen)
int  g_frameScale = 1;              // set_frame_scale: the texture's resolution, times the view
bool g_textureStale = false;        // the texture must be made again (another frame scale)
bool g_autoWide = false;            // view width follows the window shape
bool g_integerScale = false;
bool g_headless = false;
bool g_headlessPads = false;   // tests: gamepads without a window
bool g_fullscreen = false;
std::vector<SDL_Gamepad*> g_pads;   // in the order they were connected
std::deque<Event> g_pending;        // synthesized events (touch, a paste), before SDL's
constexpr int kPasteBuffer = 1024;  // the longest pasted text from a browser, in bytes
#ifdef SDL_PLATFORM_ANDROID
const bool g_touchControls = true;  // on-screen controls in play (platform/touch.h)
#else
const bool g_touchControls = false;
#endif
SDL_FingerID g_mouseFinger = 0;     // the finger acting as the mouse, 0: none
bool g_mouseFingerDown = false;

void open_gamepad(SDL_JoystickID id)
{
    for (SDL_Gamepad* pad : g_pads)
    {
        if (SDL_GetGamepadID(pad) == id) return;
    }
    if (SDL_Gamepad* pad = SDL_OpenGamepad(id)) g_pads.push_back(pad);
}

// The gamepad as the keyboard (set_gamepad_keys): the keys the pads hold, counted
// per virtual key so that two sources of one key (the d-pad and the stick, or two
// buttons mapped alike) press it once and release it with the last.
bool g_padKeysOn = false;
GamepadKeys g_padKeys;
std::map<int, int> g_padHeld;          // virtual key -> sources holding it
std::map<std::pair<SDL_JoystickID, int>, int> g_padSources;   // (pad, source) -> virtual key
// Left stick past these magnitudes turns an arrow on, below the second off.
constexpr int kStickOn = 16384, kStickOff = 12000;
constexpr int kVkLeft = 0x25, kVkUp = 0x26, kVkRight = 0x27, kVkDown = 0x28;   // VK_LEFT..VK_DOWN

void fill_buttons(Event& e);

void pad_key(int vk, bool down)
{
    if (vk == 0) return;
    int& count = g_padHeld[vk];
    const int before = count;
    count += down ? 1 : -1;
    if (count < 0) count = 0;
    if ((before == 0) == (count == 0)) return;   // another source still holds it, or none did
    Event e{count ? EventType::KeyDown : EventType::KeyUp};
    e.key = vk;
    fill_buttons(e);
    g_pending.push_back(e);
}

// One source (a button, a d-pad direction, a stick direction) of one pad.
void pad_source(SDL_JoystickID pad, int source, int vk, bool down)
{
    const auto key = std::make_pair(pad, source);
    auto it = g_padSources.find(key);
    if (down)
    {
        if (it != g_padSources.end() || vk == 0) return;
        g_padSources[key] = vk;
        pad_key(vk, true);
    }
    else if (it != g_padSources.end())
    {
        const int held = it->second;
        g_padSources.erase(it);
        pad_key(held, false);
    }
}

int pad_button_key(int button)
{
    const GamepadKeys& k = g_padKeys;
    switch (button)
    {
    case SDL_GAMEPAD_BUTTON_SOUTH: return k.south;
    case SDL_GAMEPAD_BUTTON_EAST: return k.east;
    case SDL_GAMEPAD_BUTTON_WEST: return k.west;
    case SDL_GAMEPAD_BUTTON_NORTH: return k.north;
    case SDL_GAMEPAD_BUTTON_START: return k.start;
    case SDL_GAMEPAD_BUTTON_BACK: return k.back;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return k.leftShoulder;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return k.rightShoulder;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return k.arrows ? kVkLeft : 0;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return k.arrows ? kVkRight : 0;
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return k.arrows ? kVkUp : 0;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return k.arrows ? kVkDown : 0;
    default: return 0;
    }
}

// Stick directions are sources above the button numbers.
void pad_axis(SDL_JoystickID pad, int axis, int value)
{
    if (!g_padKeys.arrows || (axis != SDL_GAMEPAD_AXIS_LEFTX && axis != SDL_GAMEPAD_AXIS_LEFTY)) return;
    const bool x = axis == SDL_GAMEPAD_AXIS_LEFTX;
    const int negative = 1000 + axis * 2, positive = negative + 1;
    const int vkNegative = x ? kVkLeft : kVkUp, vkPositive = x ? kVkRight : kVkDown;
    const bool negativeHeld = g_padSources.count({pad, negative}) != 0;
    const bool positiveHeld = g_padSources.count({pad, positive}) != 0;
    pad_source(pad, negative, vkNegative, value <= -kStickOn || (negativeHeld && value <= -kStickOff));
    pad_source(pad, positive, vkPositive, value >= kStickOn || (positiveHeld && value >= kStickOff));
}

void pad_release_all(SDL_JoystickID pad, bool everyPad)
{
    for (auto it = g_padSources.begin(); it != g_padSources.end();)
    {
        if (!everyPad && it->first.first != pad) { ++it; continue; }
        const int held = it->second;
        it = g_padSources.erase(it);
        pad_key(held, false);
    }
}

void close_gamepad(SDL_JoystickID id)
{
    pad_release_all(id, false);
    for (size_t i = 0; i < g_pads.size(); i++)
    {
        if (SDL_GetGamepadID(g_pads[i]) != id) continue;
        SDL_CloseGamepad(g_pads[i]);
        g_pads.erase(g_pads.begin() + (long)i);
        return;
    }
}

// Fit the window at a size that is not a whole multiple of the frame: nearest sampling alone made some
// pixels one screen pixel wider than others (2 and 3 at 2.25 times, 1080 lines), which looks blocky.
// The frame is first enlarged by the next whole factor with nearest sampling (into g_sharp), then
// shrunk to the window with linear filtering: every pixel the same size, sharp but for a soft edge.
// Whole multiples, "Whole pixels" and a frame shrunk to the window are drawn as before. Plain render
// targets, so it works with every renderer, WebGL included.
SDL_Texture* g_sharp = nullptr;
int g_sharpW = 0, g_sharpH = 0;

SDL_Texture* sharp_frame(int textureW, int textureH)
{
    if (g_integerScale) return g_texture;
    SDL_FRect shown{};
    if (!SDL_GetRenderLogicalPresentationRect(g_renderer, &shown) || shown.h <= 0) return g_texture;
    const float scale = shown.h / (float)textureH;
    if (scale < 1.0f || SDL_fabsf(scale - SDL_roundf(scale)) < 0.01f) return g_texture;
    const int factor = (int)SDL_ceilf(scale);
    const int w = textureW * factor, h = textureH * factor;
    const Sint64 limit = SDL_GetNumberProperty(SDL_GetRendererProperties(g_renderer),
                                               SDL_PROP_RENDERER_MAX_TEXTURE_SIZE_NUMBER, 4096);
    if (w > limit || h > limit) return g_texture;
    if (g_sharp == nullptr || g_sharpW != w || g_sharpH != h)
    {
        if (g_sharp) SDL_DestroyTexture(g_sharp);
        g_sharp = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, w, h);
        g_sharpW = w;
        g_sharpH = h;
        if (g_sharp == nullptr) return g_texture;
        SDL_SetTextureScaleMode(g_sharp, SDL_SCALEMODE_LINEAR);
    }
    if (!SDL_SetRenderTarget(g_renderer, g_sharp)) return g_texture;
    SDL_RenderTexture(g_renderer, g_texture, nullptr, nullptr);   // nearest, the whole target
    SDL_SetRenderTarget(g_renderer, nullptr);
    return g_sharp;
}

// Sets the presented view width: texture and logical presentation follow.
void apply_view_width(int width)
{
    if (width < g_width) width = g_width;
    if (width == g_viewWidth && (g_headless || g_texture != nullptr) && !g_textureStale) return;
    g_textureStale = false;
    g_viewWidth = width;
    g_frameX = (g_viewWidth - g_width) / 2;   // until the port places the frame (set_frame_x)
    if (g_headless || g_renderer == nullptr) return;
    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "view %dx%d", g_viewWidth, g_height);
    if (g_texture) SDL_DestroyTexture(g_texture);
    g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                  g_viewWidth * g_frameScale, g_height * g_frameScale);
    if (g_texture) SDL_SetTextureScaleMode(g_texture, SDL_SCALEMODE_NEAREST);
    SDL_SetRenderLogicalPresentation(g_renderer, g_viewWidth, g_height,
                                     g_integerScale ? SDL_LOGICAL_PRESENTATION_INTEGER_SCALE
                                                    : SDL_LOGICAL_PRESENTATION_LETTERBOX);
}

// Automatic widescreen: the view as wide as the window's shape allows,
// between the frame's 4:3 and 21:9, at the frame's height. An even width keeps
// the frame on whole pixels. Other shapes are letterboxed.
void update_auto_width()
{
    if (!g_autoWide || g_headless || g_window == nullptr) return;
    int w = 0, h = 0;
    if (!SDL_GetWindowSizeInPixels(g_window, &w, &h) || w <= 0 || h <= 0) return;
    int width = (int)((long long)g_height * w / h);
    if (width > kMaxViewWidth) width = kMaxViewWidth;
    width &= ~1;
    apply_view_width(width);
}

// SDL keycode -> Win32 virtual-key code (0 = not forwarded to the game).
int to_virtual_key(SDL_Keycode k)
{
    if (k >= SDLK_A && k <= SDLK_Z) return 'A' + (int)(k - SDLK_A);
    if (k >= SDLK_0 && k <= SDLK_9) return '0' + (int)(k - SDLK_0);
    if (k >= SDLK_F1 && k <= SDLK_F12) return 0x70 + (int)(k - SDLK_F1);
    switch (k)
    {
    case SDLK_BACKSPACE: return 0x08;
    case SDLK_TAB:       return 0x09;
    case SDLK_RETURN:    return 0x0D;
    case SDLK_KP_ENTER:  return 0x0D;
    case SDLK_LSHIFT: case SDLK_RSHIFT: return 0x10;
    case SDLK_LCTRL:  case SDLK_RCTRL:  return 0x11;
    case SDLK_LALT:   case SDLK_RALT:   return 0x12;
    case SDLK_PAUSE:     return 0x13;
    case SDLK_ESCAPE:    return 0x1B;
    case SDLK_AC_BACK:   return 0x1B;   // Android Back: Escape (pause, back)
    case SDLK_SPACE:     return 0x20;
    case SDLK_PAGEUP:    return 0x21;
    case SDLK_PAGEDOWN:  return 0x22;
    case SDLK_END:       return 0x23;
    case SDLK_HOME:      return 0x24;
    case SDLK_LEFT:      return 0x25;
    case SDLK_UP:        return 0x26;
    case SDLK_RIGHT:     return 0x27;
    case SDLK_DOWN:      return 0x28;
    case SDLK_INSERT:    return 0x2D;
    case SDLK_DELETE:    return 0x2E;
    case SDLK_KP_0: return 0x60; case SDLK_KP_1: return 0x61; case SDLK_KP_2: return 0x62;
    case SDLK_KP_3: return 0x63; case SDLK_KP_4: return 0x64; case SDLK_KP_5: return 0x65;
    case SDLK_KP_6: return 0x66; case SDLK_KP_7: return 0x67; case SDLK_KP_8: return 0x68;
    case SDLK_KP_9: return 0x69;
    default: return 0;
    }
}

// Unicode code point -> cp1252 byte, 0 if not representable.
int to_cp1252(uint32_t cp)
{
    if (cp < 0x80 || (cp >= 0xA0 && cp <= 0xFF)) return (int)cp;
    static const uint16_t high[32] = {
        0x20AC, 0, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0, 0x017D, 0,
        0, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0, 0x017E, 0x0178};
    for (int i = 0; i < 32; i++)
        if (high[i] == cp) return 0x80 + i;
    return 0;
}

void fill_buttons(Event& e)
{
    SDL_MouseButtonFlags state = SDL_GetMouseState(nullptr, nullptr);
    e.leftDown = (state & SDL_BUTTON_LMASK) != 0;
    e.rightDown = (state & SDL_BUTTON_RMASK) != 0;
    SDL_Keymod mod = SDL_GetModState();
    e.shift = (mod & SDL_KMOD_SHIFT) != 0;
    e.control = (mod & SDL_KMOD_CTRL) != 0;
}

void set_position(Event& e, float x, float y)
{
    e.x = (int)SDL_floorf(x) - g_frameX;   // frame coordinates
    e.y = (int)SDL_floorf(y);
    e.inside = e.x >= 0 && e.y >= 0 && e.x < g_width && e.y < g_height;
}

// Pending text input split into single cp1252 characters.
char g_text[64];
int  g_textPos = 0, g_textLen = 0;

bool translate(SDL_Event& ev, Event& out)
{
    switch (ev.type)
    {
    case SDL_EVENT_QUIT:
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        out = Event{EventType::Quit};
        return true;
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
        out = Event{EventType::FocusGained};
        return true;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        out = Event{EventType::FocusLost};
        return true;
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        update_auto_width();
        if (g_gameCursorSet && cursor_scale() != g_gameCursorScale) apply_game_cursor();
        return false;
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_FINGER_UP:
    {
        if (!g_touchControls || g_renderer == nullptr) return false;
        const int type = ev.type == SDL_EVENT_FINGER_DOWN ? 0 : ev.type == SDL_EVENT_FINGER_UP ? 2 : 1;
        int ow = 0, oh = 0, ww = 0, wh = 0;
        SDL_GetRenderOutputSize(g_renderer, &ow, &oh);
        SDL_GetWindowSize(g_window, &ww, &wh);
        std::vector<touch::KeyChange> keys;
        if (touch::finger(type, ev.tfinger.fingerID, ev.tfinger.x * ow, ev.tfinger.y * oh, ow, oh, keys))
        {
            for (const touch::KeyChange& k : keys)
            {
                Event e{k.down ? EventType::KeyDown : EventType::KeyUp};
                e.key = k.vk;
                g_pending.push_back(e);
            }
            return false;
        }
        // Any other finger (one at a time) is the mouse's left button.
        if (type == 0 && g_mouseFingerDown) return false;
        if (type != 0 && (!g_mouseFingerDown || ev.tfinger.fingerID != g_mouseFinger)) return false;
        float lx = 0, ly = 0;
        SDL_RenderCoordinatesFromWindow(g_renderer, ev.tfinger.x * ww, ev.tfinger.y * wh, &lx, &ly);
        Event move{EventType::MouseMove};
        set_position(move, lx, ly);
        if (type == 0)
        {
            g_mouseFinger = ev.tfinger.fingerID;
            g_mouseFingerDown = true;
            g_pending.push_back(move);           // the pointer first moves there
            out = Event{EventType::MouseDown};
            out.button = 1;
            set_position(out, lx, ly);
            out.leftDown = true;
            g_pending.push_back(out);
            return false;
        }
        if (type == 2)
        {
            g_mouseFingerDown = false;
            out = Event{EventType::MouseUp};
            out.button = 1;
            set_position(out, lx, ly);
            g_pending.push_back(out);
            return false;
        }
        move.leftDown = true;
        g_pending.push_back(move);
        return false;
    }
    case SDL_EVENT_GAMEPAD_ADDED:
        open_gamepad(ev.gdevice.which);
        return false;
    case SDL_EVENT_GAMEPAD_REMOVED:
        close_gamepad(ev.gdevice.which);
        return false;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        if (g_padKeysOn)
        {
            // The gamepad as the keyboard: the key events wait in g_pending.
            pad_source(ev.gbutton.which, ev.gbutton.button, pad_button_key(ev.gbutton.button),
                       ev.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);
            return false;
        }
        // Port convenience, not seen as a joystick button by the game: Start
        // is the Escape key (pause and back), so a gamepad alone can play.
        if (ev.gbutton.button != SDL_GAMEPAD_BUTTON_START) return false;
        out = Event{ev.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN ? EventType::KeyDown : EventType::KeyUp};
        out.key = 0x1B;   // VK_ESCAPE
        fill_buttons(out);
        return true;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        if (g_padKeysOn) pad_axis(ev.gaxis.which, ev.gaxis.axis, ev.gaxis.value);
        return false;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
    {
        bool down = ev.type == SDL_EVENT_KEY_DOWN;
        // Port-level shortcut, not seen by the game: Alt+Enter toggles fullscreen.
        if (down && ev.key.key == SDLK_RETURN && (ev.key.mod & SDL_KMOD_ALT))
        {
            if (!ev.key.repeat) toggle_fullscreen();
            return false;
        }
        int vk = to_virtual_key(ev.key.key);
        if (vk == 0) return false;
        out = Event{down ? EventType::KeyDown : EventType::KeyUp};
        out.key = vk;
        out.repeat = ev.key.repeat;
        fill_buttons(out);
        return true;
    }
    case SDL_EVENT_TEXT_INPUT:
    {
        g_textLen = 0;
        g_textPos = 0;
        const char* s = ev.text.text;
        while (*s && g_textLen < (int)sizeof(g_text))
        {
            Uint32 cp = SDL_StepUTF8(&s, nullptr);
            int c = to_cp1252(cp);
            if (c >= 0x20) g_text[g_textLen++] = (char)c;
        }
        return false;  // delivered by poll_event from g_text
    }
    case SDL_EVENT_MOUSE_MOTION:
        SDL_ConvertEventToRenderCoordinates(g_renderer, &ev);
        out = Event{EventType::MouseMove};
        set_position(out, ev.motion.x, ev.motion.y);
        fill_buttons(out);
        return true;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
    {
        if (ev.button.button != SDL_BUTTON_LEFT && ev.button.button != SDL_BUTTON_RIGHT) return false;
        SDL_ConvertEventToRenderCoordinates(g_renderer, &ev);
        out = Event{ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? EventType::MouseDown : EventType::MouseUp};
        out.button = ev.button.button == SDL_BUTTON_LEFT ? 1 : 2;
        set_position(out, ev.button.x, ev.button.y);
        fill_buttons(out);
        return true;
    }
    default:
        return false;
    }
}

bool next_text(Event& event)
{
    if (g_textPos >= g_textLen) return false;
    event = Event{EventType::Text};
    event.ch = (unsigned char)g_text[g_textPos++];
    return true;
}
}  // namespace

namespace { void PrepareLog(); }   // the log's output on Windows (end of this file)

bool init(const char* title, int logicalWidth, int logicalHeight, const Options& options)
{
    PrepareLog();
    g_width = logicalWidth;
    g_height = logicalHeight;
    g_viewWidth = options.viewWidth > logicalWidth ? options.viewWidth : logicalWidth;
    g_frameX = (g_viewWidth - logicalWidth) / 2;
    g_headless = options.headless;
    g_autoWide = options.autoWide;
    g_integerScale = options.integerScale;
    g_fullscreen = g_touchControls;   // touch screens: created full screen
    g_headlessPads = options.headless && options.gamepads;
    if (g_headless) return SDL_Init(g_headlessPads ? SDL_INIT_GAMEPAD : 0);

    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
#ifdef SDL_PLATFORM_ANDROID
    // A landscape game; without the hint SDL lets a resizable window rotate freely.
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
#endif
    // Touch: our own controls and mouse (above), not SDL's mouse emulation.
    if (g_touchControls) SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return false;
    }
    SDL_AddEventWatch(lifecycle_watch, nullptr);   // set_background_handler
    int scale = options.scale < 1 ? 1 : options.scale;
    // Automatic widescreen opens a 16:9 window; resizing changes the shape.
    const int windowWidth = g_autoWide ? (logicalHeight * 16 / 9 + 1) & ~1 : g_viewWidth;
#ifdef SDL_PLATFORM_ANDROID
    const SDL_WindowFlags windowFlags = SDL_WINDOW_FULLSCREEN;   // the whole screen, system bars hidden
#else
    const SDL_WindowFlags windowFlags = SDL_WINDOW_RESIZABLE;
#endif
    if (!SDL_CreateWindowAndRenderer(title, windowWidth * scale, logicalHeight * scale,
                                     windowFlags, &g_window, &g_renderer))
    {
        SDL_Log("SDL_CreateWindowAndRenderer failed: %s", SDL_GetError());
        return false;
    }
    SDL_SetWindowMinimumSize(g_window, logicalWidth / 2, logicalHeight / 2);
#if !defined(SDL_PLATFORM_WINDOWS) && !defined(SDL_PLATFORM_ANDROID)
    // (Windows takes it from the executable's resources.)
    if (SDL_Surface* icon = options.iconPng
            ? SDL_LoadPNG_IO(SDL_IOFromConstMem(options.iconPng, options.iconPngSize), true)
            : game_icon())
    {
        SDL_SetWindowIcon(g_window, icon);
        SDL_DestroySurface(icon);
    }
#endif
    SDL_SetRenderLogicalPresentation(g_renderer, g_viewWidth, logicalHeight,
                                     options.integerScale ? SDL_LOGICAL_PRESENTATION_INTEGER_SCALE
                                                          : SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetRenderVSync(g_renderer, 1);
    g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                  g_viewWidth, logicalHeight);
    if (g_texture == nullptr) return false;
    SDL_SetTextureScaleMode(g_texture, SDL_SCALEMODE_NEAREST);
    if (options.fullscreen) toggle_fullscreen();
    update_auto_width();
    if (!text_input_on_demand()) SDL_StartTextInput(g_window);
#ifdef SDL_PLATFORM_EMSCRIPTEN
    // The page's text for request_paste: the browser's own paste (Ctrl+V, Shift+Insert, a menu),
    // and the clipboard read that request_paste starts.
    EM_ASM({
        Module.blupiPaste = function(text) {
            if (typeof text !== 'string' || !text.length) return;
            stringToUTF8(text, _blupi_host_paste_buffer(), $0);
            _blupi_host_pasted();
        };
        document.addEventListener('paste', function(event) {
            if (event.clipboardData) Module.blupiPaste(event.clipboardData.getData('text/plain'));
        });
    }, kPasteBuffer);
#endif
    return true;
}

void shutdown()
{
    if (g_sharp) SDL_DestroyTexture(g_sharp);
    g_sharp = nullptr;
    if (g_cursor) SDL_DestroyCursor(g_cursor);
    if (g_texture) SDL_DestroyTexture(g_texture);
    if (g_renderer) SDL_DestroyRenderer(g_renderer);
    if (g_window) SDL_DestroyWindow(g_window);
    g_cursor = nullptr;
    g_texture = nullptr;
    g_renderer = nullptr;
    g_window = nullptr;
    SDL_Quit();
}

bool headless() { return g_headless; }

uint64_t now_ns() { return SDL_GetTicksNS(); }

bool poll_event(Event& event)
{
    if (!g_pending.empty())
    {
        event = g_pending.front();
        g_pending.pop_front();
        return true;
    }
    if (g_headless && !g_headlessPads) return false;
    if (next_text(event)) return true;
    SDL_Event ev;
    for (;;)
    {
        bool got;
        {
            WaitScope wait;
            got = SDL_PollEvent(&ev);
        }
        if (!got) break;
        if (translate(ev, event)) return true;
        if (next_text(event)) return true;
        if (!g_pending.empty()) return poll_event(event);
    }
    return false;
}

bool wait_event(Event& event, uint64_t timeoutNs)
{
    if (g_headless) return false;
    if (poll_event(event)) return true;
    Sint32 ms = (Sint32)(timeoutNs / 1000000);
    SDL_Event ev;
    bool got;
    {
        WaitScope wait;
        got = SDL_WaitEventTimeout(&ev, ms);
    }
    if (!got) return false;
    if (translate(ev, event)) return true;
    if (next_text(event)) return true;
    return poll_event(event);   // events translate() queued (touch, the gamepad as the keyboard)
}

void set_frame_scale(int scale)
{
    scale = scale < 1 ? 1 : scale > 4 ? 4 : scale;
    if (scale == g_frameScale) return;
    g_frameScale = scale;
    g_textureStale = true;
    apply_view_width(g_viewWidth);
}

void present(const uint32_t* argb, int width)
{
    if (g_headless || g_texture == nullptr) return;
    if (width != g_viewWidth * g_frameScale) return;   // composed before a resize: the next one fits
    SDL_UpdateTexture(g_texture, nullptr, argb, width * 4);
    SDL_Texture* frame = sharp_frame(width, g_height * g_frameScale);
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_renderer);
    SDL_RenderTexture(g_renderer, frame, nullptr, nullptr);
    if (touch::mode() != touch::Mode::Off)
    {
        // The controls in output pixels, also over the bars beside the frame.
        SDL_FRect frame{};
        SDL_GetRenderLogicalPresentationRect(g_renderer, &frame);
        SDL_SetRenderLogicalPresentation(g_renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
        int ow = 0, oh = 0;
        SDL_GetRenderOutputSize(g_renderer, &ow, &oh);
        touch::draw(g_renderer, ow, oh, frame);
        SDL_SetRenderLogicalPresentation(g_renderer, g_viewWidth, g_height,
                                         g_integerScale ? SDL_LOGICAL_PRESENTATION_INTEGER_SCALE
                                                        : SDL_LOGICAL_PRESENTATION_LETTERBOX);
    }
    SDL_RenderPresent(g_renderer);
}

bool presents_with_vsync()
{
    if (g_headless || g_renderer == nullptr) return false;
    int vsync = 0;
    return SDL_GetRenderVSync(g_renderer, &vsync) && vsync != 0;
}

void toggle_fullscreen()
{
    set_fullscreen(!g_fullscreen);
}

void set_fullscreen(bool on)
{
    if (g_window == nullptr || g_touchControls) return;   // touch screens: always full screen
    g_fullscreen = on;
    SDL_SetWindowFullscreen(g_window, g_fullscreen);
}

bool fullscreen() { return g_fullscreen; }

void set_integer_scale(bool on)
{
    g_integerScale = on;
    if (g_renderer == nullptr) return;
    SDL_SetRenderLogicalPresentation(g_renderer, g_viewWidth, g_height,
                                     g_integerScale ? SDL_LOGICAL_PRESENTATION_INTEGER_SCALE
                                                    : SDL_LOGICAL_PRESENTATION_LETTERBOX);
}

void set_auto_wide(bool on)
{
    g_autoWide = on;
    if (on) update_auto_width();
    else apply_view_width(g_width);
}

void set_view_width(int width)
{
    g_autoWide = false;
    apply_view_width(width);
}

int view_width() { return g_viewWidth; }

bool touch_screen()
{
    return g_touchControls;
}

void set_touch_mode(TouchMode mode)
{
    std::vector<touch::KeyChange> keys;
    touch::Mode m = mode == TouchMode::Play ? touch::Mode::Play
                  : mode == TouchMode::Editor ? touch::Mode::Editor : touch::Mode::Off;
    if (!g_touchControls || g_headless) m = touch::Mode::Off;
    touch::set_mode(m, keys);
    for (const touch::KeyChange& k : keys)
    {
        Event e{k.down ? EventType::KeyDown : EventType::KeyUp};
        e.key = k.vk;
        g_pending.push_back(e);
    }
}

void set_touch_editor_images(const std::vector<uint32_t> released[4], const std::vector<uint32_t> pressed[4],
                             int size)
{
    touch::set_editor_images(released, pressed, size);
}

void set_gamepad_keys(const GamepadKeys* keys)
{
    if (keys == nullptr)
    {
        if (g_padKeysOn) pad_release_all(0, true);
        g_padKeysOn = false;
        return;
    }
    if (g_padKeysOn && (keys->south != g_padKeys.south || keys->east != g_padKeys.east ||
                        keys->west != g_padKeys.west || keys->north != g_padKeys.north ||
                        keys->start != g_padKeys.start || keys->back != g_padKeys.back ||
                        keys->leftShoulder != g_padKeys.leftShoulder ||
                        keys->rightShoulder != g_padKeys.rightShoulder || keys->arrows != g_padKeys.arrows))
    {
        pad_release_all(0, true);   // a new mapping: nothing stays held under the old one
    }
    g_padKeys = *keys;
    g_padKeysOn = true;
}

int gamepad_count()
{
    return (int)g_pads.size();
}

bool gamepad_name(int index, char* name, int size)
{
    if (index < 0 || index >= (int)g_pads.size() || size <= 0) return false;
    const char* utf8 = SDL_GetGamepadName(g_pads[(size_t)index]);
    if (utf8 == nullptr) utf8 = "Gamepad";
    int n = 0;
    while (*utf8 && n < size - 1)
    {
        int c = to_cp1252(SDL_StepUTF8(&utf8, nullptr));
        if (c >= 0x20) name[n++] = (char)c;
    }
    name[n] = 0;
    return true;
}

bool gamepad_state(int index, int& x, int& y, unsigned& buttons)
{
    if (index < 0 || index >= (int)g_pads.size()) return false;
    SDL_Gamepad* pad = g_pads[(size_t)index];
    if (!SDL_GamepadConnected(pad)) return false;
    x = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX);
    y = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY);
    if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_LEFT)) x = -32768;
    if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) x = 32767;
    if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_UP)) y = -32768;
    if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_DOWN)) y = 32767;
    buttons = 0;
    if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_SOUTH)) buttons |= 1;
    if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_EAST)) buttons |= 2;
    if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_WEST)) buttons |= 4;
    if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_NORTH)) buttons |= 8;
    return true;
}

void set_frame_x(int x)
{
    g_frameX = x;
}

void get_mouse(int& x, int& y)
{
    if (g_headless) { x = g_width / 2; y = g_height / 2; return; }
    float wx = 0, wy = 0, lx = 0, ly = 0;
    SDL_GetMouseState(&wx, &wy);
    SDL_RenderCoordinatesFromWindow(g_renderer, wx, wy, &lx, &ly);
    x = (int)SDL_floorf(lx) - g_frameX;
    y = (int)SDL_floorf(ly);
}

void warp_mouse(int x, int y)
{
    if (g_headless || !(SDL_GetWindowFlags(g_window) & SDL_WINDOW_INPUT_FOCUS)) return;
    float wx = 0, wy = 0;
    SDL_RenderCoordinatesToWindow(g_renderer, (float)(x + g_frameX) + 0.5f, (float)y + 0.5f, &wx, &wy);
    SDL_WarpMouseInWindow(g_window, wx, wy);
}

void show_system_cursor(bool show)
{
    if (g_headless) return;
    if (show) SDL_ShowCursor();
    else SDL_HideCursor();
}

namespace
{
// The frame's integer enlargement in the window (window points, as cursor images are).
int cursor_scale()
{
    int ww = 0, wh = 0;
    if (g_window == nullptr || !SDL_GetWindowSize(g_window, &ww, &wh) || g_height <= 0) return 1;
    return wh / g_height > 1 ? wh / g_height : 1;
}

// Where custom cursors are missing: the nearest system cursor.
SDL_Cursor* system_cursor(GameCursor cursor)
{
    SDL_SystemCursor id = SDL_SYSTEM_CURSOR_DEFAULT;
    switch (cursor)
    {
    case GameCursor::Pointer: id = SDL_SYSTEM_CURSOR_POINTER; break;
    case GameCursor::Wait:    id = SDL_SYSTEM_CURSOR_WAIT; break;
    case GameCursor::ArrowU: case GameCursor::ArrowD: id = SDL_SYSTEM_CURSOR_NS_RESIZE; break;
    case GameCursor::ArrowL: case GameCursor::ArrowR: id = SDL_SYSTEM_CURSOR_EW_RESIZE; break;
    case GameCursor::ArrowUL: case GameCursor::ArrowDR: id = SDL_SYSTEM_CURSOR_NWSE_RESIZE; break;
    case GameCursor::ArrowUR: case GameCursor::ArrowDL: id = SDL_SYSTEM_CURSOR_NESW_RESIZE; break;
    default: break;
    }
    return SDL_CreateSystemCursor(id);
}

void apply_game_cursor()
{
    const int scale = cursor_scale();
    const CursorImage& original = game_cursor_image(g_gameCursor);
    SDL_Cursor* c = nullptr;
    if (!original.argb.empty())
    {
        CursorImage image = scale_cursor(original, scale);
        if (SDL_Surface* surface = SDL_CreateSurfaceFrom(image.width, image.height, SDL_PIXELFORMAT_ARGB8888,
                                                         image.argb.data(), image.width * 4))
        {
            c = SDL_CreateColorCursor(surface, image.hotX, image.hotY);
            SDL_DestroySurface(surface);
        }
    }
    if (c == nullptr)
    {
        if (g_gameCursor == GameCursor::Empty) { SDL_HideCursor(); return; }
        c = system_cursor(g_gameCursor);
    }
    if (c == nullptr) return;
    SDL_SetCursor(c);
    if (g_cursor) SDL_DestroyCursor(g_cursor);
    g_cursor = c;
    g_gameCursorScale = scale;
}
}  // namespace

void set_event_wait_hooks(void (*before)(), void (*after)())
{
    g_waitBefore = before;
    g_waitAfter = after;
}

void set_background_handler(void (*handler)(bool background))
{
    g_background = handler;
}

void simulate_background(unsigned ms, bool bare)
{
    WaitScope wait;
    if (g_background && !bare) g_background(true);
    SDL_Delay(ms);
    if (g_background && !bare) g_background(false);
}

void set_game_cursor(GameCursor cursor)
{
    if (g_headless) return;
    g_gameCursorSet = true;
    g_gameCursor = cursor;
    apply_game_cursor();
}

void set_title(const char* title)
{
    if (g_window) SDL_SetWindowTitle(g_window, title);
}

void message_box(const char* title, const char* text)
{
    SDL_Log("%s: %s", title, text);
    if (!g_headless) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, text, g_window);
}

void room_created(const char* room, const char* server)
{
#ifdef SDL_PLATFORM_EMSCRIPTEN
    EM_ASM({
        if (typeof Module.blupiRoomCreated === 'function') Module.blupiRoomCreated(UTF8ToString($0), UTF8ToString($1));
    }, room, server);
#else
    (void)room;
    (void)server;
#endif
}

namespace
{
std::string g_pasted;   // request_paste's text, for the EventType::Paste it queued

void paste_arrived(const char* text)
{
    g_pasted = text != nullptr ? text : "";
    g_pending.push_back(Event{EventType::Paste});
}
}  // namespace

void copy_text(const char* text)
{
    if (g_headless) return;
#ifdef SDL_PLATFORM_EMSCRIPTEN
    // SDL has no browser clipboard; the click that asked for the copy lets the page write it.
    EM_ASM({
        if (navigator.clipboard && navigator.clipboard.writeText)
            navigator.clipboard.writeText(UTF8ToString($0)).catch(function() {});
    }, text);
#else
    SDL_SetClipboardText(text);
#endif
}

void request_paste(bool fromKeys)
{
    if (g_headless) return;
#ifdef SDL_PLATFORM_EMSCRIPTEN
    // Ctrl+V and Shift+Insert are the browser's: its paste event follows (init's listener).
    if (fromKeys) return;
    EM_ASM({
        if (navigator.clipboard && navigator.clipboard.readText)
            navigator.clipboard.readText().then(function(text) { Module.blupiPaste(text); }).catch(function() {});
    });
#else
    (void)fromKeys;
    char* text = SDL_GetClipboardText();
    paste_arrived(text);
    SDL_free(text);
#endif
}

const std::string& pasted_text() { return g_pasted; }

bool control_held() { return !g_headless && (SDL_GetModState() & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0; }
bool shift_held() { return !g_headless && (SDL_GetModState() & SDL_KMOD_SHIFT) != 0; }

bool can_share()
{
    if (g_headless) return false;
#if defined(SDL_PLATFORM_ANDROID)
    return true;
#elif defined(SDL_PLATFORM_EMSCRIPTEN)
    // A phone's or tablet's browser; a desktop browser's share dialog would only get in the way.
    return EM_ASM_INT({
        return typeof Module.blupiShareRoom === 'function' && !!navigator.share &&
               window.matchMedia('(pointer: coarse)').matches ? 1 : 0;
    }) != 0;
#else
    return false;
#endif
}

void share_room(const char* room, const char* server)
{
    if (g_headless) return;
#if defined(SDL_PLATFORM_ANDROID)
    // BlupiActivity.shareText (port/android): the system's share sheet with the room ID.
    (void)server;
    JNIEnv* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
    jobject activity = static_cast<jobject>(SDL_GetAndroidActivity());
    if (env == nullptr || activity == nullptr) return;
    jclass type = env->GetObjectClass(activity);
    jmethodID share = env->GetMethodID(type, "shareText", "(Ljava/lang/String;)V");
    if (share != nullptr)
    {
        jstring text = env->NewStringUTF(room);
        env->CallVoidMethod(activity, share, text);
        env->DeleteLocalRef(text);
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(type);
    env->DeleteLocalRef(activity);
#elif defined(SDL_PLATFORM_EMSCRIPTEN)
    EM_ASM({ Module.blupiShareRoom(UTF8ToString($0), UTF8ToString($1)); }, room, server);
#else
    (void)room;
    (void)server;
#endif
}

std::string base_path()
{
    const char* p = SDL_GetBasePath();
    return p ? std::string(p) : std::string();
}

namespace
{
struct FolderChoice
{
    SDL_AtomicInt done;
    std::string path;
};

void SDLCALL folder_chosen(void* userdata, const char* const* list, int)
{
    FolderChoice* choice = static_cast<FolderChoice*>(userdata);
    if (list == nullptr) SDL_Log("folder dialog: %s", SDL_GetError());
    else if (list[0] != nullptr) choice->path = list[0];
    SDL_SetAtomicInt(&choice->done, 1);
}

SDL_EnumerationResult SDLCALL add_subdirectory(void* userdata, const char* dirname, const char* fname)
{
    std::string path = std::string(dirname) + fname;
    SDL_PathInfo info;
    if (SDL_GetPathInfo(path.c_str(), &info) && info.type == SDL_PATHTYPE_DIRECTORY)
    {
        static_cast<std::vector<std::string>*>(userdata)->push_back(path);
    }
    return SDL_ENUM_CONTINUE;
}
}  // namespace

std::string choose_folder(const char* title, const char* text)
{
#if defined(SDL_PLATFORM_ANDROID) || defined(SDL_PLATFORM_EMSCRIPTEN)
    // Android imports the folder in its launcher; the web page in its loader (port/web).
    (void)title;
    (void)text;
    return std::string();
#else
    if (g_headless) return std::string();
    const SDL_MessageBoxButtonData buttons[] = {
        {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Choose folder..."},
        {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Quit"},
    };
    const SDL_MessageBoxData box = {SDL_MESSAGEBOX_INFORMATION, g_window, title, text,
                                    SDL_arraysize(buttons), buttons, nullptr};
    int button = 0;
    if (!SDL_ShowMessageBox(&box, &button) || button != 1) return std::string();
    // The dialog may answer from another thread: wait for it, keeping the
    // window responsive.
    FolderChoice choice;
    SDL_SetAtomicInt(&choice.done, 0);
    SDL_ShowOpenFolderDialog(folder_chosen, &choice, g_window, nullptr, false);
    while (SDL_GetAtomicInt(&choice.done) == 0)
    {
        SDL_PumpEvents();
        SDL_Delay(10);
    }
    return choice.path;
#endif
}

std::vector<std::string> subdirectories(const std::string& dir)
{
    std::vector<std::string> dirs;
    SDL_EnumerateDirectory(dir.c_str(), add_subdirectory, &dirs);
    return dirs;
}

std::vector<std::string> data_dirs()
{
    std::vector<std::string> dirs;
#ifdef SDL_PLATFORM_ANDROID
    // The player's own copy of the game, imported into app storage by the
    // launcher activity (port/android), or pushed by adb for development.
    if (const char* p = SDL_GetAndroidInternalStoragePath()) dirs.push_back(std::string(p) + "/game");
    if (const char* p = SDL_GetAndroidExternalStoragePath()) dirs.push_back(std::string(p) + "/game");
#endif
    return dirs;
}

void set_text_input(bool on)
{
    if (g_headless || g_window == nullptr) return;
    if (on == SDL_TextInputActive(g_window)) return;
    if (on) SDL_StartTextInput(g_window);
    else SDL_StopTextInput(g_window);
}

bool text_input_on_demand()
{
#ifdef SDL_PLATFORM_ANDROID
    return true;    // the on-screen keyboard covers the game
#else
    return false;   // always on: names, and the cheat codes typed while playing
#endif
}

std::string pref_path(const char* org, const char* app)
{
    char* p = SDL_GetPrefPath(org, app);
    std::string result = p ? std::string(p) : std::string();
    SDL_free(p);
    return result;
}

namespace
{
#ifdef SDL_PLATFORM_WINDOWS
// SDL writes its log to stderr only when it can attach to the parent's console. A program whose
// stderr is redirected to a file or a pipe by a parent without a console (a test runner started as a
// service, Wine, a scheduled task) would lose every line: the port then writes it there itself.
SDL_LogOutputFunction g_sdlLog = nullptr;
void* g_sdlLogData = nullptr;

void LogOutput(void* data, int category, SDL_LogPriority priority, const char* message)
{
    struct _stat64 info;
    const int fd = _fileno(stderr);
    if (fd >= 0 && _fstat64(fd, &info) == 0 && ((info.st_mode & _S_IFREG) || (info.st_mode & _S_IFIFO)))
    {
        fprintf(stderr, "%s\n", message);
        fflush(stderr);
        return;
    }
    if (g_sdlLog) g_sdlLog(data, category, priority, message);
}
#endif

void PrepareLog()
{
#ifdef SDL_PLATFORM_WINDOWS
    static bool done = false;
    if (done) return;
    done = true;
    g_sdlLog = SDL_GetDefaultLogOutputFunction();
    SDL_SetLogOutputFunction(LogOutput, g_sdlLogData);
#endif
}
}  // namespace

void log_debug(const char* text) { PrepareLog(); SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "%s", text); }
void log_info(const char* text) { PrepareLog(); SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "%s", text); }
void set_verbose(bool verbose)
{
    PrepareLog();
    SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, verbose ? SDL_LOG_PRIORITY_DEBUG : SDL_LOG_PRIORITY_INFO);
}
}  // namespace blupi::host

#ifdef SDL_PLATFORM_EMSCRIPTEN
// The page's pasted text (init's Module.blupiPaste) comes in through this buffer.
EM_JS_DEPS(blupi_host_paste, "$stringToUTF8");
extern "C" EMSCRIPTEN_KEEPALIVE char* blupi_host_paste_buffer()
{
    static char buffer[blupi::host::kPasteBuffer];
    return buffer;
}
extern "C" EMSCRIPTEN_KEEPALIVE void blupi_host_pasted()
{
    blupi::host::paste_arrived(blupi_host_paste_buffer());
}
#endif

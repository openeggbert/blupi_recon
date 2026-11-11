// Windows entry only (README "Windows"). The historical WinMain and game TUs stay unchanged;
// Free API owns the window, timer and MIDI, Free Direct graphics, sound and networking.
//
// Before the original WinMain runs, this sets defaults from the folder the executable sits in,
// so that a double click starts the game as packaged. Variables already set in the environment
// win, so every default can be overridden:
// - a folder "hires" with IMAGE08/IMAGE16 laid out like the game's: Free API's image overlay and
//   Free Direct's backing scale at the pack's scale, which is read from the images themselves
//   (an image of the pack against the game's own image of the same name);
// - the window scale: the largest that fits the primary display (windowed mode only; in the
//   game's full screen the display's resolution is used anyway);
// - the games' own -nocd switch: Free API has no CD-ROM drive (as in web/entry.cpp).
// The working directory becomes the executable's folder, as a shortcut's "Start in" would make
// it: the games open "data\\config.def" and their other files relative to it.
#include <windows.h>
#include <SDL3/SDL.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int);
extern "C" int _wchdir(const wchar_t* path);  // msvcrt; Free API's direct.h has no wide variant

namespace {

// Width and height from a BMP's header (.BLP files are BMPs).
bool ReadBmpSize(const std::string& path, std::int32_t& width, std::int32_t& height)
{
    unsigned char header[26] = {};
    SDL_IOStream* file = SDL_IOFromFile(path.c_str(), "rb");
    if (!file) {
        return false;
    }
    const bool read = SDL_ReadIO(file, header, sizeof(header)) == sizeof(header);
    SDL_CloseIO(file);
    if (!read || header[0] != 'B' || header[1] != 'M') {
        return false;
    }
    auto le32 = [&](int at) {
        return static_cast<std::int32_t>(header[at] | header[at + 1] << 8 | header[at + 2] << 16 |
                                         static_cast<std::uint32_t>(header[at + 3]) << 24);
    };
    if (le32(14) == 12) {  // BITMAPCOREHEADER
        width = header[18] | header[19] << 8;
        height = header[20] | header[21] << 8;
    } else {
        width = le32(18);
        height = le32(22);
    }
    if (height < 0) {
        height = -height;
    }
    return width > 0 && height > 0;
}

// The pack's scale: the first of its images whose size is an exact multiple (2..8) of the
// game's own image of the same name. 0 if there is none. The Windows file system ignores case.
int HiresScale(const std::string& base)
{
    for (const char* folder : {"IMAGE16", "IMAGE08"}) {
        const std::string packFolder = base + "hires\\" + folder;
        int count = 0;
        char** names = SDL_GlobDirectory(packFolder.c_str(), "*", SDL_GLOB_CASEINSENSITIVE, &count);
        if (!names) {
            continue;
        }
        int scale = 0;
        for (int i = 0; i < count && scale == 0; ++i) {
            std::int32_t packWidth = 0, packHeight = 0, width = 0, height = 0;
            if (!ReadBmpSize(packFolder + "\\" + names[i], packWidth, packHeight) ||
                !ReadBmpSize(base + folder + "\\" + names[i], width, height)) {
                continue;
            }
            if (packWidth % width == 0 && packWidth / width == packHeight / height &&
                packHeight % height == 0 && packWidth / width >= 2 && packWidth / width <= 8) {
                scale = packWidth / width;
            }
        }
        SDL_free(names);
        if (scale != 0) {
            return scale;
        }
    }
    return 0;
}

// The largest window scale (1..maximum) whose 640x480 client area fits the primary display.
int FittingWindowScale(int maximum)
{
    int scale = 1;
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        return scale;
    }
    SDL_Rect usable{};
    if (SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(), &usable)) {
        // Leave room for the title bar and borders.
        while (scale < maximum && 640 * (scale + 1) <= usable.w && 480 * (scale + 1) + 64 <= usable.h) {
            ++scale;
        }
    }
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    return scale;
}

// SDL's paths are UTF-8; the C runtime's narrow paths use the ANSI code page.
void ChangeDirectory(const std::string& path)
{
    if (wchar_t* wide = reinterpret_cast<wchar_t*>(
            SDL_iconv_string("UTF-16LE", "UTF-8", path.c_str(), path.size() + 1))) {
        _wchdir(wide);
        SDL_free(wide);
    }
}

void SetDefault(const char* name, const std::string& value)
{
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), name, value.c_str(), false);
}

}  // namespace

int main(int argc, char** argv)
{
    const char* basePath = SDL_GetBasePath();
    const std::string base = basePath ? basePath : "";

    if (!SDL_GetPathInfo((base + "DATA").c_str(), nullptr)) {
        const std::string message = "The game data (folders DATA, IMAGE08, IMAGE16, SOUND) must be in "
                                    "the folder of this program:\n" + base;
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Game data not found", message.c_str(), nullptr);
        return 1;
    }
    ChangeDirectory(base);

    const bool userOverlay = SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "FREE_API_IMAGE_OVERLAY") != nullptr;
    const int scale = userOverlay ? 0 : HiresScale(base);
    if (scale != 0) {
        const std::string value = std::to_string(scale);
        SetDefault("FREE_API_IMAGE_OVERLAY", "hires");
        SetDefault("FREE_API_IMAGE_SCALE", value);
        SetDefault("FREE_DIRECT_BACKING_SCALE", value);
        SetDefault("FREE_API_HIGH_PIXEL_DENSITY", "1");
        if (!SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "FREE_API_WINDOW_SCALE")) {
            SetDefault("FREE_API_WINDOW_SCALE", std::to_string(FittingWindowScale(scale)));
        }
    }

    std::vector<char*> arguments(argv, argv + argc);
    bool nocd = false;
    for (int i = 1; i < argc; ++i) {
        nocd = nocd || SDL_strcasecmp(argv[i], "-nocd") == 0;
    }
    char nocdArgument[] = "-nocd";
    if (!nocd) {
        arguments.push_back(nocdArgument);
    }
    arguments.push_back(nullptr);
    return FreeApiRunWinMain(&WinMain, static_cast<int>(arguments.size() - 1), arguments.data());
}

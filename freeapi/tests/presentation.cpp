// Public-API regression: the game's cursor updates use ClientToScreen + a
// clipped primary Blt. Partial updates must equal a full back-buffer copy
// even when the SDL window differs from the 640x480 virtual desktop, also
// with Free Direct's backing scale for higher-resolution images.
#include <windows.h>
#include <ddraw.h>
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstring>

static int failures = 0;
#define CHECK(condition) do { if (!(condition)) { std::fprintf(stderr, "%d: %s\n", __LINE__, #condition); ++failures; } } while (0)

static LRESULT WINAPI WindowProc(HWND window, UINT message, WPARAM w, LPARAM l)
{
    return DefWindowProcA(window, message, w, l);
}

static LPDIRECTDRAWSURFACE Surface(LPDIRECTDRAW dd, bool primary)
{
    DDSURFACEDESC desc{};
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
    desc.dwWidth = 640;
    desc.dwHeight = 480;
    desc.ddsCaps.dwCaps = primary ? DDSCAPS_PRIMARYSURFACE : DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
    desc.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
    desc.ddpfPixelFormat.dwFlags = DDPF_RGB;
    desc.ddpfPixelFormat.dwRGBBitCount = 32;
    LPDIRECTDRAWSURFACE surface = nullptr;
    CHECK(dd->CreateSurface(&desc, &surface, nullptr) == DD_OK);
    return surface;
}

static void Fill(LPDIRECTDRAWSURFACE surface, RECT* rect, DWORD color)
{
    DDBLTFX fx{};
    fx.dwSize = sizeof(fx);
    fx.dwFillColor = color;
    CHECK(surface->Blt(rect, nullptr, nullptr, DDBLT_COLORFILL, &fx) == DD_OK);
}

static void CopyToScreen(HWND window, LPDIRECTDRAWSURFACE primary, LPDIRECTDRAWSURFACE back, RECT rect)
{
    RECT screen = rect;
    CHECK(ClientToScreen(window, reinterpret_cast<LPPOINT>(&screen)));
    CHECK(ClientToScreen(window, reinterpret_cast<LPPOINT>(&screen) + 1));
    CHECK(primary->Blt(&screen, back, &rect, DDBLT_WAIT, nullptr) == DD_OK);
}

// Runs the cursor-update comparison with Free Direct's backing scale (1: the
// original pixels; 2: higher-resolution images, docs in ../README.md). The
// game's coordinates are the same in both; with scale 2 every buffer holds
// 1280x960 physical pixels, all of which must match.
static void RunAtBackingScale(HWND window, int scale)
{
    char value[8];
    std::snprintf(value, sizeof(value), "%d", scale);
    SDL_setenv_unsafe("FREE_DIRECT_BACKING_SCALE", value, 1);
    LPDIRECTDRAW dd = nullptr;
    CHECK(DirectDrawCreate(nullptr, &dd, nullptr) == DD_OK);
    SDL_unsetenv_unsafe("FREE_DIRECT_BACKING_SCALE");
    CHECK(dd->SetCooperativeLevel(window, DDSCL_NORMAL) == DD_OK);
    LPDIRECTDRAWSURFACE primary = Surface(dd, true), back = Surface(dd, false), snapshot = Surface(dd, false);
    LPDIRECTDRAWCLIPPER clipper = nullptr;
    CHECK(dd->CreateClipper(0, &clipper, nullptr) == DD_OK);
    CHECK(clipper->SetHWnd(0, window) == DD_OK);
    CHECK(primary->SetClipper(clipper) == DD_OK);
    SDL_Window* sdlWindow = reinterpret_cast<SDL_Window*>(window);
    SDL_Renderer* renderer = SDL_GetRenderer(sdlWindow);
    CHECK(SDL_SetRenderLogicalPresentation(renderer, 640, 480, SDL_LOGICAL_PRESENTATION_LETTERBOX));

    for (const auto& size : {SDL_Point{640,480}, SDL_Point{1280,960}, SDL_Point{945,708}, SDL_Point{1280,720}, SDL_Point{320,240}}) {
        CHECK(SDL_SetWindowSize(sdlWindow, size.x, size.y));
        SDL_PumpEvents();
        RECT oldCursor{70,92,134,152}, newCursor{300,240,364,300};
        Fill(back, nullptr, 0x00224466);
        Fill(back, &oldCursor, 0x0000ffff);
        CopyToScreen(window, primary, back, RECT{0,0,640,480});
        Fill(back, &oldCursor, 0x00224466);
        Fill(back, &newCursor, 0x0000ffff);
        CopyToScreen(window, primary, back, oldCursor);
        CopyToScreen(window, primary, back, newCursor);
        CHECK(snapshot->Blt(nullptr, primary, nullptr, DDBLT_WAIT, nullptr) == DD_OK);
        DDSURFACEDESC actual{}, expected{};
        actual.dwSize = sizeof(actual); expected.dwSize = sizeof(expected);
        CHECK(snapshot->Lock(nullptr, &actual, 0, nullptr) == DD_OK);
        CHECK(back->Lock(nullptr, &expected, 0, nullptr) == DD_OK);
        CHECK(actual.dwWidth == 640 && actual.dwHeight == 480);
        CHECK(actual.lPitch == 640 * 4 * scale);
        int different = 0;
        for (int y=0; y<480*scale; ++y) {
            auto* a = reinterpret_cast<const unsigned char*>(actual.lpSurface) + y*actual.lPitch;
            auto* e = reinterpret_cast<const unsigned char*>(expected.lpSurface) + y*expected.lPitch;
            for (int x=0; x<640*scale; ++x) if (std::memcmp(a+4*x, e+4*x, 4)) ++different;
        }
        std::printf("scale %d, window %dx%d: %d mismatched pixels after cursor move\n", scale, size.x, size.y, different);
        CHECK(different == 0);
        CHECK(snapshot->Unlock(nullptr) == DD_OK); CHECK(back->Unlock(nullptr) == DD_OK);
    }
    snapshot->Release(); back->Release(); primary->Release(); clipper->Release(); dd->Release();
}

int main()
{
    SDL_setenv_unsafe("SDL_VIDEODRIVER", "dummy", 1);
    SDL_setenv_unsafe("SDL_RENDER_DRIVER", "software", 1);
    SDL_setenv_unsafe("FREE_DIRECT_ENABLE_VSYNC", "0", 1);
    WNDCLASSA wc{};
    wc.lpfnWndProc = WindowProc;
    wc.lpszClassName = "BlupiPresentationRegression";
    CHECK(RegisterClassA(&wc));
    HWND window = CreateWindowExA(0, wc.lpszClassName, "presentation test", WS_VISIBLE,
                                 0, 0, 640, 480, nullptr, nullptr, nullptr, nullptr);
    CHECK(window != nullptr);
    for (int scale : {1, 2}) RunAtBackingScale(window, scale);
    DestroyWindow(window);
    return failures ? 1 : 0;
}

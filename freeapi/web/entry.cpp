// Browser bridge only. The historical WinMain and game TUs stay unchanged.
// Free API owns the window, timer and MIDI; Free Direct owns graphics/audio.
#include <windows.h>
#include <emscripten.h>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int);
extern UINT g_lastPhase;

// Read-only runtime evidence for browser smoke checks; no injected game state.
extern "C" EMSCRIPTEN_KEEPALIVE unsigned BlupiWebPhase()
{
    return g_lastPhase;
}

extern "C" BOOL WINAPI __real_PeekMessageA(LPMSG, HWND, UINT, UINT, UINT);
extern "C" BOOL WINAPI __wrap_PeekMessageA(LPMSG msg, HWND window, UINT first, UINT last, UINT remove)
{
    // Yield outside Free API's locks so DOM input, rendering and audio run.
    // Asyncify preserves the original WinMain's polling-loop stack.
    emscripten_sleep(1);
    return __real_PeekMessageA(msg, window, first, last, remove);
}

extern "C" BOOL WINAPI __wrap_WaitMessage()
{
    emscripten_sleep(10);
    return TRUE;
}

int main(int, char**)
{
    // The original GetCurrentDir can find bundled game data in the VFS root.
    char executable[] = "/game";
    char nocd[] = "-nocd";
    char* argv[] = {executable, nocd, nullptr};
    return FreeApiRunWinMain(&WinMain, 2, argv);
}

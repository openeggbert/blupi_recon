# The port in the browser (Emscripten), since 1.4.0

The SDL3 port's five programs build to WebAssembly with Emscripten:

- `blupi-sdl3` (Speedy Blupi 1 / Speedy Eggbert 1);
- `blupi2-sdl3` (Speedy Blupi 2 / Speedy Eggbert 2);
- `eggbert-demo-sdl3`, `eggbert-value-sdl3`, `blupi-demo-sdl3` ([EDITIONS.md](EDITIONS.md)).

They are the same programs as on the desktop. The historical TUs and the port's code are compiled
unchanged, apart from the few points below. This is separate from the repository's Free API web build
(`../../freeapi/`), which compiles the historical platform TUs themselves and bundles the game data.

**No game data is in the web build.** As on the desktop, the player uses their own copy of the game:

1. **Choose the folder.** The page asks for the game's folder (the browser's folder picker). It may
   be the game folder itself, or a folder that contains it, like a CD.
2. **Copy the files into the browser.** The loader (`port/web/loader.js`) finds the game's root:
   the shortest folder with `DATA/config.def` and the edition's title image. It copies the folders
   the port reads into the browser's storage: IndexedDB, through Emscripten's IDBFS mounted at
   `/persist/<program>/data`.
   - **Copied:** `DATA`, `IMAGE`, `IMAGE08`, `IMAGE16`, `SOUND`, `MUSIC`, a CD's language folders'
     `SOUND`, and the root's executables (read only for their size and hash: the installed
     language).
   - **Not copied:** movies and anything else.

   Nothing is uploaded.
3. **Start the program** with `--data /persist/<program>/data --user /persist/<program>/user`.
   - The saves and the settings file go to the user folder, which is written back to IndexedDB every
     5 seconds, when the page is hidden, and when the game ends.
   - The next visit starts at once.
   - "Remove the stored game files" deletes the copy.

## What differs from the desktop

- **The main loop yields to the browser.** It is the launcher's ordinary loop, linked with
  `-sASYNCIFY`: each wait for the next tick (`SDL_WaitEventTimeout`) yields to the browser, which
  then draws the canvas and delivers input.
- **Multiplayer only through a Free Direct server** (since 1.5.0, port/docs/MULTIPLAYER.md). A browser
  has no UDP, so the provider list has one entry, "Internet through a Free Direct server", which uses the
  browser's WebSocket (`common/net/websocket.cpp`). The page's `?server=` and `?room=` fill the game's
  server screen; when the game creates a room, the page shows a link with both, for the other players.
  A page served over `https://` (GitHub Pages) can only reach a server over `wss://`, that is a server
  with a TLS certificate or behind a TLS proxy; there a plain name means `wss://` on port 443 (since
  1.5.1). The page offers `fds.openeggbert.com` (`DEFAULT_SERVER` in `web/loader.js`), run as described
  in [FREE_DIRECT_SERVER.md](FREE_DIRECT_SERVER.md). Desktop and browser players can share a room. [EVIDENCE] `port/tools/web_check.mjs
  --relay`: Chrome hosts, a native Linux program joins, both reach PLAY.
- **No folder dialog in the program** (`host::choose_folder` returns nothing). The page does it
  before the start.
- **The SoundFont** (GeneralUser GS) is a file of the site (`soundfont/soundfont.sf2`). The loader
  puts it at `/soundfont.sf2`, the executable's folder, where the launcher looks for it.
- **Audio** starts after the click on "Play", which browsers require.
- **Not tested:** touch screens, the on-screen keyboard and gamepads in the browser. SDL3's
  Emscripten backend provides them.

## Building

With an activated Emscripten SDK (tested with emcc 6.0.9) and the SDL 3.4.16 sources at
`build/third_party/SDL3-3.4.16`:

```bash
source ~/emsdk/emsdk_env.sh
export CCACHE_DIR=/rv/cnaccache CCACHE_BASEDIR=/rv
emcmake cmake -S build/third_party/SDL3-3.4.16 -B build/port-web/sdl3-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF \
  -DSDL_TEST_LIBRARY=OFF -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_INSTALL_PREFIX=$PWD/build/port-web/sdl3
cmake --build build/port-web/sdl3-build -j 4 && cmake --install build/port-web/sdl3-build
emcmake cmake -S port -B build/port-web/port -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DSDL3_DIR=$PWD/build/port-web/sdl3/lib/cmake/SDL3 -DBLUPI_BUNDLE_SOUNDFONT=OFF \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
cmake --build build/port-web/port -j 4
python3 port/tools/package_web.py     # build/port-web/site and build/release/...-web.zip (--no-zip: the site only)
python3 build/port-web/site/serve.py  # http://localhost:8080/
```

**What the build produces:**

- **Each program:** `<program>.js` and `<program>.wasm`. The link options are in
  `cmake/package_app.cmake`.
- **The tests:** the ctest suite is not built for Emscripten. The desktop suite covers the same code.
- **`package_web.py`:** assembles the site with the loader, the programs, the SoundFont, the notices,
  `serve.py` and `README.txt`, and zips it.

## Test

`port/tools/web_check.mjs` (Node 22+, Chrome, no npm packages) serves the site, and for the test only
a game folder as a listing (the loader's `test-import`). For each program it then:

1. copies the files into IndexedDB and starts the program;
2. checks the title screen on the canvas, and with `--attract` the attract-mode demo after it;
3. reloads the page and checks that the program starts again from the stored files.

```bash
~/emsdk/node/24.19.0_64bit/bin/node port/tools/web_check.mjs --site build/port-web/site \
  --output build/port-web/check --attract --program blupi-sdl3=/path/to/Speedy_Blupi_I ...
```

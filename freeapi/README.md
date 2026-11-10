# Free API builds: native and web

Builds the historical source **unchanged** (all TUs of `../src`, or of `../games/sb2/src`) against
[Free API](https://github.com/openeggbert/free-api) (a WinAPI subset) and
[Free Direct](https://github.com/openeggbert/free-direct) (a DirectX 3 subset), both over SDL3.
Where the SDL3 port (`../port`) replaces the seven platform TUs with port code, this build runs the
reconstructed `blupi.cpp`, `pixmap.cpp`, `ddutil.cpp`, `sound.cpp`, `movie.cpp`, `network.cpp` and
`misc.cpp` themselves. It changes nothing the matching build (`../tools`) or the port produces.

Status (2026-10-24, prototype): Speedy Blupi 1, Speedy Blupi 2 and Speedy Eggbert 2 build on
Linux x86-64 and reach the title, the menus, the world-1 hub and a mission with MIDI music; for
Speedy Blupi 1 also the player file written to `DATA`, and 8-bit as well as 16-bit colour.
Speedy Blupi 1's choose-player screen and Speedy Blupi 2's title are pixel-identical to the SDL3
port's, which is itself frame-identical to the rebuilt original (`../port/docs/ORACLE.md`); only
the cursor differed, through the timing of the scripted mouse.
The web build adds all four English editions (see below). The historical source,
matching tools and SDL3 port are unchanged.

## Requirements

Sibling checkouts `../../free-api` and `../../free-direct` with their blupi_recon support,
CMake ≥ 3.23, Ninja, GCC with C++20, and the SDL3, SDL3_image and SDL3_mixer
development files (Free API's `FREE_API_USE_SYSTEM_SDL3`).

## Build

```bash
cmake -S freeapi -B build/freeapi-linux -G Ninja                                # Speedy Blupi 1 (US)
cmake -S freeapi -B build/freeapi-se1-linux -G Ninja -DBLUPI_EDITION=eggbert     # Speedy Eggbert 1 (English)
cmake -S freeapi -B build/freeapi-sb2-linux -G Ninja -DBLUPI_GAME=sb2           # Speedy Blupi 2 (English)
cmake -S freeapi -B build/freeapi-se2-linux -G Ninja -DBLUPI_GAME=sb2 \
      -DBLUPI_EDITION=eggbert                                                 # Speedy Eggbert 2
cmake --build build/freeapi-linux
```

One game per build directory, because Free API compiles one string table in. The edition selects
`_EGAMES` and its resource script. Blupi 2 also stages the whole-file overlay in
`games/sb2/blupi/`, including its headers, in the build directory. `BLUPI_DEFINES` are
a target's build-wide defines (`targets.json`, e.g. `LANGUAGE_DE=1` for German Speedy Blupi 1) and
`BLUPI_RC_FILE` its resource script (`src/res/*.rc`, `games/sb2/res/*.rc`).

## Run

Like the original: the executable sits in the game's folder (`DATA`, `IMAGE08`, `IMAGE16`,
`SOUND`) and runs from there, because the game finds its folder from its own path. Speedy Blupi 1.0
and 2 insist on their CD unless given their own `-nocd` switch (Free API has no CD-ROM drive). The
game writes its player and saved-game files into `DATA`, so use a copy of the game:

```bash
mkdir game && cp -r "$GAME/DATA" game/ && ln -s "$GAME"/{IMAGE08,IMAGE16,SOUND} game/
ln -s ../blupi-freeapi game/ && cd game
FREE_API_SOUNDFONT=/path/to/soundfont.sf2 ./blupi-freeapi -nocd
```

`FullScreen=0` in `DATA/config.def` gives a 640×480 window. Music needs a SoundFont
(`FREE_API_SOUNDFONT`, or `soundfont/default.sf2`; the GeneralUser GS file of
`../port/third_party/generaluser-gs` works).

## Higher-resolution images

The games can show images at twice (or up to four times) the original resolution, for example
sprites re-rendered from the original 3D models (`../../sprite-utils/plan.md`). The historical
source does not change. The game keeps computing in its 640×480 pixels. Only the compatibility
libraries know about the higher resolution:

- **Free Direct** (`FREE_DIRECT_BACKING_SCALE=k`): every DirectDraw surface keeps a pixel buffer
  `k` times larger on each axis, while surface sizes, blit rectangles and GDI coordinates stay
  logical. The logical presentation stays 640×480, so the mouse is unaffected.
- **Free API** (`FREE_API_IMAGE_OVERLAY=<pack>`, `FREE_API_IMAGE_SCALE=k`, default 2):
  `LoadImageA` takes an image from the pack when it is exactly `k` times the original's width and
  height. `GetObjectA` then reports the original's size. Any other size is ignored with one log
  line, and the original loads.

Details: `../../free-direct/docs/directdraw-limitations.md` (backing scale) and
`../../free-api/docs/image-overlay.md` (overlay, scaled DCs, window).

**Native run.** Run from the game folder as above:

```bash
FREE_DIRECT_BACKING_SCALE=2 FREE_API_WINDOW_SCALE=2 FREE_API_IMAGE_OVERLAY=/path/to/pack \
    ./blupi-freeapi -nocd
```

- `FREE_API_WINDOW_SCALE=2` opens a 1280×960 window.
- On a high-DPI screen, use `FREE_API_HIGH_PIXEL_DENSITY=1` instead.
- In full screen the display's resolution is used anyway.
- Without a pack, `FREE_DIRECT_BACKING_SCALE=2` shows the original images enlarged. Missing pack
  files fall back the same way, so a pack may be incomplete.

**Web.** Configure an edition with `-DBLUPI_WEB_HIRES_PACK=<pack>` (and `-DBLUPI_WEB_HIRES_SCALE`,
default 2), or give `build_web.sh` `BLUPI_WEB_HIRES_ROOT` containing one pack per edition
directory (`sb1-blupi/`, …).

- The pack is staged as `/hires`, and the page then starts at its scale.
- `?scale=1` shows the original pixels.
- `?scale=2` without a pack shows enlarged originals.
- The canvas uses the browser's device-pixel ratio. A 2× image shows its detail in full screen
  or on a high-DPI screen.

**What a pack must contain:**

- **Layout:** the game's folder layout (`IMAGE08/OBJECT.BLP`, `IMAGE16/DECOR000.BLP`, …). Names
  may differ in case, or use `.bmp`/`.png` for `.blp`.
- **Size:** each image exactly `k` times the size of the **file** it replaces, for example Speedy
  Blupi 1's `OBJECT.BLP` 896×987 → 1792×1974. Every icon rectangle of the sheet must also be
  exactly `k` times larger (`sprite-utils` `--scale`/`pack` produce this).
- **Colour mode:** use 24-bit images (8-bit works too) for the 16-bit colour mode. In 8-bit mode
  the images are reduced to the game palette.
- **Transparency:** either of two forms, also mixed within a pack.
  - **Alpha channel:** a 32-bit BMP with an alpha mask (or PNG, natively; the web build's
    SDL_image has no PNG codec). Edges may be soft and anything may be semi-transparent: Free
    Direct blends it over the background. Use straight (not premultiplied) alpha.
  - **Colour key, as in the originals:** hard edges on pure blue `RGB(0,0,255)`. Anti-aliased edges
    against blue show blue fringes.

  In both forms:
  - The games' colour key still applies. Opaque pixels must avoid the key, which in 16-bit mode is
    every colour with R < 8, G < 4 and B ≥ 248.
  - The "hide" power-up's mask icons (object sheet, icons 0xED and 0xF5) keep their pure red.
    Their other part may be blue or transparent; both give the same result.
  - 8-bit mode cannot blend; it draws pixels with alpha ≥ 128.
- **Fonts:** `TEXT.BLP`/`LITTLE.BLP` glyphs must stay inside their 2× cells. Character widths come
  from the game's tables.

**Checks** (tools under `tools/`):

- **Synthetic pack:** `make_test_pack.py --game <installation> --output build/<dir>` writes a pack
  that merely enlarges every image (nearest neighbour). Frames with it must equal frames without
  it at the same scale.
  - `--alpha` writes the blue-keyed sprite sheets as 32-bit BMPs in which every key pixel is
    transparent black. Frames must still be the same, now through the alpha path.
  - `--sheets-only` limits the pack to those sheets.
- **Frame capture:** `FREE_DIRECT_CAPTURE_PRIMARY=<dir>` (with `FREE_DIRECT_CAPTURE_LIMIT`) saves
  every changed presented frame.
- **Comparison:** `compare_frames.py upscale <1x> <2x>` checks that each 2× frame is an exact 2×
  enlargement of a 1× frame. `compare_frames.py same <a> <b>` compares two captures.

[EVIDENCE] 2026-11-10, Linux, SDL dummy video driver, Free API `176765b` and Free Direct `c97fece`
plus this change. The game was left idle on its title until the attract demo started (about
50 s); 40 frames each:

| Check | Speedy Blupi 1 | Speedy Eggbert 2 |
|---|---|---|
| 2× without a pack: exact 2× enlargements of 1× frames | 40 of 40 | 38 of 40 (see below) |
| 2× with the synthetic pack equals 2× without it | 40 of 40 (17 images from the pack) | 40 of 40 (18 images from the pack) |
| 2× with the alpha pack (`--alpha --sheets-only`) equals 2× without it | 30 of 30 (13 sheets with alpha) | 30 of 30 (13 sheets with alpha) |

In Eggbert 2, two start-up frames contain a 90×90 area under the software cursor whose odd
physical columns hold the next logical pixel (a one-pixel horizontal offset at 2×). It is
reproducible, the same with and without a pack, and gone from the third frame on. Its cause is
not yet found; no blit there has unequal source and destination sizes. The minimap (`DrawMap`)
stretches at the physical resolution, so it is not an exact enlargement either; the captured
frames did not reach it.

Other checks:

- `blupi_presentation_test` passes at scales 1 and 2 for all five window sizes.
- The Free Direct and Free API test suites pass, with new tests for the backing scale, overlay,
  scaled DCs and window scale.
- The web smoke check (`--navigate true`) passed for all four editions at scale 1 and for Speedy
  Blupi 1 and Speedy Eggbert 2 at `?scale=2`: title → players → world, demo, audio and saves,
  with a 1280×960 back buffer.
- A web package with a marked 2× `IMAGE16/INIT.BLP` showed its one-pixel lines on the title,
  which a 1× image cannot hold.
- With `BLUPI000.BLP` made 50 % transparent, Blupi and the lives in the HUD are drawn blended
  with the background, without a box around them.
- After the alpha support, the web smoke check again passed for all four editions at scale 1 and
  for Speedy Eggbert 2 at `?scale=2`.
- Real 2× sheets: Speedy Eggbert 1 (`build/freeapi-se1-linux`) ran on a Wayland desktop in a
  1280×960 window with `../../speedyblupi-hd/games/Speedy Eggbert/Game/2x` (Ray Dream re-renders
  and upscaled fallbacks). All 21 images it loaded came from the pack, none was rejected, and the
  game quit normally.
- Real 4× sheets: Speedy Eggbert 2 ran at `FREE_DIRECT_BACKING_SCALE=4`/`FREE_API_IMAGE_SCALE=4`
  with `speedyblupi-hd`'s `4x` pack, using the game's full screen (`FullScreen=1`) and
  `FREE_API_HIGH_PIXEL_DENSITY=1`, on a 2560×1440 monitor.
  - The back buffer was 2560×1920 and was shown in real pixels.
  - The game ran at 20 frames per second, using about 0.5 GB of memory.
  - 24 images came from the pack.
  - A 4× window would not fit such a screen; full screen does (see "Cursor and fullscreen
    coordinates" for the full-screen fix it needed).

## Web: build all four games

From the repository root, with CMake ≥ 3.24, Ninja, and an activated Emscripten SDK:

```bash
source /path/to/emsdk/emsdk_env.sh
bash freeapi/tools/build_web.sh
python3 freeapi/tools/serve_web.py
```

Open **http://localhost:8080/**. Select a game and press **Start game**. Graphics, levels,
recorded demos, sound effects and MIDI files load automatically from the web package. The shared
SoundFont is also included; no game-folder or SoundFont picker is needed. The button provides
the browser interaction needed to start audio. There is no upload endpoint.

As explicitly requested by the user, the default read-only build inputs are under
`/rv/tmp/speedy-blupi-games`:

| Web game | Installation directory |
|---|---|
| `sb1-blupi` | `Speedy_Blupi_I/` |
| `sb1-eggbert` | `Speedy Eggbert/Game/` |
| `sb2-blupi` | `Speedy_Blupi_II/` |
| `sb2-eggbert` | `Speedy_Eggbert_2/` |

`tools/stage_web_data.py` copies supported assets from `DATA`, `IMAGE08`, `IMAGE16` and `SOUND`
into each build directory's `assets/` tree, normalizes names to lowercase, and records source
sizes and SHA-256 hashes in `assets/manifest.json`. DATA seeds contain official `WORLDxxx.BLP`,
`DEMOxxx.BLP` and `config.def`; installation-specific player profiles, user missions and benchmark
output are excluded. Original executable files are neither packaged nor run. The installations
are never modified. Only generated copies under ignored `build/` enter the package.

To use another installation root, set `BLUPI_WEB_DATA_ROOT` before running the build script.
For an individual CMake build, `BLUPI_WEB_DATA_DIR` overrides its installation path. The shared
GeneralUser GS SoundFont defaults to `../free-eggbert/assets/soundfont/default.sf2`, with its
adjacent `LICENSE.txt`. Set `BLUPI_WEB_SOUNDFONT` and `BLUPI_WEB_SOUNDFONT_LICENSE` to override them.

The script builds pinned SDL 3.4.0, SDL_image 3.4.0 and SDL_mixer 3.2.0 from upstream source
archives, verifies their SHA-256 hashes, and installs them in `build/freeapi-web/sdk`. It then
compiles the four games sequentially, with at most four compiler jobs. Free API and Free Direct
come from the sibling checkouts; `BLUPI_FREE_API_DIR` / `BLUPI_FREE_DIRECT_DIR` can override their
paths when configuring individual builds. The script also creates
`build/freeapi-web/blupi-four-games-web.zip`, including the game data, SoundFont and notices.

The static output is **`build/freeapi-web/site/`**:

| Directory | Executable |
|---|---|
| `sb1-blupi/` | Speedy Blupi 1, US, `_EGAMES=0` |
| `sb1-eggbert/` | Speedy Eggbert 1, English, `_EGAMES=1` |
| `sb2-blupi/` | Speedy Blupi 2, English, `_EGAMES=0`, Blupi overlay |
| `sb2-eggbert/` | Speedy Eggbert 2, English, `_EGAMES=1` |

Each game directory contains `index.html`, `game.js`, `game.wasm`, `game.data`, and the player UI.
The shared `soundfont/` directory contains MIDI instruments. Serve the entire
`site/` directory over HTTPS (or localhost). Static hosting, including GitHub Pages,
uses the shared browser bootstrap and isolation service worker described below.
Servers may instead supply these headers directly:

```text
Cross-Origin-Opener-Policy: same-origin
Cross-Origin-Embedder-Policy: require-corp
```

The included foreground server supplies the headers and the `application/wasm` MIME type.
Use `--port 9000` to change its port; Ctrl-C stops it. `file://` cannot run the threaded build.
The browser must support WebAssembly threads and WebGL, and permit service workers when
the server does not supply the isolation headers.

### GitHub Pages and other static hosting

`web/bootstrap.js` checks isolation before loading either the player or `game.js`. On a host
without COOP/COEP it registers the package-root `coi-serviceworker.js`, waits for activation,
and reloads the document. The worker adds COOP `same-origin` and COEP `require-corp` to fetched
responses. Its default scope covers the four game directories and SoundFont, including direct
links into a game, without controlling unrelated pages above the package directory. The worker
does not cache assets offline. A session-storage marker bounds automatic reloads; failures
display a retry button and never start the WASM runtime or leave a misleading download status.
On servers already providing isolation, the bootstrap loads the game directly.

The worker is the unmodified MIT-licensed upstream file from
https://github.com/gzuidhof/coi-serviceworker at commit
`7b1d2a092d0d2dd2b7270b6f12f13605de26f214` (v0.1.7), SHA-256
`e97bbac6017322d48aa54a5bc7ce473c725a4b7376ff245002e0ba71c3dcdd7e`.
Its license is retained in `web/coi-serviceworker.LICENSE.txt` and packaged as
`licenses/coi-serviceworker.txt`. It is registered as a worker only; its optional window-side
automatic-update/reload code is not used, so checking for updates cannot reload an active game.

To refresh the HTML/JavaScript shell and ZIP around existing binaries without recompiling C++:

```bash
python3 freeapi/tools/package_web.py --archive build/freeapi-web/blupi-four-games-web.zip
```

Publish the entire resulting `site/` directory, including both shared JavaScript files. The
GitHub Pages regression below serves it under `/SpeedyBlupiEggbert12/` without isolation headers;
each game starts in its own fresh browser context, then also reloads with its existing saves.

[EVIDENCE] Chrome 155, 2026-11-08: all four games passed cold direct links under
`/SpeedyBlupiEggbert12/` on a server sending no COOP/COEP headers. Each first start used exactly
two document requests (initial page and one controlled navigation) and one game-data request.
All four passed title/player/play navigation, recorded demos, WebAudio, save reload and backup
round trips, fullscreen/restore and editor input; Eggbert 2 also passed sustained motor audio.
A server already sending isolation headers still passed. Missing-worker (404) and active-worker
without isolation tests both showed a terminal error with Retry loading, did not load the WASM
runtime/data, and did not reload indefinitely. Reports are `build/freeapi-web/bundled-smoke/`,
`pages-smoke.log`, `pages-missing-worker-report.json` and `pages-reload-limit-report.json` under
`build/freeapi-web/`. These are bounded browser checks, not a historical matching proof.

### Browser adapter and saves

- `web/entry.cpp` supplies a strong `main(int, char**)`, calls Free API's WinMain bridge with
  the original `-nocd` option, and supplies an executable path in the virtual root. The full
  signature matters: Emscripten lowers `main()` differently, allowing Free API's weak
  argument-taking entry point to be selected instead.
- Link wrappers yield through [Asyncify](https://emscripten.org/docs/porting/asyncify.html)
  before `PeekMessageA` and instead of the native `WaitMessage` sleep. They yield outside the
  compatibility library's locks. Free API's original timer and MIDI threads run in a prestarted
  pthread pool. Rendering, input translation and sound still go through Free API / Free Direct.
- Emscripten preloads `/image08`, `/image16`, `/sound` and `/data_seed` from `game.data` before
  enabling Start. `/data_seed` initializes missing files in `/data`; existing browser saves
  take precedence. The shared SoundFont loads automatically. Only the virtual `config.def`
  copy is adjusted for windowed mode, the virtual CD root and MIDI instead of CD audio.
- Each game has its own [IDBFS](https://emscripten.org/docs/api_reference/Filesystem-API.html#filesystem-api-idbfs)
  database at `/persistent/<game>/data`. It loads before the game starts and flushes every three
  seconds, when the page is hidden, and with **Save to browser**. Keep the same site origin
  (including port) to retain access to its saves. Clearing site data removes them.
- **Export saves** downloads a JSON backup of DATA. **Import saves**, before starting the game,
  checks the format, game identity and paths, restores the files and flushes them. Game assets
  load automatically again after a page reload; no local files need to be selected.

Known scope: AVI movies and CD audio are unavailable, as in the native Free API build. Browser
multiplayer is unavailable: the default Free Direct loopback transport does not connect separate
WASM instances. The web UI is intended for a keyboard and mouse. Long campaign play, editor
coverage and other browser engines still need separate verification.

### Vehicle engine loops

[EVIDENCE] The original Eggbert 2 motor path uses `Play(0, 0, 1)` for helicopter/hovercraft
engines. Static disassembly of Eggbert2.exe confirms the looping flag at VA 0x42a892; the
reconstructed `sound.cpp` preserves it. The 0.3965 s / 0.5501 s motor samples were intact in the
web package, but Free Direct previously ignored `DSBPLAY_LOOPING` and played them once.

The fix is in Free Direct's DirectSound implementation: an SDL stream callback repeats static
PCM independently of the game's timer, with synchronized stopping and buffer destruction.
The library regression tests cover sustained looping, volume changes, stopping, switching to
a one-shot and releasing an active loop. Neither historical source nor the web adapter needs
an audio workaround.

`--navigate true --engine true` adds an Eggbert 2 browser check. It mutes MIDI through the
original settings screen, types the game's own `IOVERCRAFT` code, measures WebAudio RMS after
the start effect has finished, holds/releases Up to switch motor modes, then types `FUNSKATE`
to stop the motor. Quiet and stopped controls must be silent; each 2–3 second motor window
must contain nonzero audio throughout. These are ordinary keyboard/mouse events; game state
is not patched. Reports and screenshots use the `sb2-eggbert-engine` filename prefix.

[EVIDENCE] On 2026-11-08 the previous web build failed this check with zero audio throughout
the low/high motor windows. With the Free Direct fix, every sample of the 3 s low, 2 s high
and 2 s low-again windows had RMS above 0.03; quiet/stopped controls were exactly zero.
The native DirectSound, no-driver and DirectDraw/DirectSound integration suites passed.
All four rebuilt web editions also passed the regular smoke and presentation checks.
Before/after logs are `build/freeapi-web/engine-browser-{before,after}.log`; the current
per-window samples are in `build/freeapi-web/bundled-smoke/sb2-eggbert-engine.json`.

### Cursor and fullscreen coordinates

[EVIDENCE] The doubled software cursor was reproducible without a game or Wine. The original
game calls `ClientToScreen` before partial primary-surface blits. Free API scaled those points
to the physical window, while Free Direct only subtracted its position. A partial cursor
update therefore landed somewhere different from the full back-buffer copy after scaling.

The fix lives in the compatibility libraries: Free API's `src/winuser_cursor.cpp` uses SDL's
logical renderer conversions (including letterbox offsets) with matching integer rounding;
Free Direct's `src/directdraw/DirectDrawSurface.cpp` maps clipped primary destinations back
through `ScreenToClient`. Neither historical game source nor Wine is changed.

`tests/presentation.cpp` checks the actual public APIs with cursor-sized partial updates and
compares every primary-buffer pixel with the back buffer. Before the fix, the mismatched-pixel
counts at window sizes 640×480, 1280×960, 945×708, 1280×720 and 320×240 were respectively
0, 7680, 15513, 11280 and 8400. After the fix every case has zero differences. Build this native
fixture with `-DBLUPI_FREEAPI_PRESENTATION_TESTS=ON` and run `blupi_presentation_test`.

[EVIDENCE] The games' own full screen (`FullScreen=1` in `config.def`) was broken natively on any
display larger than 640×480: the frame was squeezed into the right part of the screen, and
stale pixels stayed on the left.
- **Cause.** In full screen the primary surface has no clipper, and `CPixmap::Display` and
  `MouseQuickDraw` blit at the rectangle `ClientToScreen` returns. Free API mapped it to the
  display-sized window instead of the game's 640×480 screen.
- **Fix.** Free API's `src/winuser_cursor.cpp` now treats the game's full screen as real
  DirectDraw does: the screen is the display mode, so `ClientToScreen`/`ScreenToClient` leave
  points unchanged, and `GetCursorPos`/`SetCursorPos` use the same logical coordinates.
- **Test.** `blupi_presentation_test` covers it: a full frame, then cursor updates, at backing
  scales 1 and 2. Without the fix it finds 17362 and 69448 mismatched pixels on the dummy
  1024×768 display; with the fix, none.
- **Real run.** Speedy Eggbert 2 ran at 4× in full screen on a 2560×1440 monitor, with
  `FREE_API_HIGH_PIXEL_DENSITY=1`, at 20 frames per second, with the whole frame shown.

[EVIDENCE] The web fullscreen button also bypassed SDL by calling the DOM fullscreen API
directly. The canvas pixel buffer and renderer viewport then disagreed, cropping the game,
including after returning to the window. `web/player.js` now uses the fullscreen bridge SDL
installs as `Module.requestFullscreen`, so SDL handles pixel-buffer sizing and restoration.

The browser check below supports `--presentation true`: it uses a real virtual widescreen
monitor, compares title-screen samples after two fullscreen/restore cycles, clicks the same
logical menu controls in fullscreen, and visits the editor while moving its software cursor.
`--dpr 2` repeats it on a high-DPI monitor. Screenshots and coordinate metrics are written beside
the regular smoke report. Native window/input regression tests and DirectDraw tests also pass.

[EVIDENCE] All four editions passed two fullscreen/restore cycles on a 1920×1080 virtual
monitor in Chrome 155. The framebuffer became 1920×1080 with a centered 1440×1080 game viewport;
the whole image and logical menu clicks survived both transitions. Each editor screenshot shows
one cursor and selection frame after movement through several disjoint areas. Speedy Blupi 1
also passed at device-pixel ratio 2, including fullscreen clicks and the editor. Evidence is in
`build/freeapi-web/bundled-smoke/*-presentation.json` and `*-editor-cursor.png`, with the high-DPI
run under `build/freeapi-web/storage-check/`.

### Browser smoke check

Node 22+ and Chrome are sufficient; no npm dependencies or local game folders are needed:

```bash
node freeapi/tools/smoke_web.mjs --navigate true --output build/freeapi-web/bundled-smoke
# Also exercise fullscreen, restoration, letterboxed input and the editor:
node freeapi/tools/smoke_web.mjs --navigate true --presentation true --output build/freeapi-web/bundled-smoke
# Repeat presentation checks at high DPI for one edition:
node freeapi/tools/smoke_web.mjs --presentation true --dpr 2 --output build/freeapi-web/storage-check sb1-blupi
# Verify sustained hovercraft audio and stopping in Speedy Eggbert 2:
node freeapi/tools/smoke_web.mjs --navigate true --engine true --output build/freeapi-web/storage-check sb2-eggbert
# Static hosting: cold direct links at the deployed subdirectory, no server isolation headers:
node freeapi/tools/smoke_web.mjs --headers false --cold true --prefix SpeedyBlupiEggbert12 --mount-prefix true --navigate true --output build/freeapi-web/bundled-smoke
```

The check starts a temporary localhost server and one headless browser, loads the packaged
assets without file pickers, captures title/player/world screens, navigates with mouse and keyboard events,
and checks IDBFS after reloading. `--navigate true` also starts the game's own recorded demo and
measures WebAudio samples. Its output and screenshots stay under `build/freeapi-web/smoke/`;
the browser and server close even on a test failure. This is runtime smoke evidence, not a
historical STRICT or equivalence proof. It also checks BMP and MIDI file signatures, the loaded
SoundFont, per-edition `game.data` requests and the absence of upload requests.

[EVIDENCE] Web smoke verification with bundled assets on 2026-11-08, Emscripten 6.0.9, Chrome 155 on Linux;
Free API `176765b` and Free Direct `5430a3d`, including the coordinate and motor-loop fixes
described above:

| Executable | Bundled data, no picker | Title → players → world | Recorded demo | Nonzero WebAudio | IDBFS reload | Backup import/export |
|---|---|---|---|---|---|---|
| Speedy Blupi 1 US | passed | passed | passed | passed | passed | passed |
| Speedy Eggbert 1 English | passed | passed | passed | passed | passed | passed |
| Speedy Blupi 2 English | passed | passed | passed | passed | passed | passed |
| Speedy Eggbert 2 English | passed | passed | passed | passed | passed | passed |

Each executable reached original phase IDs 1524 (title), 1545 (players) and 1525 (play), with
no captured JavaScript runtime exceptions. Escape returned through the menus; the film button
started an original `DEMO2xx.BLP` replay. Wrong-edition backups and `../` save paths were rejected.
The JSON report, request log and screenshots are in `build/freeapi-web/bundled-smoke/`. All
requests were GETs. Each game loaded its own `game.data` plus the shared SoundFont; staged data
and source-installation hashes matched after the build. The earlier native Eggbert 2 build also
passed, using the prototype's `_EGAMES=1` selection to check native-build compatibility.

[OPEN] These bounded checks do not certify complete campaign/editor coverage, MIDI playback
quality, other browser engines or pixel equality with a historical executable.

## What fills which gap

- `compat/msvc_crt.h` (forced include, this build only): the VC5 `rand`/`srand` generator, so
  decoration and demos replay as in the original, and a `FILE` with VC5's `_flag`, which
  `CEvent::CopyMission` tests (`file->_flag & _IOERR`). Same approach as the port's
  `port_forced.h` and `port_hist_stdio.h`.
- Free API: case-insensitive folders and textual `..` in paths, `_ftime`, `GetDriveType`,
  `GetCurrentDirectory`, `joyGetDevCaps`, `AdjustWindowRect`'s menu bar, a backslashed `_pgmptr`,
  string tables from these `.rc` files (numeric IDs, cp1252), MCI opened by device-type ID and
  `MCI_STOP`, and, on 64-bit hosts, the MCI parameter blocks and callback window that the 1998
  code passes through 32-bit `DWORD` casts (`src/sound.cpp:654-672`). Its `plan.md`, tasks
  TASK-BR-01..16.
- Free Direct: 16-bit (RGB565) surfaces, and with `FREE_DIRECT_HONOR_DISPLAY_BPP` (set here)
  surfaces in the display's depth instead of always 32 bits; without it `DDColorMatch`'s
  `1 << dwRGBBitCount` (`src/ddutil.cpp:375`) breaks every sprite's transparency. Also
  `DirectPlayEnumerate` and two `DPERR_*` names. Its `plan.md`, phase BR.

Known differences from the original: no AVI movies and no CD audio (both declined, as on a PC
without them), multiplayer only between Free Direct programs, and 8-bit colour converted through
GDI's nearest palette entry.

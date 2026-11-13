# Speedy Blupi SDL3 port 1.5.2

The native port of **Speedy Blupi** (Epsitec, 1998) and **Speedy Blupi 2** (2001), and of their
eGames editions **Speedy Eggbert** and **Speedy Eggbert 2**, for Windows, Linux, Android and web
browsers. It is
built from this repository's reconstruction of the original programs' source, compiled unchanged with
a modern compiler, plus a small platform layer on SDL3.

Version 1.5.2 lets the Windows, Linux and Android programs reach a Free Direct server over `wss://`
(TLS) as well. Version 1.5.1 gives the web page a server to play on, fds.openeggbert.com. Version 1.5 plays
multiplayer through a Free Direct server, without open ports and also in the browser,
where it can be played at https://openeggbert.github.io/blupi_recon/. Version 1.4 runs all five programs
in a web browser as well. Version 1.3 added the Speedy Eggbert Demo, Speedy Eggbert SE Valueware and the June 1998 Speedy Blupi
Demo as programs of their own, internet games from the game's own multiplayer screens, the gamepad as
the keyboard, and Speedy Blupi 2's CD music from tracks you rip yourself. Version 1.2 brought Speedy
Blupi 2 and Speedy Eggbert 2, and the original multiplayer of all four games.

**The original games' files are not included.** You need your own copy of the game: the CD or the
installed game. On the first start the port asks for its folder and remembers it.

## Since 1.5.2 (not yet released)

- **Speedy Blupi 2's CD music from Ogg Vorbis or FLAC files:** besides `music/track02.wav`, the
  track may be `music/track02.ogg` or `music/track02.flac`, which take far less room.
- **Multiplayer on poor networks:** a typed Internet address is now found and joined over slow or lossy
  links (before, one lost packet or a slow round trip could hide the session), and a few lost packets in a
  row no longer drop a player (a player is given up after 5 to 10 s without an answer, instead of 1.5 to
  3 s). The last moves of a player who leaves are no longer thrown away.
- **Android: a game survives a short trip to the background:** switching to another app no longer drops
  you from a multiplayer game; the connection is kept alive while the game is paused (tested on the
  desktop; not yet on a device).
- **The original Windows cursors with `MouseType=2` or `3`:** the games' own arrows, hourglass and
  hand from their resources, instead of the system's cursors, enlarged with the window.

## New in 1.5.2

- **`wss://` on Windows, Linux and Android:** type `wss://fds.openeggbert.com` on the server screen (or
  `--net-server wss://...`) and the game connects through the server's TLS front on port 443, like the web
  page. It helps on networks that let only port 443 out, and keeps a server password
  (`BLUPI_NET_PASSWORD`) off the network in clear. A plain name still means `ws://` on port 51325, so
  nothing changes for those who do not type `wss://`.
- **The certificate is checked:** it must be valid for the server's name and trusted by the system (on
  Windows, the system's own check; on Linux, the distribution's CA bundle; on Android, the system's
  certificates). `SSL_CERT_FILE` or `SSL_CERT_DIR` name other trusted certificates. A server whose
  certificate does not pass is refused with the reason (not trusted, another name, expired).
- TLS is Mbed TLS 3.6.7, compiled into the programs (`LICENSE-MbedTLS.txt`). The Windows programs also
  use the system's `bcrypt.dll` and `crypt32.dll`.

## New in 1.5.1

- **The web page has a server:** its multiplayer screen offers `fds.openeggbert.com` until you have used
  another server, so a browser game needs nothing typed but OK. Desktop, Android and browser players
  share its rooms.
- **A plain server name on an `https://` page means port 443** (`wss://`), where a TLS proxy serves it;
  1.5.0 used 51325 there too. The desktop and Android programs still use 51325 (`ws://`), and the server
  screen's hint names the port that applies.
- **Running a server for all of them:** `port/docs/FREE_DIRECT_SERVER.md`, with the systemd service and the
  nginx configuration for `wss://`.
- `--net-server-default ADDRESS`: the server screen's server when none is remembered (the web page uses it).

## New in 1.5

- **Multiplayer through a Free Direct server:** the multiplayer provider list has a third entry,
  "Internet through a Free Direct server". Every player connects out to a
  [free-direct-server](https://github.com/openeggbert/free-direct/blob/hires-images/docs/free-direct-server.md),
  which anyone can run, so nobody needs an open port. Type the server's address and leave the room empty:
  the game creates a room and shows its ID (also on the clipboard). The other players choose the same
  server and type that ID; the games in the room are then listed as on a local network. Windows, Linux,
  Android and browser players can share a room. The desktop and Android programs speak `ws://`; a server
  that only offers `wss://` (TLS) works from the browser. `--net-server` and `--net-room` fill the
  screen in advance.
- **Multiplayer in the browser,** through such a server (the browser has no other way). The page passes
  `?server=` and `?room=` to the game, and when the game creates a room the page shows a link to give to
  the other players. A page served over `https://`, like GitHub Pages, needs a server with `wss://`.
- **On GitHub Pages:** https://openeggbert.github.io/blupi_recon/ serves the web package of the latest
  release. As always, you choose the folder of your own copy of the game; nothing is uploaded.
- **Windows and Android packages again**, for the first time since 1.2.0 (1.4.0's were added to its
  release afterwards). The Windows package is cross-compiled on Linux with MinGW-w64. The Android app
  is signed with a new key (the 1.2.0 key is not available): **uninstall 1.2.0 before installing**; 1.4.0
  and later update in place.
- **Fixes:**
  - An unplayed Speedy Blupi 2, copied from the CD and never started, was not recognised: the port
    looked for `DATA/BENCH.BLP`, which the game only writes on its first run. It now looks for the first
    attract-mode demo, which every Speedy Blupi 2 and Speedy Eggbert 2 has.
  - The settings file's gamepad keys (1.3) and the remembered Internet address were ignored, and a later
    save of the settings replaced them with the defaults. They are now used and kept.
  - Windows: `--verbose` output and the launcher's messages reach a redirected output (a file or a pipe)
    even when the program is not started from a console.
- **Tested** in Chrome and Firefox (web), on Linux, on Windows (under Wine, and the package on Windows
  10), and the Android app was built but not run.

## New in 1.4

- **In a web browser:** `speedy-blupi-sdl3-1.4.0-web.zip` holds all five programs compiled to
  WebAssembly, with a page to choose one. As on the desktop, you use your own copy of the game: on the
  first start the page asks for the game's folder and keeps its files in the browser's storage (nothing
  is uploaded); saves and settings stay there too. Unpack the zip, run `python3 serve.py` in it and open
  http://localhost:8080/ (or put the folder on any web server). Multiplayer came to the browser in 1.5.
  See `port/docs/WEB.md`.

## New in 1.3

- **Three more editions, each as a program of its own:** `eggbert-demo-sdl3` (Speedy Eggbert Demo,
  eGames 1999), `eggbert-value-sdl3` (Speedy Eggbert SE Valueware v1.0) and `blupi-demo-sdl3` (Speedy
  Blupi Demo v0.8, Epsitec, June 1998). Each is built from that edition's own reconstructed source and
  plays it with its own title screen and its own restrictions, exactly as the original did. Each keeps
  its own settings and saves. Point it at the edition's own folder (the demo's or the Valueware's).
  Windows and Linux; not in the Android app.
- **Internet games from the game itself:** the multiplayer provider list has a second entry, "Internet
  (enter an address)". Choosing it asks for the address of the computer that created the game, then
  lists and joins that game, as the original's Internet provider did. The address is remembered. The
  host's game port (28640 by default) must be reachable from the joining computer. `--net-join` still
  works.
- **The gamepad as the keyboard:** while the game is set to the keyboard (its default), a gamepad also
  works as the keyboard. The d-pad and the left stick are the arrow keys, A jumps, B and X fire, Start
  pauses and Back leaves. Each button can be changed in the settings file (`gamepadA` …
  `gamepadArrows`). Choosing the gamepad as the game's input device still uses it as the original's
  joystick.
- **Speedy Blupi 2's CD music:** with `CDAudio=1` in the game's `data/config.def`, Speedy Blupi 2 and
  Speedy Eggbert 2 play the CD's music tracks, which you supply as WAV files ripped from your disc:
  `music/track02.wav` and so on, in the game folder. The CD's audio tracks are not data files, so they
  are not in your installed game or in disc images.

## New in 1.2

- **Speedy Blupi 2 and Speedy Eggbert 2** on Windows and Linux (`blupi2-sdl3`) and in the Android
  app. The program recognises the edition from your data. It plays like the original, with the same optional
  widescreen, smooth motion and Display options as Speedy Blupi.
- **Multiplayer**, again, in all four games: the original's network game for two to four players,
  rebuilt on a native network layer in place of DirectPlay. The original game logic is unchanged.
  - **Choosing a session:** use the game's own multiplayer screens. Choose the provider "Native network
    (UDP)", then create a session, or join one found on your local network.
  - **Platforms:** Windows, Linux and Android players of the same edition can play together. Players
    need the same game and edition (Speedy Blupi, Speedy Eggbert, Speedy Blupi 2 or Speedy Eggbert 2)
    and the same multiplayer maps; any language works. Other sessions are not listed.
  - **Internet play:** start the joining player's game with `--net-join HOST[:PORT]` to add a host by
    address. Hosts use UDP ports 28640 (game) and 28641 (discovery); allow them in the firewall. Use
    `--net-port N` for other ports.
  - **If the host leaves,** the session ends for everyone, as the original effectively did (host
    migration is not supported).
- **Speedy Eggbert's title screen** says "Version 1.1" like the original, and the Spanish `¡` has the
  original width.

## Downloads

| File | For |
|---|---|
| `speedy-blupi-sdl3-1.5.2-windows-x64.zip` | 64-bit Windows 10 or 11: `blupi-sdl3`, `blupi2-sdl3`, `eggbert-demo-sdl3`, `eggbert-value-sdl3`, `blupi-demo-sdl3`, `SDL3.dll` |
| `speedy-blupi-sdl3-1.5.2-linux-x86_64.tar.gz` | x86-64 Linux with glibc 2.38 or newer (Debian 13, Ubuntu 24.04, Fedora 39 and later), X11 or Wayland: the same five programs |
| `speedy-blupi-sdl3-1.5.2-android.apk` | Android 7.0 or newer (arm64-v8a, x86_64): Speedy Blupi and Speedy Blupi 2 |
| `speedy-blupi-sdl3-1.5.2-web.zip` | a web browser: the five programs as WebAssembly, the page, `serve.py`; also at https://openeggbert.github.io/blupi_recon/ |
| `SHA256SUMS.txt` | checksums |

All packages are built on Debian 13 from the release tag: Linux natively (it needs glibc 2.38; 1.2.0's,
built on Ubuntu 22.04, needed 2.35), Windows with MinGW-w64, Android with Gradle and the NDK, the web
package with Emscripten.

- **Windows and Linux:** unpack anywhere and start `blupi-sdl3` (Speedy Blupi, Speedy Eggbert) or
  `blupi2-sdl3` (Speedy Blupi 2, Speedy Eggbert 2); for the demos and Valueware, `eggbert-demo-sdl3`,
  `eggbert-value-sdl3` or `blupi-demo-sdl3`. The Windows executables are not code-signed, so
  SmartScreen may ask for confirmation ("More info", "Run anyway").
- **Android:** install the APK. It is signed with a new key: uninstall 1.2.0 or older first (that also
  removes the games you imported; import them again). It updates 1.4.0 and 1.5.x in place.
  Choose a game folder to import; the app copies it into its own storage. It tells the two games apart,
  and after a second import it asks which game to start.

## What you get

- **The original games, exactly:** 640×480, the original 50 ms tick, 8- and 16-bit colour. Their
  frames equal the historical rebuilds' pixel for pixel (checked over the attract modes and menu tours),
  and the demos replay identically on Windows and Linux.
- **Music** from the games' MIDI files, played with the GeneralUser GS SoundFont (bundled).
- **Optional extras** under *Display options* on the settings screen; none of them changes how the game
  plays:
  - widescreen from 4:3 up to 21:9, following the window's shape, in play and in the editor;
  - smooth motion between ticks;
  - redraw at the monitor's rate or the original 20 per second;
  - window or full screen (Alt+Enter), and fit-the-window or whole-pixel scaling;
  - the language.
- **Languages:**
  - Speedy Blupi: English, German, French.
  - Speedy Eggbert: English, German, Spanish, French, Italian, Portuguese.
  - Speedy Blupi 2: English, German, French.
  - Your own language pack: `--export-language`.
- **Gamepads** through the game's own joystick option.
- **Android:** always full screen and landscape. Menus are tapped directly. In play there is a touch
  joystick with jump, fire and pause buttons, and the editor has a scroll pad. The Back button acts as
  Escape.

## Not in this release

- **The intro and ending movies.** The original skipped them when the system could not play AVI, and
  the installed games do not include them.
- **Speedy Blupi 2's CD audio tracks** themselves: supply them as WAV, Ogg Vorbis or FLAC files ripped from your disc (above).

## Known limitations

- **Multiplayer** is tested automatically (two and four players on one machine, Windows and Linux;
  through a Free Direct server on one machine, also between a browser and Linux) and was played between
  real devices for both games, but has had little real-world play beyond that. Also:
  - an Android player who switches away from the game is likely to drop out of a session;
  - fds.openeggbert.com is the one public Free Direct server so far; any group can run its own
    (port/docs/FREE_DIRECT_SERVER.md).
  - `wss://` from the desktop was tested on Linux and on Windows under Wine (against fds.openeggbert.com
    and certificate test sites), not yet on a real Windows PC.
- **Android:** the 1.5.x APKs were built but not run (no device or emulator was available); `wss://`
  there is untested. A player's own
  language file goes into a `languages` folder inside the game folder before importing it. Gamepads and
  running in the background have had little testing. The 1.3 and 1.5 additions (the address and server
  screens, the gamepad as the keyboard) are built for Android but untested there, and the edition programs
  are desktop only.
- **Linux:** the first-run folder dialog needs the desktop's file chooser portal (or `zenity`). Without
  it, start the game with `--data "<game folder>"`, or put the port's files into the game's folder.

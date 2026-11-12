# Speedy Blupi SDL3 port 1.3.0

The native port of **Speedy Blupi** (Epsitec, 1998) and **Speedy Blupi 2** (2001), and of their
eGames editions **Speedy Eggbert** and **Speedy Eggbert 2**, for Windows, Linux and Android. It is
built from this repository's reconstruction of the original programs' source, compiled unchanged with
a modern compiler, plus a small platform layer on SDL3.

Version 1.3 adds the Speedy Eggbert Demo, Speedy Eggbert SE Valueware and the June 1998 Speedy Blupi
Demo as programs of their own, internet games from the game's own multiplayer screens, the gamepad as
the keyboard, and Speedy Blupi 2's CD music from tracks you rip yourself. Version 1.2 brought Speedy
Blupi 2 and Speedy Eggbert 2, and the original multiplayer of all four games.

**The original games' files are not included.** You need your own copy of the game: the CD or the
installed game. On the first start the port asks for its folder and remembers it.

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
| `speedy-blupi-sdl3-1.3.0-linux-x86_64.tar.gz` | x86-64 Linux with glibc 2.41 or newer (Debian 13, Ubuntu 25.04 and later), X11 or Wayland: `blupi-sdl3`, `blupi2-sdl3`, `eggbert-demo-sdl3`, `eggbert-value-sdl3`, `blupi-demo-sdl3` |
| `SHA256SUMS.txt` | checksums |

For 1.3.0 only the Linux package was built, on Debian 13 (so it needs glibc 2.41; the 1.2.0 package,
built on Ubuntu 22.04, needed 2.35). Windows and Android packages of 1.3.0 have not been built yet;
the 1.2.0 ones remain available.

- **Windows and Linux:** unpack anywhere and start `blupi-sdl3` (Speedy Blupi, Speedy Eggbert) or
  `blupi2-sdl3` (Speedy Blupi 2, Speedy Eggbert 2); for the demos and Valueware, `eggbert-demo-sdl3`,
  `eggbert-value-sdl3` or `blupi-demo-sdl3`. The Windows executables are not code-signed, so
  SmartScreen may ask for confirmation ("More info", "Run anyway").
- **Android:** install the APK; it updates 1.1.0 and the 1.2 release candidates in place and keeps your
  imported games.
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
- **Speedy Blupi 2's CD audio tracks** themselves: supply them as WAV files ripped from your disc (above).

## Known limitations

- **Multiplayer** is tested automatically (two and four players on one machine, Windows and Linux)
  and was played between real devices for both games, but has had little real-world play beyond that.
  Also:
  - an Android player who switches away from the game is likely to drop out of a session.
- **Android:** a player's own language file goes into a `languages` folder inside the game folder before
  importing it. Gamepads and running in the background have had little testing. The 1.3 additions (the
  Internet address screen, the gamepad as the keyboard) are built for Android but untested there, and the
  edition programs are desktop only.
- **Linux:** the first-run folder dialog needs the desktop's file chooser portal (or `zenity`). Without
  it, start the game with `--data "<game folder>"`, or put the port's files into the game's folder.

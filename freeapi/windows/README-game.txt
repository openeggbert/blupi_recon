@TITLE@ for Windows (64-bit), Free API build
=============================================================

The reconstructed source of the original game, compiled unchanged against Free API
(the Windows API it uses) and Free Direct (DirectX), both over SDL3. The game's own
data is in this folder, next to the program.

Starting
--------
1. Unpack the whole folder to a place you can write to (not "Program Files"):
   the game saves its players and games in DATA.
2. Double-click @EXE@.

@HIRES@
The game starts in full screen. For a window, change FullScreen=1 to FullScreen=0
in DATA\config.def; the window is then as large as fits the screen.

Optional settings, as environment variables (for example in a .bat file):
  FREE_API_WINDOW_SCALE=1..4   window size, a multiple of 640x480
  FREE_API_SOUNDFONT=file.sf2  another SoundFont for the music
To see the original images, rename the folder hires.

Multiplayer
-----------
In the game's multiplayer screen, choose a provider:

* FreeDirect - the local network. One player creates a game, the others choose it
  from the list. Windows Firewall asks on the first start: allow the program on
  private networks. The computers must reach each other on UDP ports 51321 and
  51323. If the list stays empty, set FREE_DIRECT_ENET_HOST_ADDRESS to the host
  computer's IP address before starting the game.

* FreeDirect server - over the Internet, through a Free Direct server (the
  FreeDirectServer package). Enter the server's address, port 51325 and the
  password if the server has one, then create a room. Give the room ID that
  appears to the other players; they choose "Join a room" and enter it. Inside
  the room, create or join the game as on a local network. No player needs an
  open port; only the server does. This build connects with ws:// only (no TLS).

Only the same game, edition and language play together.

Not available: the AVI movies and CD audio (the music plays from the MIDI files).

Licences: COPYING.txt (GPL version 3) for the game's reconstructed source, and the
folder licenses for SDL3, SDL3_image, Free API, Free Direct, ENet, IXWebSocket,
TinySoundFont, the MinGW-w64 runtime and the GeneralUser GS SoundFont.

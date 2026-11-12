Speedy Blupi and Speedy Eggbert - SDL3 port for the web, version @VERSION@
=========================================================================

The SDL3 port of the reconstructed games, compiled to WebAssembly: Speedy
Blupi 1 / Speedy Eggbert 1, Speedy Blupi 2 / Speedy Eggbert 2, the Speedy
Eggbert Demo, Speedy Eggbert SE Valueware and the June 1998 Speedy Blupi Demo.

The original games' files are NOT included: you need your own copy of the
game. On the first start the page asks for its folder (the one that contains
DATA, the image folders and SOUND). The game's files are copied into this
browser's storage (IndexedDB) and stay there; nothing is uploaded. Saved
games and settings are kept there too.


Starting
--------

Browsers do not run this from a file:// page. Serve the folder:

  python3 serve.py

and open http://localhost:8080/ in Chrome, Firefox, Edge or Safari. Any
other static web server works too (it must send .wasm as application/wasm),
also on a web host.

Choose a game, then "Choose the game folder...", then "Play". The next time
"Play" starts at once. "Remove the stored game files" frees the space again.

Keys, mouse and gamepads work as in the desktop version; Alt+Enter switches
to full screen. Multiplayer is not available in a browser (it would need UDP
or a relay server). The intro and ending movies are not played, as in the
desktop version.

Licences: COPYING.txt (the port, GPL v3), THIRD_PARTY.txt, LICENSE-SDL3.txt
and soundfont/LICENSE-GeneralUser-GS.txt.

Speedy Blupi / Speedy Eggbert 1 and 2 - web build

Start the included local server with Python 3:
    python3 serve_web.py
Open http://localhost:8080/ in a browser supporting WebAssembly threads and WebGL.
Ctrl-C stops the server. Use --port 9000 to select a different port.

Choose one of the four games and press Start game. Graphics, levels, recorded
demos, sound effects and MIDI music files are bundled in each game's game.data.
The shared soundfont/default.sf2 supplies MIDI instruments automatically.
No game-folder or SoundFont selection is required. No files are uploaded and
no original executable is run.

Saves are stored in the browser, separately for each game and site origin.
Use Export saves for a backup. Import saves is available before starting.

For other hosting, serve this directory over HTTPS with these response headers:
    Cross-Origin-Opener-Policy: same-origin
    Cross-Origin-Embedder-Policy: require-corp
The .wasm MIME type is application/wasm.

Source and build instructions: https://github.com/openeggbert/blupi_recon
See freeapi/README.md in the source tree. COPYING.txt contains GPLv3;
licenses/ contains third-party notices. Game data belongs to Epsitec SA.

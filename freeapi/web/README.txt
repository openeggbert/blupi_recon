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

For static hosting (including GitHub Pages), serve the entire directory over HTTPS.
bootstrap.js registers coi-serviceworker.js for this directory and reloads once
on first use. The game runtime loads only after browser isolation is enabled.
Allow service workers; use Retry loading if preparation fails. The worker does
not add an offline asset cache or modify browser saves.

Servers can alternatively supply these headers directly, as serve_web.py does:
    Cross-Origin-Opener-Policy: same-origin
    Cross-Origin-Embedder-Policy: require-corp
The .wasm MIME type is application/wasm. Keep bootstrap.js, coi-serviceworker.js
and licenses/coi-serviceworker.txt in the package root, above all four games.

Source and build instructions: https://github.com/openeggbert/blupi_recon
See freeapi/README.md in the source tree. COPYING.txt contains GPLv3;
licenses/ contains third-party notices. Game data belongs to Epsitec SA.

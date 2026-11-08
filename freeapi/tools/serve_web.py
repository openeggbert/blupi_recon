#!/usr/bin/env python3
"""Foreground local server for the threaded WebAssembly build (Ctrl-C stops it)."""
import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, HTTPServer
from pathlib import Path


class Handler(SimpleHTTPRequestHandler):
    extensions_map = {**SimpleHTTPRequestHandler.extensions_map, '.wasm': 'application/wasm'}

    def end_headers(self):
        self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
        self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
        self.send_header('Cache-Control', 'no-store')
        super().end_headers()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    beside_script = Path(__file__).resolve().parent
    default_directory = (beside_script if (beside_script / 'index.html').is_file()
                         else Path(__file__).resolve().parents[2] / 'build/freeapi-web/site')
    parser.add_argument('--directory', type=Path, default=default_directory)
    parser.add_argument('--port', type=int, default=8080)
    args = parser.parse_args()
    if not (args.directory / 'index.html').is_file():
        parser.error('Missing web build: run bash freeapi/tools/build_web.sh first')
    with HTTPServer(('127.0.0.1', args.port), partial(Handler, directory=str(args.directory))) as server:
        print(f'http://localhost:{args.port}/', flush=True)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass


if __name__ == '__main__':
    main()

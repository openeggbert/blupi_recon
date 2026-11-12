#!/usr/bin/env python3
"""Serve this folder for the browser: python3 serve.py [port], then open http://localhost:8080/.

Browsers do not run WebAssembly from file:// pages, so the web build needs a web server. This one
serves the folder on this computer only (127.0.0.1). Any static web server works as well; it must
send .wasm files as application/wasm.
"""
import http.server
import os
import sys


class Handler(http.server.SimpleHTTPRequestHandler):
    extensions_map = {**http.server.SimpleHTTPRequestHandler.extensions_map,
                      ".wasm": "application/wasm", ".js": "text/javascript"}


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    server = http.server.ThreadingHTTPServer(("127.0.0.1", port), Handler)
    print("Open http://localhost:%d/ (Ctrl+C stops the server)" % port)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()

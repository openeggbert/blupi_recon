# Running the Free Direct server for the port

How to run [free-direct-server](https://github.com/openeggbert/free-direct/blob/hires-images/docs/free-direct-server.md) so that every build of
the port can use it: the web page on GitHub Pages (`https://`, so only `wss://`), and the desktop and
Android programs (`ws://` only, port/docs/MULTIPLAYER.md). One server process serves both, so all of
them share its rooms. The web page offers `fds.openeggbert.com` as its default server (`port/web/loader.js`).

```text
browser (https page) ── wss://fds.openeggbert.com (443) ── nginx (TLS) ──┐
desktop, Android ────── ws://fds.openeggbert.com:51325 ─────────────────┴── free-direct-server :51325
```

Players type the plain name `fds.openeggbert.com` everywhere: on an `https://` page the game takes
`wss://` on port 443, on the desktop and Android `ws://` on port 51325 (since port 1.5.1).

## 1. Build the server

On the server (Debian or Ubuntu; it needs a C++20 compiler, CMake and git). TLS is nginx's job, so the
server is built without it. The server's code is on free-direct's `hires-images` branch.

```bash
sudo apt install build-essential cmake ninja-build git
git clone --branch v11.4.6 --depth 1 https://github.com/machinezone/IXWebSocket ~/deps/IXWebSocket-v11.4.6
git clone --branch hires-images https://github.com/openeggbert/free-direct ~/free-direct
cmake -S ~/free-direct/server -B ~/free-direct/build-server -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DFREE_DIRECT_IXWEBSOCKET_SOURCE_DIR=$HOME/deps/IXWebSocket-v11.4.6 -DFREE_DIRECT_RELAY_TLS=OFF
cmake --build ~/free-direct/build-server
sudo install -m 755 ~/free-direct/build-server/free-direct-server /usr/local/bin/
free-direct-server --help
```

## 2. Run it as a service

`/etc/systemd/system/free-direct-server.service`:

```ini
[Unit]
Description=Free Direct server (multiplayer relay for the Speedy Blupi port)
After=network-online.target
Wants=network-online.target

[Service]
# 0.0.0.0:51325 for the desktop and Android programs (ws://); nginx reaches it on 127.0.0.1.
# --allow-origin: web pages from GitHub Pages only (native programs send no Origin and are always
# accepted). Add another --allow-origin for each other site that hosts the web build.
ExecStart=/usr/local/bin/free-direct-server --host 0.0.0.0 --port 51325 --allow-origin https://openeggbert.github.io
DynamicUser=yes
Restart=on-failure

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now free-direct-server
journalctl -u free-direct-server -f        # its log: connections, rooms (IDs shortened), refusals
```

A password (`--password-file`) keeps strangers off the server; the port's programs take it from the
environment variable `BLUPI_NET_PASSWORD`, which a browser cannot set, so a server for the web page runs
without one. The room IDs are the secret.

## 3. nginx: wss:// on 443

DNS: `fds.openeggbert.com` has A and AAAA records for the server. First the plain-HTTP site, so that
certbot can get the certificate. `/etc/nginx/sites-available/fds.openeggbert.com`:

```nginx
# WebSocket upgrade: Connection "upgrade" for an upgrade request, "close" otherwise.
map $http_upgrade $connection_upgrade {
    default upgrade;
    ''      close;
}

server {
    listen 80;
    listen [::]:80;
    server_name fds.openeggbert.com;

    location /.well-known/acme-challenge/ {
        root /var/www/html;
    }
    location / {
        return 301 https://$host$request_uri;
    }
}
```

If another site file already defines a `map $http_upgrade $connection_upgrade`, leave this one out
(nginx refuses a second definition).

```bash
sudo ln -s /etc/nginx/sites-available/fds.openeggbert.com /etc/nginx/sites-enabled/
sudo nginx -t && sudo systemctl reload nginx
sudo apt install certbot
sudo certbot certonly --webroot -w /var/www/html -d fds.openeggbert.com --deploy-hook "systemctl reload nginx"
```

Then add the TLS site to the same file:

```nginx
server {
    listen 443 ssl;
    listen [::]:443 ssl;
    server_name fds.openeggbert.com;

    ssl_certificate     /etc/letsencrypt/live/fds.openeggbert.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/fds.openeggbert.com/privkey.pem;

    location / {
        proxy_pass http://127.0.0.1:51325;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection $connection_upgrade;
        proxy_set_header Host $host;
        # Games stay connected for hours; the server pings every 20 s.
        proxy_read_timeout 3600s;
        proxy_send_timeout 3600s;
        proxy_buffering off;
    }
}
```

```bash
sudo nginx -t && sudo systemctl reload nginx
```

certbot renews the certificate itself (its systemd timer); the deploy hook reloads nginx after each
renewal.

## 4. Firewall

Open TCP 80 and 443 (nginx) and TCP 51325 (the desktop and Android programs), for example
`sudo ufw allow 80,443,51325/tcp`.

## 5. Check

```bash
# The TLS proxy answers a WebSocket upgrade with 101 (any 16-byte key, base64):
curl -si --http1.1 -H "Connection: Upgrade" -H "Upgrade: websocket" -H "Sec-WebSocket-Version: 13" \
  -H "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==" -H "Origin: https://openeggbert.github.io" \
  https://fds.openeggbert.com/ | head -1
# The desktop program: choose "Internet through a Free Direct server", leave the room empty, OK.
blupi-sdl3 --net-server fds.openeggbert.com
```

In the browser: https://openeggbert.github.io/blupi_recon/, a program, the multiplayer screen, the provider
"Internet through a Free Direct server": the server is filled in; OK with an empty room creates one and
the page shows a link for the other players.

## Notes

- Behind nginx the server sees every browser player as 127.0.0.1, so its per-address limits (wrong room
  IDs, connections) count them together.
- The server keeps no game data and logs no passwords or messages; rooms disappear when it restarts.
- Updating: `git -C ~/free-direct pull`, build again, `sudo install ...`, `sudo systemctl restart
  free-direct-server` (running games end).

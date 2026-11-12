# Native multiplayer (roadmap stage 10)

## Status (2026-10-25, branch `multiplayer-pilot`)

Scope: Speedy Blupi and Speedy Eggbert (SB1), Speedy Blupi 2 and Speedy Eggbert 2 (SB2), one game and
edition at a time, on any platform. Windows, Linux and Android builds of the same edition play together;
different games or editions never do (the protocol id and the content check below refuse them).

[EVIDENCE] Tests (Windows MinGW build; the two-instance test also on Linux):

- `net_sb1_two_instances` and `net_sb1_four_instances` (`tests/net_session.py --players 2|4`, scripts
  `tests/net/sb1_*.input`): a host and one or three clients, headless and lockstepped by the supervisor
  (`--net-test-sync`), go through the original screens: choose player, the provider list ("Native network
  (UDP)"), sessions, create / join, the lobby with all members, ready, start, then play. Every peer receives
  every other member's MOVE bundles and their embedded events: the host's lift start/end synchro (types
  50/51, the original host-only lift coordination), object starts/deletes and sounds; lobby JOIN, READY,
  LOBBY and START. No packet is rejected. Run twice, each peer's frame trace and network audit repeat byte
  for byte (about 22 s per run on Windows, 2 s on Linux).
- `net_sb2_two_instances` (Speedy Eggbert 2 data) and `net_sb2_four_instances` (Speedy Blupi 2 data):
  the same scenario with `--game sb2` (scripts `tests/net/sb2_*.input`, from SB2's own phase tables),
  with the same checks and repeatability (about 23 s per run on Windows).
- `net_transport`: handshake, member ids, reliable order, relay without echo, unsequenced delivery,
  protocol/version/content/full/after-start refusals, LAN discovery (and reporting of an incompatible
  session), graceful disconnect events, bounded close, host loss.
- `net_protocol`: the validator's corpus of valid and malformed packets for every message kind, and the
  places where the SB1 and SB2 rules differ.

[DESIGN] Cross-platform rules:

- **Wire format:** the historical byte layout, little-endian, signed char; `cnetwork.cpp` asserts the
  sizes and offsets of `NetMessage`, `NetPacket` and `NetPlayer`, so a platform that differed would not
  build.
- **Compatibility:** every session envelope carries a 64-bit content fingerprint: the game, the edition
  and the multiplayer maps (SB1 `data/world101..110`, SB2 `data/world201..212`) as the game loads them. A join with another edition
  or other maps is refused; such sessions are logged and not listed.
- **Finding sessions:** LAN discovery (a broadcast query, answered on the game port + 1) fills the
  original session list. Addresses can be added with `--net-join HOST[:PORT]`; `--net-port N` changes
  the port (default 28640, discovery 28641). Android holds a Wi-Fi multicast lock while in the foreground,
  so a phone that hosts is discoverable.
- **Internet (since 1.3.0):** the provider list has a second entry, "Internet (enter an address)". As
  DirectPlay's TCP/IP provider asked for an address in a dialog of its own while the game enumerated
  sessions, choosing it opens the port's address screen over the session screen
  (`common/ui/address_screen.cpp`). The player types a host name or IPv4 address, with `:port` if it is not
  the game port; OK hands it to the adapter and posts the session screen's own refresh, which then also
  probes that address. An empty field searches the local network only; Escape or Cancel closes the screen.
  The last address is remembered (`netAddress` in the settings file). The host side is unchanged: it creates
  its session as before, and its game port (28640 by default) must be reachable from the joining computer
  (forwarded through a router where needed; no NAT traversal or relay). [EVIDENCE] `net_sb1_internet_address`
  (`tests/net_session.py --internet`): the client uses another game port, finds nothing on the LAN or at its
  own address, types the host's address, then finds, joins and plays the session; both runs repeat exactly.
  Android: built, untested (the on-screen keyboard opens while the screen is up).

- **Through a Free Direct server (since 1.5.0):** the provider list has a third entry, "Internet through
  a Free Direct server"; in a browser it is the only one (there is no UDP there). Every player, the host
  too, connects out to a [free-direct-server](https://github.com/openeggbert/free-direct/blob/hires-images/docs/free-direct-server.md), so no
  player needs an open port. Its rooms, with secret IDs, are private LANs: the session list shows the
  games in the player's room. Choosing the entry opens the address screen in a second form with two
  fields: the server (a name or IP with `:port` if it is not 51325, on an `https://` page 443, or a
  `ws://`/`wss://` URL) and a room. On the desktop and Android, `wss://name` reaches a server through
  its TLS front on port 443, as the web page does (since 1.5.2): for networks that let only 443 out, or
  a server password that should not cross the network in clear.
  An empty room creates one; the screen then shows its ID (also on the clipboard, and in a browser as a
  link with `?server=` and `?room=`) to give to the others, who type it. The last server and room are
  remembered (`netServer`, `netRoom`); `--net-server` and `--net-room` fill the screen, and the password
  of a server that has one comes from `BLUPI_NET_PASSWORD`.
  [DESIGN] The server's protocol ([directplay-relay-protocol.md](https://github.com/openeggbert/free-direct/blob/hires-images/docs/directplay-relay-protocol.md)) is a second
  transport of the same session layer (`common/net/relay.*`): the host hosts a session in the room,
  members join it, and the session envelope (Hello, Welcome, Members, Data, Gone) travels unchanged as the
  payload of the relay's Data messages, routed host to member and member to host, with the sender's
  reliability flag in front. The server delivers everything reliably and in order. The room admits only
  the kind of build that created it: the relay's compatibility identity names the program, game,
  edition, protocol and the content fingerprint. The WebSocket client (`common/net/websocket.*`) is the
  port's own: on the desktop and Android `ws://` over ENet's TCP sockets, polled in the game loop without
  threads, and `wss://` with TLS on the same sockets (`common/net/tls.*`, since 1.5.2): Mbed TLS 3.6
  ([third_party/README.md](../third_party/README.md)), built in unless `BLUPI_TLS=OFF`, which then refuses
  `wss://`. The certificate must be valid for the typed name and trusted: by `SSL_CERT_FILE` or
  `SSL_CERT_DIR` if set, else on Linux by the distribution's CA bundle, on Android by the system's CA
  directory, and on Windows by the system's own chain and name check (CertGetCertificateChain), made
  after the handshake and before anything is sent. A plain address on the desktop still means `ws://` on
  51325. In a browser the
  browser's WebSocket. A page served over `https://` may only use `wss://`, so a plain address there means
  `wss://` on port 443 (since 1.5.1; 1.5.0 kept 51325). The web page offers `fds.openeggbert.com`
  until the player has used another server (`--net-server-default`). Running a server for all of
  them, with nginx in front for `wss://`: [FREE_DIRECT_SERVER.md](FREE_DIRECT_SERVER.md).
  [EVIDENCE] `net_relay_addresses` (`tests/test_net_relay.cpp`: addresses and room IDs as typed);
  `net_sb1_relay` and `net_sb2_relay` (`tests/net_session.py --relay <free-direct-server>`,
  scripts `tests/net/*_relay.input`): the host's server screen creates a room (its ID goes to the room file
  of `--net-room-file`), the client's screen joins it, then both play as in the LAN test, lockstepped; both
  runs repeat exactly. `port/tools/web_check.mjs --relay`: a browser (the web build in headless Chrome)
  creates the room and the session and a native Linux program joins it; both reach PLAY.
  `net_tls` (`tests/net_tls.py`, probe `tests/test_net_tls.cpp`): free-direct-server behind a TLS front
  (`tests/tls_front.py`, test certificates made by openssl): welcome with the test CA as a file and as
  a directory; refused for another CA, another name, the system's trust (the test CA is no system's; on
  Windows the system's chain check, which under Wine first passed it until the chain's own trust status
  was checked as well), a certificate file without certificates, `ws://` to the TLS port and `wss://` to
  the plain port. `net_sb1_relay_wss`: the `net_sb1_relay` game through
  `wss://` and the front; both runs repeat exactly. Both also pass in the Windows build under Wine.
  `net_tls_online` (`-DBLUPI_TEST_ONLINE_SERVER=wss://...`): a real server with the system's trust.
  [EVIDENCE, 2026-11-12] `wss://fds.openeggbert.com` welcomed the probe on Linux (CA bundle) and on
  Windows under Wine (the system check); badssl.com's expired, wrong-name, self-signed and
  untrusted-root certificates were refused on both. Not tested: Android, and real Windows.

[OPEN] Not yet:

- real-network behaviour tests (latency, loss, reordering) and the host-loss and client-drop paths as
  game scenarios (transport level only so far);
- [EVIDENCE, 2026-10-25, user test] An SB1 session between an Android phone (1.2.0-rc1) and a Windows PC
  worked without issues. Not yet: more real devices and networks. [HYPOTHESIS] A backgrounded app stops
  servicing its session (SDL pauses the game loop), so after ENet's peer timeout (1.5 to 3 s) the others see
  it leave; untested;
- [EVIDENCE, 2026-10-26, user test] SB2 multiplayer with the local test builds (Windows package and
  Android APK of commit e6a305f) worked;
- the validator's remaining whitelists and margins before internet exposure;

Host migration is unsupported by design (below).

Goal: restore the original multiplayer of Speedy Blupi 1 and Speedy Blupi 2 by replacing DirectPlay
underneath the historical `CNetwork` interface. The reconstructed game logic (CEvent lobby and phases,
CDecor's `TreatNetData`, `NetMessage` producers in decnet/decmove/decblupi) stays as it is: the historical
TUs in `src/` and `games/sb2/src/` are never edited. Evidence for every protocol fact below is in the
assessment [MULTIPLAYER_ASSESSMENT.md](MULTIPLAYER_ASSESSMENT.md) (static, file:line); facts are [EVIDENCE], design choices
[DESIGN], unknowns [OPEN].

## What the original does

- [EVIDENCE] **Topology and authority.** DirectPlay 5 peer session (KEEPALIVE | MIGRATEHOST, at most four
  players). Every peer broadcasts its own Blupi's pose and the events it produced; the host runs the lobby
  table, the start, and the moving-object (lift) dwell boundaries. Neither input lockstep nor a
  host-authoritative world: each peer simulates locally and applies the others' poses and events.
- [EVIDENCE] **Reliability.** `CNetwork::Send` maps any nonzero flag to `DPSEND_GUARANTEED`. Lobby traffic
  (JOIN, LEAVE, LOBBY, TEAM, READY, START, CHAT), ABANDON/LOST/DIE/PAUSE and any MOVE bundle carrying a
  decor change, cloud stop or a non-transient object start/delete are guaranteed; ordinary MOVE bundles
  (pose, sounds, lift synchro, transient effects) are unreliable. Old MOVE poses are ignored by timestamp,
  but the events embedded in an old packet are still applied.
- [EVIDENCE] **Messages.** Lobby: JOIN 28 B, LEAVE 12, LOBBY 132, TEAM 12, READY 12, START 2, CHAT 108,
  CLOSE (receive-only). Gameplay: MOVE 20+8n (n <= 5 embedded 8-byte events), ABANDON 4, LOST 4, DIE 4,
  PAUSE 3. The two receive sites ask for 200 bytes (lobby, `CEvent::TreatEventBase`) and 60 bytes
  (gameplay, `CDecor::TreatNetData`).
- [EVIDENCE] **Host migration: requested from DirectPlay, never handled by the game.** No DirectPlay
  system message is dispatched; `IsHost` is cached at create/join and never changes. A host leaving the
  lobby gracefully closes the others' sessions (LEAVE with host = TRUE). Sudden losses are not detected by
  game code beyond pose prediction stopping after five calls.

## Port design

- [DESIGN] **Transport:** ENet 1.3.18 (MIT, `third_party/enet/`, unmodified), wrapped by
  `common/net/`: a small host/client session in a star (clients connect to the host; the host relays each
  client's payload to the other clients with its original origin id and never echoes it back). Channel 0
  is reliable and ordered per origin; channel 1 carries unreliable bundles unsequenced, so that late
  bundles still deliver their events as in the original. This topology changes routing, not authority.
- [DESIGN] **Envelope:** game protocol id (SB1 and SB2 differ and never interoperate), protocol version,
  session epoch, origin member id (the DPID the game sees; assigned by the host, nonzero), payload length.
  The member id comes from the connection, never from a client-written field.
- [DESIGN] **One validator, one rule table per game** (`common/net/game_protocol.*`). [EVIDENCE] SB2
  inherited SB1's messages and layouts unchanged (`games/sb2/src` decor.h, network.h, the PK_*/MESS_*
  values, the same Send/Receive sites); only what each game's code can emit differs: object types and
  speeds, the unreliable effect types, sounds, icon counts and the lobby's maps (SB1 1..10, SB2 1..12).
  Each table cites its sources; SB1 and SB2 also have their own protocol ids. One `CNetwork` adapter
  (`common/net/cnetwork.cpp`) is compiled in each game's historical context; `game/network_port.h`
  names the game's rules. The validator classifies a payload by (length, type) into the lobby or the
  gameplay namespace (unambiguous: lobby lengths 2/12/28/108/132, gameplay 3/4/20..60), and validates it atomically before `Receive`
  returns it: exact lengths, sender owns the DPIDs it names, slot/team/ready/map ranges, NUL-terminated
  names and chat, embedded event count and per-event ranges. Rejected packets are counted and logged,
  never passed on. `Receive(…, 200, …)` returns only lobby messages; `Receive(…, 60, …)` only gameplay
  messages; a gameplay message that arrives before the receiver has entered PLAY waits in its queue.
- [DESIGN] **Host migration:** unsupported, and reported ("host disconnected; host migration
  unsupported" in the log). The star topology ends with its host, so the adapter hands the game the
  original departure messages, generated by the trusted session layer (never accepted from the network):
  in the lobby the host's LEAVE with host = TRUE, which closes the session as a graceful host leave did
  (event.cpp); in play an ABANDON for the host and every other remote member, which removes them
  (decmove.cpp), so a remaining single player wins as in the original. A client that disconnects without
  ABANDON is announced by the host as that member's ABANDON (play) or LEAVE (lobby). Unlike DirectPlay's
  MIGRATEHOST, no new host is elected; the original game never acted on migration either (no system
  message is dispatched, `IsHost` never changes).
- [DESIGN] **Discovery:** the pilot lists the sessions it finds at configured endpoints (default
  `127.0.0.1` on the default port; `--net-join HOST[:PORT]` adds more); LAN broadcast discovery follows.
- [DESIGN] **Determinism for tests:** a test-only ingress mode releases received messages at tick
  boundaries in a fixed order, so two headless instances driven by scripts produce repeatable per-peer
  traces. Normal play never waits.

## Milestones

1. **Pilot (SB1 US, two local instances):** provider, create, enumerate, join, lobby names/colours/ready,
   start, play with remote poses and replicated events, pause, leave; malformed-packet rejection tests;
   two-instance regression test.
2. LAN, up to four players, with discovery.
3. Internet: direct IP and port (1.3.0), then through a Free Direct server, which needs no open port
   and works in a browser (1.5.0).
4. SB2 on the same transport, with its own protocol id and rules (done 2026-10-25).
5. Android: INTERNET permission, lifecycle handling, interoperability with the desktop for the editions
   the app supports.

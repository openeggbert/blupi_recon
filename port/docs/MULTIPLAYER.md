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
  too, connects out to a [free-direct-server](https://github.com/openeggbert/free-direct/blob/develop/docs/free-direct-server.md), so no
  player needs an open port. Its rooms, with secret IDs, are private LANs: the session list shows the
  games in the player's room. Choosing the entry opens the address screen in a second form with two
  fields: the server (a name or IP with `:port` if it is not 51325, on an `https://` page 443, or a
  `ws://`/`wss://` URL) and a room. On the desktop and Android, `wss://name` reaches a server through
  its TLS front on port 443, as the web page does (since 1.5.2): for networks that let only 443 out, or
  a server password that should not cross the network in clear.
  An empty room creates one; the screen then shows its ID (in a browser also the page, as a link with
  `?server=` and `?room=`) to give to the others. Nothing goes to the clipboard by itself: a click on
  the ID (or Ctrl+C) copies it, and on Android and in a touch screen's browser also opens the system's
  share sheet (`BlupiActivity.shareText`, `navigator.share` with the link). The others paste it into
  the room field: Ctrl+V, Shift+Insert, a right click on a field, or the Paste text above the room field.
  A paste takes the room ID alone, a sentence with an ID in the server's form, or the web page's link,
  which fills the server as well (`Invite` in `common/net/relay.cpp`); a browser asks for permission to
  read the clipboard on the first right click or Paste, while its own Ctrl+V needs none. When the
  browser refuses, or the clipboard holds no text, the screen says so. [EVIDENCE, after 1.7.0] In 1.7.0
  Ctrl+V did nothing in a browser: SDL 3.4 prevents the default action of every key pressed with Ctrl,
  so the browser's paste never happened; the page now keeps Ctrl+V from SDL (`host.cpp`). And a link
  pasted while the room field had the focus lost the server's dots (`example.org` became `exampleorg`):
  the server is now filtered by its own field's rule. `port/tools/web_check.mjs --paste` pastes with a
  real Ctrl+V (a key event whose editing command runs only if the page lets it) and with a click on
  Paste, and reads the game's log line `server screen: paste of N characters`. Typing the ID
  still works. (1.5.0 to 1.6.0 copied the ID as soon as the room existed, and in a browser not at all:
  SDL 3.4 has no browser clipboard.) The last server and room are
  remembered (`netServer`, `netRoom`); `--net-server` and `--net-room` fill the screen, and the password
  of a server that has one comes from `BLUPI_NET_PASSWORD`.
  [DESIGN] The server's protocol ([directplay-relay-protocol.md](https://github.com/openeggbert/free-direct/blob/develop/docs/directplay-relay-protocol.md)) is a second
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
  [EVIDENCE] `net_relay_addresses` (`tests/test_net_relay.cpp`: addresses and room IDs as typed, and
  pastes); `port/tools/web_check.mjs --relay` also reads the browser's clipboard after the host's click on
  the ID, and feeds the page's paste to the game; [EVIDENCE, 2026-11-14] by hand under Xvfb: a host's
  click on the ID put it on the X clipboard, and a second program's Paste filled the room field and
  joined the room. Not run: Android's share sheet (built only);
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
  untrusted-root certificates were refused on both. Not tested: Android.
  [EVIDENCE, 2026-11-13] On Windows 10 (a VirtualBox VM, the 1.6.0 package): `test_net_tls` was welcomed
  by `wss://fds.openeggbert.com` 6 times out of 6 (one earlier attempt, made just after the VM's network
  came up, ended with "the server does not answer"), and refused badssl.com's four certificates with the
  right reasons (expired, another name, not trusted twice). A Speedy Blupi game through
  `wss://fds.openeggbert.com`: the Linux 1.6.0 program hosted (room and session), the Windows 10 program
  joined with the room ID, and both reached play.

## Robustness (1.6.0, 2026-11-13)

[EVIDENCE] Tests on Linux (`tests/net_session.py`; SB1 on Speedy Blupi data, SB2 on Speedy Eggbert 2
data; every test runs twice and both runs must repeat exactly, except where noted):

- **A poor link:** `net_sb1_impaired`, `net_sb2_impaired`. The client joins through the Internet entry and a
  lossy UDP proxy (`tests/net_impair.py`): each datagram, in both directions, is dropped with probability
  0.1, delayed by 40 +/- 30 ms (so about a fifth arrive before an earlier one) and duplicated with
  probability 0.02. The frame traces and audits equal those of a run without the proxy (`--reference`):
  under the test lockstep every message still arrives at its tick. Ten seeds passed.
- **A member killed in play** (a crash, a cable pulled): `net_sb1_client_killed_in_play`. The host gets
  the member's ABANDON after ENet's peer timeout and plays on.
- **The host killed in play:** `net_sb1_host_killed_in_play`, `net_sb2_host_killed_in_play`. The client
  notes "host disconnected; host migration unsupported" and gets the host's ABANDON.
- **A member quits its program in play:** `net_sb1_client_quits_in_play`, `net_sb2_client_quits_in_play`.
  Its session closes at once; the host gets its last moves, then its ABANDON.
- **The host quits in the lobby:** `net_sb1_host_quits_in_lobby`. The client gets the host's LEAVE and the
  game leaves the lobby.
- **The app in the background, as on Android:** `net_sb1_client_suspended`, `net_sb2_client_suspended`:
  `--net-test-suspend` blocks the client's game thread for 12 s in play, as SDL blocks it inside its event
  pump while an Android app is in the background, and the session keep-alive below runs. Nobody notices:
  the traces equal those of a run without the suspension. `net_sb1_client_suspended_bare` and
  `net_sb2_client_suspended_bare` do the same without the keep-alive, which is what the port did before:
  the host drops the client after its peer timeout, and the client finds its host gone when it comes back.
  This confirms the earlier hypothesis on the desktop; whether a message of the departed host was already
  queued then varies, so these two runs are not compared with each other.

What the tests found, and the changes:

- **Finding a session at a typed address** waited 200 ms. One lost datagram (ENet resends after about
  500 ms) or a round trip of 80 ms or more and the session was not listed. A remote address (typed, or
  `--net-join`) now gets 5 s and a join 8 s; both end as soon as the host answers, so only an unreachable
  address waits that long. This machine's own address keeps 200 ms.
- **Peer timeout:** members were given up after 1.5 to 3 s without acknowledgements (ENet limit 8). On the
  lossy link, a few lost datagrams in a row ended games. It is now ENet's default limit of 32 round trips,
  5 to 10 s, so a member that is really gone is noticed within 10 s.
- **The order of a departure:** the adapter queued a member's departure before the messages the member had
  sent before it, and then the game rejected those as a departed member's ("rejected origin", "rejected
  gameplay-member"). Messages now come first, and a member stays known until the game has read its
  departure.
- **The session keep-alive (`common/net/keepalive.*`):** the game loop runs under a lock that the host
  releases while it waits for events. SDL's background event (`SDL_EVENT_WILL_ENTER_BACKGROUND`, delivered
  inside that wait) starts a thread that services every session every 10 ms until the app is back
  (`SDL_EVENT_DID_ENTER_FOREGROUND`). The game stays paused, as SDL keeps it on Android. A full receive
  queue now drops an unreliable message, a state the next one replaces, instead of ending the session.
  [OPEN] Tested on the desktop through `--net-test-suspend`, which calls the same handler; not run on
  an Android device. How long Android keeps a background app's network is up to the system.
- **The test harness:** a peer leaves the session only once every peer has completed the last frame (an
  exit grant), so no peer sees another's departure in its last frame. Before, `net_sb1_two_instances`
  could fail that way.

## Lag through a Free Direct server (after 1.6.0, 2026-11-14)

Players reported lag and other players' Blupis "teleporting" in games through fds.openeggbert.com.
The analysis, measurements and the proposed datagram transports are in Free Direct's
[relay-latency.md](https://github.com/openeggbert/free-direct/blob/develop/docs/relay-latency.md).
In short: [EVIDENCE] each player sends its own Blupi's position every tick, mostly unreliably; a
receiver predicts a missing one for at most 5 ticks (250 ms, `CDecor::DoNetSmooth`) and then jumps to
the next one that arrives. Through the server everything travels over one TCP connection (WebSocket),
where one lost packet holds back every message behind it until TCP resends it; they then arrive at once.

- **Route (session wire version 2):** [DESIGN] once the game has started, every member's game messages
  go to all others straight through the server (the relay's route 2), no longer to the host and back:
  two crossings of the Internet between members instead of four, and no wait for the host's loop. Before
  the start the host still relays, so that every member sees the lobby (joins, readiness, chat) in one
  order. The Members message names each member's connection on the server, and a member takes a
  message straight from the server only from the connection of the member it names as its origin.
  Version 2 sessions and rooms do not admit 1.6.0 and earlier programs (the compatibility identity
  names the version). [EVIDENCE] `net_sb1_relay_three` (`tests/net_session.py --relay --players 3`): a
  host and two members through a server, in play the members' messages straight to each other; both
  runs repeat exactly. `net_sb1_relay`, `net_sb1_relay_wss`, `net_sb2_relay` and the LAN tests pass.
- **Datagrams (UDP):** [DESIGN] when the server offers them (Welcome's flags; Free Direct's
  `docs/directplay-relay-protocol.md`, "Datagrams"), the relay client (`net/relay.cpp`) binds a UDP
  socket's address to its connection, and game messages that need not arrive (the session's
  unreliable flag: positions) go as datagrams; whatever must arrive, and everything when UDP gets no
  answer within 3 s, stays on the WebSocket. A lost datagram loses only itself, as over ENet on a LAN.
  ENet's sockets, so IPv4. **In a browser** (no UDP) the same datagrams go over a WebRTC data
  channel to the server, unordered and without retransmissions, when the server offers it: the page's
  offer goes as RtcOffer over the WebSocket, the answer comes back as RtcAnswer, and the server sends
  BindAck over the channel when it opens; no STUN or TURN server is needed. [EVIDENCE]
  `port/tools/web_check.mjs --relay --expect-webrtc`: with the browser hosting and a native program
  on UDP, the page sent 411 datagrams and received 401 through its channel in Chrome, 409 and 423 in
  Firefox. [EVIDENCE] The relay
  tests check that every program binds (`network: server datagrams on`). With the measurement below
  repeated with datagrams on, the client counted 1068 messages from the host, 2 over 150 ms late and
  1 over 250 ms (the latest 271 ms), against 47 and 16 over the WebSocket alone; the late ones are
  messages that must arrive (with game events), which still use the WebSocket.
- **Link diagnostics:** in play, `session.cpp` measures how late each sender's messages arrive against
  the tick the sender stamped on them (the envelope's tick, now also outside the test lockstep): later
  than the earliest arrival of the last second by over 150 ms, and by over 250 ms. A player who stands
  still sends nothing, so idleness is not counted. `network: link from player N ...` goes to stderr every
  30 s while new late messages occur, and when the session ends. Free Direct's DirectPlay counts the same
  stalls for the Free API builds.
- **`tests/tcp_stall.py`:** a TCP proxy that stalls one direction of a connection now and then and then
  lets everything through at once, as TCP does after a loss; for measurements on this machine.
  [EVIDENCE, 2026-11-14] Two SB1 programs in real time (offscreen video, both players walking for 78 s)
  through a local free-direct-server, the client's connection through the proxy. Without stalls the
  host counted 1031 messages from the client, none over 150 ms late (the latest 17 ms). With a 300 ms
  stall every 3 s on average (server to client), the client counted 1025 messages from the host: 47 over
  150 ms late and 16 over 250 ms, the latest 304 ms: sixteen stops and jumps of the host's Blupi.

[OPEN] Not yet:

- [EVIDENCE, 2026-10-25, user test] An SB1 session between an Android phone (1.2.0-rc1) and a Windows PC
  worked without issues. Not yet: more real devices and networks, and the keep-alive on a device;
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

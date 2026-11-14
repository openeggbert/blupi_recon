"""Two to four SB1 or SB2 instances in one native multiplayer session (port/docs/MULTIPLAYER.md).

A host and clients run headless with scripted input (tests/net/<game>_*.input) and --net-test-sync: this
supervisor grants each tick to both only after both finished the previous one, telling each how many
messages every member had sent by then, so each peer sees the same messages at the same tick in every run.
The test checks from each peer's network audit (user dir/network.trace) that they created and joined one
session, both reached PLAY with two members, each received the other's moves, and no packet was rejected.
It runs the scenario twice and requires each peer's frame trace and audit to repeat exactly. Every process
and wait is bounded; children are killed on failure. Never runs an original executable.

--internet: two peers; the client uses another game port and the provider list's Internet entry
(<game>_client_internet.input), so it finds the host's session only through the address it types on the
port's address screen ("N type TEXT" lines become one "char" per tick; {port} is the host's port).

--relay SERVER: two peers (or --players 3) through a Free Direct server (free-direct-server, started here
on a free port): all choose the provider list's server entry (<game>_<name>_relay.input). The host's server
screen creates a room and writes its ID to the room file (--net-room-file); the clients' screens read it
from there.

--tls (with --relay): the peers reach the server through wss://localhost and a TLS front for it, with test
certificates (tls_front.py; SSL_CERT_FILE names the test CA). Exit 77 (skipped) without openssl.

--impair SPEC (with --internet): the client reaches the host through a lossy UDP proxy (net_impair.py:
loss, delay, jitter, duplicate, seed), and types the proxy's port. Under the lockstep every message still
arrives at its tick, so --reference requires the same frame traces as without the proxy.

--scenario NAME: a member leaves or the app is suspended at a fixed tick (SCENARIOS below). A killed peer
is detected through ENet's peer timeout (at most 10 s, session.cpp): the supervisor waits 12 s before the
survivors' next tick, so the departure reaches them at that tick in every run. A peer that quits ends its
program at that tick; the survivors' next tick follows once it has gone. --net-test-suspend blocks a
peer's game thread for 12 s as Android blocks it in the background, with the session keep-alive
(net/keepalive.h) or, with the bare variant, without it.

--reference: also run the plain scenario once (no proxy, no scenario) and require the same frame traces,
and the same audits apart from the typed address.
"""
import argparse
import os
from pathlib import Path
import shutil
import socket
import subprocess
import sys
import time

import net_impair
import tls_front

PLAY = 1525  # WM_PHASE_PLAY, in both games
ABANDON = "04 08 00 00"   # the trusted departure message in play (cnetwork.cpp Pump)
SUSPEND_MS = 12000        # longer than ENet's peer timeout (session.cpp: 10 s at most)

# Scenario: departures {tick: (role, "kill" | "quit")}, a suspension (role, tick, bare), and the audit lines
# each survivor must show afterwards (substrings of the notes after the tick).
SCENARIOS = {
    # A member's program is killed in play (a crash, the network gone): the host removes it.
    "client-killed-in-play": {"depart": {420: (1, "kill")}, "expect": {"host": [f"receive 2 0 {ABANDON}"]}},
    # The host's program is killed in play: the client loses the session and the host's player.
    "host-killed-in-play": {"depart": {420: (0, "kill")},
                            "expect": {"client": ["host disconnected; host migration unsupported", f"receive 1 0 {ABANDON}"]}},
    # A member quits its program in play: its session closes at once.
    "client-quits-in-play": {"depart": {420: (1, "quit")}, "expect": {"host": [f"receive 2 0 {ABANDON}"]}},
    # The host quits in the lobby, before the start: the client receives the host's LEAVE.
    "host-quits-in-lobby": {"depart": {320: (0, "quit")}, "lobby": True,
                            "expect": {"client": ["host disconnected; host migration unsupported", "receive 1 0 0c 03"]}},
    # The client is in the background for 12 s in play, with the keep-alive: nobody notices.
    "client-suspended": {"suspend": (1, 420, False), "expect": {}},
    # The same without the keep-alive (what the port did before): the host drops the client after its
    # peer timeout, and the client finds its host gone when it comes back.
    # Not repeatable in one detail: whether the host's last messages reach the client before it finds the
    # host gone (they are then rejected as a departed member's), so only the departures are required.
    "client-suspended-bare": {"suspend": (1, 420, True), "loose": True,
                              "expect": {"host": [f"receive 2 0 {ABANDON}"],
                                         "client": ["host disconnected; host migration unsupported"]}},
}


def free_port():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def write_atomic(path, text):
    tmp = path.with_suffix(".tmp")
    tmp.write_text(text, encoding="ascii")
    os.replace(tmp, path)


def expand(source, target, port):
    """'N type TEXT' becomes one 'char' line per character from tick N; {port} is the host's port."""
    lines = []
    for line in source.read_text(encoding="ascii").splitlines():
        parts = line.split(None, 2)
        if len(parts) == 3 and parts[1] == "type":
            tick, text = int(parts[0]), parts[2].split("#")[0].strip().replace("{port}", str(port))
            lines += [f"{tick + i} char {ord(c)} 0" for i, c in enumerate(text)]
        else:
            lines.append(line)
    target.write_text("\n".join(lines) + "\n", encoding="ascii")
    return target


def start_server(args, work):
    """free-direct-server on a free TCP port of this machine, once it accepts connections. Both runs use
    the one server, so their audits (which name its address) can repeat exactly."""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(("127.0.0.1", 0))
        server_port = s.getsockname()[1]
    log = open(work / "server.log", "w", encoding="utf-8")
    server = subprocess.Popen([args.relay, "--host", "127.0.0.1", "--port", str(server_port)],
                              stdout=log, stderr=subprocess.STDOUT)
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        try:
            with socket.create_connection(("127.0.0.1", server_port), timeout=0.2):
                return server, log, server_port
        except OSError:
            if server.poll() is not None:
                break
            time.sleep(0.05)
    server.kill()
    log.close()
    raise RuntimeError("free-direct-server did not start (see server.log)")


def run(args, work, port, scenario=None, impaired=True):
    if work.exists():
        shutil.rmtree(work)
    control = work / "control"
    control.mkdir(parents=True)
    peers = []
    server = args.server
    scenario = scenario or {}
    departures = scenario.get("depart", {})
    target = args.proxy.port if (args.proxy and impaired) else port
    for role, name in enumerate(peer_names(args.players)):
        user = work / name
        user.mkdir()
        env = dict(os.environ, BLUPI_NET_TEST_DIR=str(control), BLUPI_NET_TEST_ROLE=str(role))
        script, own_port = Path(args.inputs) / f"{args.game}_{name}.input", port
        if args.internet and name != "host":
            script = expand(Path(args.inputs) / f"{args.game}_{name}_internet.input", work / f"{name}.input", target)
            own_port = args.client_port
        if args.relay:
            script = Path(args.inputs) / f"{args.game}_{name}_relay.input"
        frames = args.frames
        for tick, (who, how) in departures.items():
            if who == role and how == "quit":
                frames = tick + 1   # its program ends after that tick (and the supervisor's exit grant)
        command = [args.exe, "--headless", "--data", args.data, "--user", str(user), "--color", "16",
                   "--frames", str(frames), "--input", str(script),
                   "--trace", str(work / f"{name}.trace"), "--net-test-sync", "--net-port", str(own_port)]
        if scenario.get("suspend") and scenario["suspend"][0] == role:
            _, tick, bare = scenario["suspend"]
            command += ["--net-test-suspend", f"{tick}:{SUSPEND_MS}" + (":bare" if bare else "")]
        for frame in args.state_dump:
            command += ["--state-dump", f"{frame}:{work / f'{name}-state-{frame}.txt'}"]
        if server:
            address = f"wss://localhost:{args.front.port}" if args.front else f"ws://127.0.0.1:{server[2]}"
            command += ["--net-server", address, "--net-room-file", str(control / "room.txt")]
            if args.front:
                env["SSL_CERT_FILE"] = str(args.certs["ca"])
        log = open(work / f"{name}.log", "w", encoding="utf-8")
        peers.append((name, subprocess.Popen(command, env=env, stdout=log, stderr=subprocess.STDOUT), log))
    try:
        alive = list(range(args.players))
        sent = [0] * args.players
        for role in alive:
            write_atomic(control / f"{role}-grant-0", " ".join("0" * args.players) + "\n")
        deadline = time.monotonic() + args.timeout
        def grant(roles, tick):
            # Role r joins as member id r + 1 (the scripts join in role order); a member that left
            # keeps the count it had sent.
            for role in roles:
                write_atomic(control / f"{role}-grant-{tick}", " ".join(map(str, sent)) + "\n")
        for tick in range(args.frames):
            done = {role: control / f"{role}-done-{tick}" for role in alive}
            while not all(d.exists() for d in done.values()):
                if time.monotonic() > deadline:
                    raise RuntimeError(f"tick {tick}: timed out waiting for the peers")
                if any(peers[r][1].poll() is not None for r in alive) and not all(d.exists() for d in done.values()):
                    raise RuntimeError(f"tick {tick}: a peer exited early")
                time.sleep(0.001)
            for role, d in done.items():
                sent[role] = int(d.read_text(encoding="ascii").split()[0])
            if tick in departures:
                role, how = departures[tick]
                if how == "kill":
                    peers[role][1].kill()
                    peers[role][1].wait()
                    time.sleep(SUSPEND_MS / 1000)   # beyond ENet's peer timeout (session.cpp: 10 s at most)
                else:
                    grant([role], tick + 1)   # its exit grant: it leaves the session and ends
                    peers[role][1].wait(timeout=30)
                    time.sleep(1.0)
                alive.remove(role)
            grant(alive, tick + 1)
        for role in alive:
            peers[role][1].wait(timeout=max(1.0, deadline - time.monotonic()))
    finally:
        for name, process, log in peers:
            if process.poll() is None:
                process.kill()
                process.wait()
            log.close()

    return {name: process.returncode for name, process, _ in peers}


def audit(work, name):
    lines = (work / name / "network.trace").read_text(encoding="utf-8").splitlines()
    notes = [line.split(" ", 1)[1] for line in lines if " " in line]
    received = [line.split() for line in lines if line.split()[1:2] == ["receive"]]
    states = [line.split() for line in lines if line.split()[1:2] == ["state"]]
    return lines, notes, received, states


def check_scenario(work, players, scenario):
    """Each survivor shows the expected departure lines after the scenario's tick, and none before it."""
    failures = []
    names = peer_names(players)
    at = min(list(scenario.get("depart", {})) + ([scenario["suspend"][1]] if scenario.get("suspend") else []))
    for name in names:
        try:
            lines = (work / name / "network.trace").read_text(encoding="utf-8").splitlines()
        except OSError:
            continue
        def tick(line):
            return int(line.split(" ", 1)[0])
        later = [line.split(" ", 1)[1] for line in lines if tick(line) > at]
        for text in scenario["expect"].get(name, []):
            if not any(text in note for note in later):
                failures.append(f"{name}: no '{text}' after tick {at}")
        departures = [line for line in lines if (f" 0 {ABANDON}" in line or "host disconnected" in line)]
        if departures and not scenario["expect"].get(name):
            failures.append(f"{name}: unexpected departure: {departures[0]}")
        if any(tick(line) <= at for line in departures):
            failures.append(f"{name}: a departure before tick {at}")
        if departures:
            print(f"{name}: {departures[0]}")
    return failures


def peer_names(players):
    return ["host", "client", "client2", "client3"][:players]


def check(work, codes, players, internet_port=None, relay=False, scenario=None):
    failures = []
    scenario = scenario or {}
    killed = {peer_names(players)[r] for r, how in scenario.get("depart", {}).values() if how == "kill"}
    for name, code in codes.items():
        if code != 0 and name not in killed:
            failures.append(f"{name} exited {code} (see {work / (name + '.log')})")
    if failures:
        return failures
    if scenario.get("depart") or scenario.get("suspend"):
        failures += check_scenario(work, players, scenario)
    names = peer_names(players)
    peers = {name: audit(work, name) for name in names}
    expect = {name: ("joined session",) for name in names}
    expect["host"] = ("created session", "START")
    if internet_port is not None:
        expect["client"] = (f"Internet address 127.0.0.1:{internet_port}", "joined session")
    if relay:
        # Every program binds its UDP address on the server (datagrams for what need not arrive);
        # under the test lockstep all messages are guaranteed, so they still use the WebSocket.
        for name in names:
            log = (work / f"{name}.log").read_text(encoding="utf-8", errors="replace")
            if "network: server datagrams on" not in log:
                failures.append(f"{name}: no datagrams with the server (see {work / (name + '.log')})")
        expect["host"] = ("provider Free Direct server", "server room created", "created session", "START")
        for name in names[1:]:
            expect[name] = ("provider Free Direct server", "server room joined", "joined session")
    if scenario.get("lobby"):
        # The session ends before its start: no START, no play, no moves.
        expect = {name: tuple(n for n in notes if n != "START") for name, notes in expect.items()}
    for name, (lines, notes, received, states) in peers.items():
        for note in expect[name]:
            if not any(n.startswith(note) for n in notes):
                failures.append(f"{name}: no '{note}' in its audit")
        if not scenario and any(n.startswith("host disconnected") for n in notes):
            failures.append(f"{name}: lost its host: {[n for n in notes if n.startswith('host disconnected')][0]}")
        if not scenario and any(f" 0 {ABANDON}" in line for line in lines):
            failures.append(f"{name}: a member left the game")
        if any(n.startswith("rejected") and not (scenario.get("loose") and n == "rejected gameplay-member") for n in notes):
            failures.append(f"{name}: rejected packets: {[n for n in notes if n.startswith('rejected')]}")
        if scenario.get("lobby") or name in killed:
            continue
        if not any(int(s[2]) == PLAY and len([x for x in s if x.startswith("member=")]) == players for s in states):
            failures.append(f"{name}: never in PLAY with {players} members")
        own = str(names.index(name) + 1)
        counts = []
        for other in (str(i + 1) for i in range(players) if str(i + 1) != own):
            moves = [r for r in received if r[2] == other and len(r) > 5 and r[5] == "01"]
            if not moves:
                failures.append(f"{name}: received no MOVE from member {other}")
            events = sum(int(r[23], 16) for r in moves if len(r) > 23)
            counts.append(f"{len(moves)} moves/{events} events from {other}")
        print(f"{name}: {', '.join(counts)}; {len(lines)} audit lines")
    return failures


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", required=True)
    parser.add_argument("--data", required=True)
    parser.add_argument("--game", choices=("sb1", "sb2"), default="sb1", help="which game's input scripts")
    parser.add_argument("--inputs", required=True, help="directory with <game>_host.input, <game>_client.input, ...")
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument("--frames", type=int, default=700)
    parser.add_argument("--players", type=int, choices=(2, 3, 4), default=2)
    parser.add_argument("--timeout", type=float, default=240.0, help="seconds per run")
    parser.add_argument("--internet", action="store_true",
                        help="the client joins through the Internet provider and a typed address (two peers)")
    parser.add_argument("--relay", metavar="SERVER",
                        help="free-direct-server executable: two peers through it (the server provider)")
    parser.add_argument("--tls", action="store_true",
                        help="with --relay: through wss:// and a TLS front with test certificates")
    parser.add_argument("--impair", metavar="SPEC",
                        help="with --internet: through a lossy UDP proxy, e.g. loss=0.1,delay=40,jitter=30,duplicate=0.02")
    parser.add_argument("--scenario", choices=sorted(SCENARIOS), help="a member leaves or is suspended (two peers)")
    parser.add_argument("--reference", action="store_true",
                        help="also run without --impair and --scenario: the frame traces must be the same")
    parser.add_argument("--state-dump", type=int, action="append", default=[], metavar="FRAME",
                        help="diagnostic: each peer dumps its hashed state fields after FRAME")
    args = parser.parse_args()
    args.exe = str(Path(args.exe).resolve())
    args.work = args.work.resolve()
    port = free_port()
    if args.internet or args.relay:
        if args.internet:
            args.players = 2
        args.client_port = free_port()
        while args.client_port in (port, port + 1) or port in (args.client_port + 1,):
            args.client_port = free_port()
    args.server = args.front = args.proxy = None
    if args.impair and not args.internet:
        parser.error("--impair needs --internet")
    if args.scenario:
        args.players = 2
    if args.tls and not args.relay:
        parser.error("--tls needs --relay")
    if args.tls and not tls_front.openssl():
        print("no openssl: cannot make the test certificates")
        return tls_front.SKIP
    if args.relay:
        args.work.mkdir(parents=True, exist_ok=True)
        args.server = start_server(args, args.work)
    results = []
    try:
        if args.tls:
            args.certs = tls_front.make_certs(args.work / "certs")
            args.front = tls_front.TlsFront(args.certs["cert"], args.certs["key"], args.server[2])
        if args.impair:
            args.proxy = net_impair.UdpProxy(port, net_impair.parse(args.impair))
        return repeat(args, port, results)
    finally:
        if args.proxy:
            print(f"proxy: {args.proxy.summary()}")
            args.proxy.stop()
        if args.front:
            args.front.stop()
        if args.server:
            args.server[0].kill()
            args.server[0].wait()
            args.server[1].close()


def repeat(args, port, results):
    scenario = SCENARIOS.get(args.scenario)
    typed = (args.proxy.port if args.proxy else port) if args.internet else None
    killed = {peer_names(args.players)[r] for r, how in (scenario or {}).get("depart", {}).values() if how == "kill"}
    for run_index in (1, 2):
        work = args.work / f"run{run_index}"
        started = time.monotonic()
        codes = run(args, work, port, scenario)
        failures = check(work, codes, args.players, typed, bool(args.relay), scenario)
        print(f"run {run_index}: {time.monotonic() - started:.1f} s")
        if failures:
            print("\n".join(failures))
            return 1
        results.append(work)
    for name in peer_names(args.players):
        for item in (f"{name}.trace", f"{name}/network.trace"):
            a, b = (w / item for w in results)
            if name in killed or (scenario or {}).get("loose"):
                continue   # killed at an arbitrary point of its next tick; or not repeatable (above)
            if a.read_bytes() != b.read_bytes():
                print(f"not repeatable: {item} differs between runs")
                return 1
    if args.reference:
        work = args.work / "reference"
        codes = run(args, work, port, None, impaired=False)
        failures = check(work, codes, args.players, port if args.internet else None, bool(args.relay))
        if failures:
            print("reference run: " + "\n".join(failures))
            return 1
        def audit_lines(path):
            return [l for l in path.read_text(encoding="utf-8").splitlines() if " Internet address " not in l]
        for name in peer_names(args.players):
            if (results[0] / f"{name}.trace").read_bytes() != (work / f"{name}.trace").read_bytes():
                print(f"{name}.trace differs from the reference run (without --impair/--scenario)")
                return 1
            if audit_lines(results[0] / name / "network.trace") != audit_lines(work / name / "network.trace"):
                print(f"{name}/network.trace differs from the reference run (apart from the typed address)")
                return 1
        print("frame traces and audits equal the reference run's")
    how = " through the Internet provider and a typed address" if args.internet else ""
    if args.impair:
        how += f" through a lossy proxy ({args.impair})"
    if args.scenario:
        how += f", scenario {args.scenario}"
    if args.relay:
        how = " through a Free Direct server and a room" + (" over wss://" if args.tls else "")
    print(f"{args.players}-instance session{how}: create/join/lobby/ready/start/play passed; each peer's traces repeat exactly")
    return 0


if __name__ == "__main__":
    sys.exit(main())

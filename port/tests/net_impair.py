"""A lossy UDP proxy for the multiplayer tests (net_session.py --impair; port/docs/MULTIPLAYER.md).

The client is pointed at the proxy's port instead of the host's. Every datagram, in both directions, is
dropped with probability `loss`, else delivered after `delay` +/- `jitter` milliseconds (uniformly; a
later datagram with a shorter delay overtakes an earlier one, so jitter reorders), and duplicated with
probability `duplicate`. Each client address gets its own socket toward the host, so the host sees one
peer per client. The random choices are seeded; the timing is the operating system's.
"""
import heapq
import os
import random
import select
import socket
import threading
import time


def parse(spec):
    """'loss=0.1,delay=40,jitter=30,duplicate=0.02,seed=7' -> dict (missing keys default to 0)."""
    values = {"loss": 0.0, "delay": 0.0, "jitter": 0.0, "duplicate": 0.0, "seed": 1.0}
    for item in filter(None, spec.split(",")):
        key, _, value = item.partition("=")
        if key not in values:
            raise ValueError(f"unknown impairment '{key}' (loss, delay, jitter, duplicate, seed)")
        values[key] = float(value)
    if not 0 <= values["loss"] < 1 or not 0 <= values["duplicate"] < 1 or values["delay"] < 0 or values["jitter"] < 0:
        raise ValueError(f"impairment out of range: {spec}")
    return values


class UdpProxy:
    def __init__(self, target_port, impairment):
        self.target = ("127.0.0.1", target_port)
        self.impairment = impairment
        self.random = random.Random(int(impairment["seed"]))
        self.front = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.front.bind(("127.0.0.1", 0))
        self.port = self.front.getsockname()[1]
        self.clients = {}    # client address -> socket toward the host
        self.owners = {}     # that socket -> client address
        self.queue = []      # (due, order, socket, data, address)
        self.order = 0
        self.stats = {"datagrams": 0, "dropped": 0, "duplicated": 0, "reordered": 0}
        self.last_due = {}   # per direction: the latest due time scheduled so far
        self.running = True
        # NET_IMPAIR_LOG=<file>: one line per datagram (time, direction, size, fate), for diagnosis.
        self.log = open(os.environ["NET_IMPAIR_LOG"], "w") if os.environ.get("NET_IMPAIR_LOG") else None
        self.started = time.monotonic()
        self.thread = threading.Thread(target=self._run, daemon=True)
        self.thread.start()

    def _schedule(self, sock, data, address, direction):
        imp = self.impairment
        self.stats["datagrams"] += 1
        if self.random.random() < imp["loss"]:
            self.stats["dropped"] += 1
            if self.log:
                self.log.write(f"{time.monotonic() - self.started:9.3f} {direction:4} {len(data):5} dropped\n")
            return
        if self.log:
            self.log.write(f"{time.monotonic() - self.started:9.3f} {direction:4} {len(data):5}\n")
        copies = 2 if self.random.random() < imp["duplicate"] else 1
        if copies == 2:
            self.stats["duplicated"] += 1
        now = time.monotonic()
        for _ in range(copies):
            delay = max(0.0, imp["delay"] + self.random.uniform(-imp["jitter"], imp["jitter"])) / 1000
            due = now + delay
            if due < self.last_due.get(direction, 0.0):
                self.stats["reordered"] += 1
            self.last_due[direction] = max(due, self.last_due.get(direction, 0.0))
            self.order += 1
            heapq.heappush(self.queue, (due, self.order, sock, data, address))

    def _run(self):
        while self.running:
            timeout = 0.01
            if self.queue:
                timeout = max(0.0, min(timeout, self.queue[0][0] - time.monotonic()))
            readable, _, _ = select.select([self.front, *self.owners], [], [], timeout)
            for sock in readable:
                try:
                    data, source = sock.recvfrom(65536)
                except OSError:
                    continue
                if sock is self.front:
                    upstream = self.clients.get(source)
                    if upstream is None:
                        upstream = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                        upstream.bind(("127.0.0.1", 0))
                        self.clients[source] = upstream
                        self.owners[upstream] = source
                    self._schedule(upstream, data, self.target, "up")
                else:
                    self._schedule(self.front, data, self.owners[sock], "down")
            now = time.monotonic()
            while self.queue and self.queue[0][0] <= now:
                _, _, sock, data, address = heapq.heappop(self.queue)
                try:
                    sock.sendto(data, address)
                except OSError:
                    pass

    def stop(self):
        self.running = False
        self.thread.join(timeout=2)
        if self.log:
            self.log.close()
        for sock in [self.front, *self.owners]:
            sock.close()

    def summary(self):
        s = self.stats
        return (f"{s['datagrams']} datagrams: {s['dropped']} dropped, {s['duplicated']} duplicated, "
                f"{s['reordered']} delivered before an earlier one")

#!/usr/bin/env python3
"""A TCP proxy that stalls, as TCP does when it resends a lost packet: from time to time it holds one
direction of a connection for a while and then lets everything it held through at once. Between a
game and a Free Direct server (WebSocket over TCP) it shows what a lossy Internet path does to the
messages: none is lost, but some arrive late and together.

    tcp_stall.py --listen 51400 --target 127.0.0.1:51325 --every 3 --stall 300 [--seed 1]

Each direction of each connection stalls on average once every --every seconds (a Poisson process)
for --stall milliseconds; --direction picks the server-to-client side (down), the other (up) or
both. Only for tests on this machine (port/docs/MULTIPLAYER.md, "Lag").
"""
import argparse
import asyncio
import random
import time


async def pump(reader, writer, stalls):
    """Copy one direction; while a stall is on, hold what arrives and send it when the stall ends."""
    held = bytearray()
    try:
        while True:
            now = time.monotonic()
            until = stalls.until(now)
            if until:
                timeout = until - now
            else:
                if held:
                    writer.write(bytes(held))
                    held.clear()
                    await writer.drain()
                timeout = stalls.next_gap(now) or None
            try:
                chunk = await asyncio.wait_for(reader.read(65536), timeout)
            except asyncio.TimeoutError:
                continue
            if not chunk:
                break
            if stalls.until(time.monotonic()):
                held += chunk
            else:
                writer.write(chunk)
                await writer.drain()
    except ConnectionError:
        pass
    finally:
        if held:
            try:
                writer.write(bytes(held))
                await writer.drain()
            except ConnectionError:
                pass
        writer.close()


class Stalls:
    """When one direction stalls: exponential gaps of mean `every` seconds, each stall `stall` s."""

    def __init__(self, rng, every, stall, on):
        self.rng, self.every, self.stall, self.on = rng, every, stall, on
        self.start = time.monotonic() + (rng.expovariate(1 / every) if on else 1e18)
        self.count = 0

    def until(self, now):
        if not self.on:
            return 0
        while now >= self.start + self.stall:
            self.start += self.stall + self.rng.expovariate(1 / self.every)
        if now >= self.start:
            return self.start + self.stall
        return 0

    def next_gap(self, now):
        if not self.on:
            return 0
        return max(0.001, self.start - now)


async def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--listen", type=int, required=True)
    parser.add_argument("--target", required=True, help="HOST:PORT of the server")
    parser.add_argument("--every", type=float, default=3.0, help="mean seconds between stalls")
    parser.add_argument("--stall", type=float, default=300, help="milliseconds of each stall")
    parser.add_argument("--direction", choices=("down", "up", "both"), default="down")
    parser.add_argument("--seed", type=int, default=1)
    args = parser.parse_args()
    host, port = args.target.rsplit(":", 1)
    rng = random.Random(args.seed)

    async def handle(client_reader, client_writer):
        server_reader, server_writer = await asyncio.open_connection(host, int(port))
        down = Stalls(rng, args.every, args.stall / 1000, args.direction in ("down", "both"))
        up = Stalls(rng, args.every, args.stall / 1000, args.direction in ("up", "both"))
        await asyncio.gather(pump(server_reader, client_writer, down), pump(client_reader, server_writer, up))

    server = await asyncio.start_server(handle, "127.0.0.1", args.listen)
    print(f"tcp_stall: 127.0.0.1:{args.listen} -> {args.target}, a {args.stall:.0f} ms stall every "
          f"{args.every:g} s on average ({args.direction})", flush=True)
    async with server:
        await server.serve_forever()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass

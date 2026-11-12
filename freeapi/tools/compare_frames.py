#!/usr/bin/env python3
"""Compare frames captured with FREE_DIRECT_CAPTURE_PRIMARY across backing scales.

upscale BASE SCALED --scale K
    BASE was captured at backing scale 1, SCALED at scale K without an image
    pack. Every SCALED frame must be the nearest-neighbour K-times enlargement
    of a BASE frame: each KxK block uniform, and the reduced frame among BASE's.
    Blocks that are not uniform are reported with their bounding box (the
    games' minimap is stretched at the physical resolution).

same A B
    Two captures at the same scale (for example without and with a synthetic
    pack from make_test_pack.py): every frame of B must also occur in A.

Frames are compared by content, so captures may differ in timing (the games
run in real time and present only changed frames).
"""
import argparse
import hashlib
import struct
import sys
from pathlib import Path


def read_frame(path):
    """(width, height, rows) of a 24-bit BMP, rows top-down as bytes."""
    data = path.read_bytes()
    offset, = struct.unpack_from('<I', data, 10)
    width, height = struct.unpack_from('<ii', data, 18)
    bpp, = struct.unpack_from('<H', data, 28)
    if bpp != 24:
        raise ValueError(f'{path}: {bpp} bpp, expected 24')
    stride = (width * 3 + 3) // 4 * 4
    rows = [data[offset + y * stride: offset + y * stride + width * 3] for y in range(abs(height))]
    if height > 0:
        rows.reverse()
    return width, abs(height), rows


def frames(folder):
    return sorted(Path(folder).glob('frame-*.bmp'))


def digest(rows):
    h = hashlib.sha1()
    for row in rows:
        h.update(row)
    return h.hexdigest()


def reduce(width, height, rows, k):
    """The frame reduced by k (top-left pixel of each block) and the bounding
    box of non-uniform blocks, or None when every block is uniform."""
    small_width = width // k
    reduced, bad = [], None
    for by in range(height // k):
        top = rows[by * k]
        small = bytearray(small_width * 3)
        for c in range(3):
            small[c::3] = top[c::3 * k]
        expected = bytearray(width * 3)
        for c in range(3):
            for j in range(k):
                expected[j * 3 + c::3 * k] = small[c::3]
        for j in range(k):
            row = rows[by * k + j]
            if row != expected:
                for bx in range(small_width):
                    if row[bx * 3 * k:(bx + 1) * 3 * k] != expected[bx * 3 * k:(bx + 1) * 3 * k]:
                        box = (bx, by, bx, by)
                        bad = box if bad is None else (min(bad[0], bx), min(bad[1], by), max(bad[2], bx), max(bad[3], by))
        reduced.append(bytes(small))
    return reduced, bad


def upscale(base, scaled, k):
    base_hashes = {digest(read_frame(p)[2]): p.name for p in frames(base)}
    total = matched = uniform = 0
    for path in frames(scaled):
        width, height, rows = read_frame(path)
        reduced, bad = reduce(width, height, rows, k)
        total += 1
        uniform += bad is None
        found = digest(reduced) in base_hashes
        matched += found
        if bad is not None or not found:
            where = '' if bad is None else f' non-uniform blocks x {bad[0]}..{bad[2]}, y {bad[1]}..{bad[3]} (logical)'
            print(f'{path.name}: {"reduced frame found in base" if found else "reduced frame NOT in base"};{where}')
    print(f'{total} frames at scale {k}: {uniform} pure {k}x enlargements, {matched} reduce to a frame of the base capture')
    return total > 0 and matched == total


def same(a, b):
    known = {digest(read_frame(p)[2]) for p in frames(a)}
    total = found = 0
    for path in frames(b):
        total += 1
        if digest(read_frame(path)[2]) in known:
            found += 1
        else:
            print(f'{path.name}: not in {a}')
    print(f'{found} of {total} frames of {b} also occur in {a}')
    return total > 0 and found == total


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest='mode', required=True)
    up = sub.add_parser('upscale')
    up.add_argument('base')
    up.add_argument('scaled')
    up.add_argument('--scale', type=int, default=2)
    eq = sub.add_parser('same')
    eq.add_argument('a')
    eq.add_argument('b')
    args = parser.parse_args()
    ok = upscale(args.base, args.scaled, args.scale) if args.mode == 'upscale' else same(args.a, args.b)
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Build a synthetic higher-resolution image pack from an installed game.

Every image of the game's IMAGE08 and IMAGE16 folders is enlarged SCALE times
by nearest neighbour, keeping its format (8-bit with its own palette, or
24-bit) and its relative path. Such a pack holds no new detail, so the game must
draw exactly the same frames with it as without it at the same backing scale:
the equivalence check for the overlay loader, GetObjectA's logical size and the
scaled GDI paths (README.md, "Higher-resolution images").

The installation is only read; the pack is written inside the repository's
build/ or scratch/ directory.
"""
import argparse
import struct
from pathlib import Path

IMAGE_FOLDERS = ('IMAGE08', 'IMAGE16')
EXTENSIONS = {'.blp', '.bmp'}


def enlarge_bmp(data, scale):
    """Return the BMP `data` enlarged `scale` times (uncompressed 8/24-bit only)."""
    if data[:2] != b'BM':
        raise ValueError('not a BMP file')
    offset, = struct.unpack_from('<I', data, 10)
    header_size, width, height, planes, bpp, compression = struct.unpack_from('<IiiHHI', data, 14)
    if header_size < 40 or compression != 0 or bpp not in (8, 24):
        raise ValueError(f'unsupported BMP (header {header_size}, {bpp} bpp, compression {compression})')
    rows = abs(height)
    stride = (width * bpp + 31) // 32 * 4
    pixel_bytes = bpp // 8
    new_width = width * scale
    new_stride = (new_width * bpp + 31) // 32 * 4
    out_rows = []
    for y in range(rows):
        row = data[offset + y * stride: offset + y * stride + width * pixel_bytes]
        wide = bytearray(new_stride)
        for channel in range(pixel_bytes):
            source = row[channel::pixel_bytes]
            for copy in range(scale):
                start = copy * pixel_bytes + channel
                wide[start:new_width * pixel_bytes:pixel_bytes * scale] = source
        out_rows.extend([bytes(wide)] * scale)
    header = bytearray(data[:offset])
    image_size = new_stride * rows * scale
    struct.pack_into('<I', header, 2, offset + image_size)
    struct.pack_into('<ii', header, 18, new_width, height * scale)
    struct.pack_into('<I', header, 34, image_size)
    return bytes(header) + b''.join(out_rows)


def build(game, output, scale):
    repo = Path(__file__).resolve().parents[2]
    if not any(output.is_relative_to(repo / name) for name in ('build', 'scratch')):
        raise ValueError('Write the pack inside the repository build/ or scratch/ directory')
    folders = {p.name.upper(): p for p in game.iterdir() if p.is_dir()}
    count = 0
    for name in IMAGE_FOLDERS:
        folder = folders.get(name)
        if folder is None:
            continue
        for source in sorted(folder.iterdir()):
            if not source.is_file() or source.suffix.lower() not in EXTENSIONS:
                continue
            target = output / folder.name / source.name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(enlarge_bmp(source.read_bytes(), scale))
            count += 1
    if not count:
        raise ValueError(f'No images under {game}')
    print(f'Wrote {count} images enlarged {scale}x to {output}')


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--game', type=Path, required=True, help='installed game folder (read only)')
    parser.add_argument('--output', type=Path, required=True, help='pack folder under build/ or scratch/')
    parser.add_argument('--scale', type=int, default=2, choices=range(1, 9), metavar='1..8')
    args = parser.parse_args()
    build(args.game.resolve(), args.output.resolve(), args.scale)


if __name__ == '__main__':
    main()

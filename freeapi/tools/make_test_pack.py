#!/usr/bin/env python3
"""Build a synthetic higher-resolution image pack from an installed game.

Every image of the game's IMAGE08 and IMAGE16 folders is enlarged SCALE times
by nearest neighbour, keeping its format (8-bit with its own palette, or
24-bit) and its relative path. Such a pack holds no new detail, so the game must
draw exactly the same frames with it as without it at the same backing scale:
the equivalence check for the overlay loader, GetObjectA's logical size and the
scaled GDI paths (README.md, "Higher-resolution images").

With --alpha, the sprite sheets the games key with blue are written as 32-bit
BMPs with an alpha channel instead: every pixel the 16-bit mode treats as the
key (RGB565 0x001F) becomes transparent black, every other pixel opaque. The
frames must still be the same, now through the alpha path (blending, and the
composition of the "hide" effect's scratch image).

The installation is only read; the pack is written inside the repository's
build/ or scratch/ directory.
"""
import argparse
import struct
from pathlib import Path

IMAGE_FOLDERS = ('IMAGE08', 'IMAGE16')
EXTENSIONS = {'.blp', '.bmp'}
# Sheets both games key with RGB(0,0,255) (src/blupi.cpp, games/sb2/src/pixmap.cpp CacheAll).
BLUE_KEYED = {'object', 'blupi000', 'blupi001', 'blupi002', 'blupi003', 'temp', 'map',
              'element', 'explo', 'button00', 'jauge', 'text', 'little'}


def rgb_rows(data):
    """(width, height, rows of (r, g, b) tuples, bottom_up) of an uncompressed 8/24-bit BMP."""
    offset, = struct.unpack_from('<I', data, 10)
    header_size, width, height, _, bpp = struct.unpack_from('<IiiHH', data, 14)
    stride = (width * bpp + 31) // 32 * 4
    palette = []
    if bpp == 8:
        start = 14 + header_size
        palette = [(data[i + 2], data[i + 1], data[i]) for i in range(start, offset, 4)]
    rows = []
    for y in range(abs(height)):
        row = data[offset + y * stride: offset + y * stride + width * bpp // 8]
        if bpp == 8:
            rows.append([palette[i] for i in row])
        else:
            rows.append([(row[i + 2], row[i + 1], row[i]) for i in range(0, len(row), 3)])
    return width, abs(height), rows, height > 0


def keyed_to_alpha(data):
    """The BMP `data` as a 32-bit BMP (BITMAPV4HEADER, alpha mask): pixels whose RGB565
    truncation is the blue key 0x001F become transparent black, the others opaque."""
    width, height, rows, bottom_up = rgb_rows(data)
    pixels = bytearray()
    for row in rows:
        for r, g, b in row:
            if ((r >> 3) << 11 | (g >> 2) << 5 | (b >> 3)) == 0x001F:
                pixels += b'\x00\x00\x00\x00'
            else:
                pixels += bytes((b, g, r, 255))
    header = struct.pack('<IiiHHIIiiII', 108, width, height if bottom_up else -height, 1, 32, 3,
                         len(pixels), 2835, 2835, 0, 0)
    header += struct.pack('<IIII', 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000)
    header += b'BGRs' + bytes(36 + 12)
    offset = 14 + len(header)
    return b'BM' + struct.pack('<IHHI', offset + len(pixels), 0, 0, offset) + header + bytes(pixels)


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


def build(game, output, scale, alpha=False, sheets_only=False):
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
            if sheets_only and source.stem.lower() not in BLUE_KEYED:
                continue
            target = output / folder.name / source.name
            target.parent.mkdir(parents=True, exist_ok=True)
            image = enlarge_bmp(source.read_bytes(), scale)
            if alpha and source.stem.lower() in BLUE_KEYED:
                image = keyed_to_alpha(image)
            target.write_bytes(image)
            count += 1
    if not count:
        raise ValueError(f'No images under {game}')
    print(f'Wrote {count} images enlarged {scale}x{" (blue keys as alpha)" if alpha else ""} to {output}')


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--game', type=Path, required=True, help='installed game folder (read only)')
    parser.add_argument('--output', type=Path, required=True, help='pack folder under build/ or scratch/')
    parser.add_argument('--scale', type=int, default=2, choices=range(1, 9), metavar='1..8')
    parser.add_argument('--alpha', action='store_true', help='blue-keyed sheets as 32-bit BMPs with alpha')
    parser.add_argument('--sheets-only', action='store_true',
                        help='only the blue-keyed sprite sheets (other images fall back to the originals)')
    args = parser.parse_args()
    build(args.game.resolve(), args.output.resolve(), args.scale, args.alpha, args.sheets_only)


if __name__ == '__main__':
    main()

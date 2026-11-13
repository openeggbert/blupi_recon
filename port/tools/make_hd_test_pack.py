#!/usr/bin/env python3
"""A synthetic higher-resolution image pack for the port's tests (port/docs/HD.md).

Every image of the game's image folders (IMAGE08, IMAGE16, or the June 1998 demo's IMAGE) is enlarged
SCALE times by pixel repetition, keeping its format (8 bits with its own colour table, or 24 bits) and
its relative path. Such a pack holds no new detail, so the port must show exactly the same frames with it
as without it at the same --hd-scale (--hd-check): the check of the pack loader, of the size rule and of
the conversion of the pack's images. After freeapi/tools/make_test_pack.py, without its alpha variant.

The game folder is only read; write the pack below the repository's build/ directory.

    python3 port/tools/make_hd_test_pack.py --game <game folder> --scale 2 --output build/<dir>
"""
import argparse
import struct
from pathlib import Path

IMAGE_FOLDERS = {"image08", "image16", "image"}
EXTENSIONS = {".blp", ".bmp"}


def enlarge(data, scale):
    """An uncompressed 8- or 24-bit BMP, enlarged; None for any other kind."""
    if data[:2] != b"BM":
        return None
    offset, = struct.unpack_from("<I", data, 10)
    header_size, width, height, planes, bpp, compression = struct.unpack_from("<IiiHHI", data, 14)
    if bpp not in (8, 24) or compression != 0 or width <= 0 or height == 0:
        return None
    rows = abs(height)
    stride = (width * bpp + 31) // 32 * 4
    pixel = bpp // 8
    table = data[14 + header_size:offset] if bpp == 8 else b""
    big_width = width * scale
    big_stride = (big_width * bpp + 31) // 32 * 4
    out_rows = []
    for y in range(rows):
        row = data[offset + y * stride:offset + y * stride + width * pixel]
        wide = b"".join(row[x * pixel:(x + 1) * pixel] * scale for x in range(width))
        wide += b"\0" * (big_stride - len(wide))
        out_rows += [wide] * scale
    pixels = b"".join(out_rows)
    header = struct.pack("<IiiHHIIiiII", 40, big_width, height * scale, 1, bpp, 0, len(pixels), 2835, 2835,
                         len(table) // 4, 0)
    start = 14 + len(header) + len(table)
    return b"BM" + struct.pack("<IHHI", start + len(pixels), 0, 0, start) + header + table + pixels


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--game", type=Path, required=True)
    ap.add_argument("--scale", type=int, default=2, choices=(2, 3, 4))
    ap.add_argument("--output", type=Path, required=True)
    ap.add_argument("--only", default="", help="comma-separated image names (any case, no extension): only these")
    a = ap.parse_args()
    only = {n.strip().lower() for n in a.only.split(",") if n.strip()}
    count = 0
    for folder in sorted(p for p in a.game.iterdir() if p.is_dir() and p.name.lower() in IMAGE_FOLDERS):
        for image in sorted(folder.iterdir()):
            if image.suffix.lower() not in EXTENSIONS or not image.is_file():
                continue
            if only and image.stem.lower() not in only:
                continue
            big = enlarge(image.read_bytes(), a.scale)
            if big is None:
                continue
            target = a.output / folder.name / image.name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(big)
            count += 1
    print(f"{count} images enlarged {a.scale} times into {a.output}")
    return 0 if count else 1


if __name__ == "__main__":
    raise SystemExit(main())

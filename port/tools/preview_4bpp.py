#!/usr/bin/env python3
"""Render a GBA 4bpp tileset to a PNG so you can eyeball an extracted asset.

GBA 4bpp tiles are 8x8 pixels, 32 bytes each, two pixels per byte with the
LOW nibble holding the LEFT pixel. Colour 0 is transparent on hardware; here
it is rendered as the palette's entry 0 (or mid-grey without a palette).

Palettes are RGB555 little-endian halfwords, 16 entries per bank.
This writes a plain RGB PNG using only the standard library.
"""

import argparse
import struct
import sys
import zlib

TILE_BYTES = 32
TILE_W = TILE_H = 8


def write_png(path, width, height, rows_rgb):
    """rows_rgb: list of bytearrays, each width*3 bytes."""
    raw = bytearray()
    for row in rows_rgb:
        raw.append(0)          # filter type 0 (None)
        raw.extend(row)

    def chunk(tag, payload):
        body = tag + payload
        return struct.pack(">I", len(payload)) + body + struct.pack(">I", zlib.crc32(body))

    header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)  # 8-bit truecolour
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", header))
        f.write(chunk(b"IDAT", zlib.compress(bytes(raw), 9))) 
        f.write(chunk(b"IEND", b""))


def rgb555_to_rgb888(value):
    r, g, b = value & 0x1F, (value >> 5) & 0x1F, (value >> 10) & 0x1F
    # replicate the top 3 bits into the low bits so 0x1F maps to 0xFF
    return ((r << 3) | (r >> 2), (g << 3) | (g >> 2), (b << 3) | (b >> 2))


def load_palette(path, bank):
    data = open(path, "rb").read()
    off = bank * 32
    if off + 32 > len(data):
        raise SystemExit("palette bank %d is beyond the end of %s" % (bank, path))
    return [rgb555_to_rgb888(v) for v in struct.unpack_from("<16H", data, off)]


def grayscale_palette():
    # colour 0 stays visibly distinct so you can see tile boundaries
    return [(255, 0, 255)] + [(i * 17, i * 17, i * 17) for i in range(1, 16)]


def render(tiles_bytes, palette, tiles_per_row):
    tile_count = len(tiles_bytes) // TILE_BYTES
    if tile_count == 0:
        raise SystemExit("input holds less than one full tile")

    rows_of_tiles = (tile_count + tiles_per_row - 1) // tiles_per_row
    width = tiles_per_row * TILE_W
    height = rows_of_tiles * TILE_H

    rows = [bytearray(width * 3) for _ in range(height)]

    for index in range(tile_count):
        tx = (index % tiles_per_row) * TILE_W
        ty = (index // tiles_per_row) * TILE_H
        base = index * TILE_BYTES
        for y in range(TILE_H):
            row = rows[ty + y]
            for byte_index in range(4):
                packed = tiles_bytes[base + y * 4 + byte_index]
                for half in range(2):
                    # low nibble is the left pixel
                    color = palette[(packed >> (half * 4)) & 0xF]
                    px = (tx + byte_index * 2 + half) * 3
                    row[px:px + 3] = bytes(color)
    return width, height, rows


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("input", help="raw .4bpp tile data")
    ap.add_argument("output", help="PNG to write")
    ap.add_argument("--palette", help="raw RGB555 palette file (16 colours per bank)")
    ap.add_argument("--bank", type=int, default=0, help="palette bank index (default 0)")
    ap.add_argument("--width", type=int, default=16, help="tiles per row (default 16)")
    args = ap.parse_args()

    tiles = open(args.input, "rb").read()
    leftover = len(tiles) % TILE_BYTES
    if leftover:
        print("note: %d trailing bytes are not a whole tile and were dropped"
              % leftover, file=sys.stderr)
        tiles = tiles[:len(tiles) - leftover]

    palette = load_palette(args.palette, args.bank) if args.palette else grayscale_palette()
    width, height, rows = render(tiles, palette, args.width)
    write_png(args.output, width, height, rows)
    print("wrote %s  %dx%d  (%d tiles)" % (args.output, width, height,
                                           len(tiles) // TILE_BYTES))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

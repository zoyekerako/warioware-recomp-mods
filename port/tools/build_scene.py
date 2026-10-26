#!/usr/bin/env python3
"""Apply a GraphicsTable to a simulated GBA video state and dump it for the renderer.

This is the offline stand-in for what the game does at runtime: walk a
GraphicsTable, decompress or copy each entry into VRAM or the palette shadow,
then hand the whole state to the compositor. The output file is a flat,
fixed-layout blob so the C side needs no parser:

    "WWSC"                4
    version = 1           4
    dispcnt               2
    (padding)             2
    bgcnt[4]              8
    bg_hofs[4]  (s16)     8
    bg_vofs[4]  (s16)     8
    bg_palette[256]     512
    obj_palette[256]    512
    VRAM             0x18000
    OAM               0x400
"""

import argparse
import json
import os
import struct

ROM_BASE = 0x08000000
VRAM_BASE = 0x06000000
VRAM_SIZE = 0x18000
OAM_SIZE = 0x400
GFXBUF = 0x03004000
BG_PAL = GFXBUF + 0x54
OBJ_PAL = GFXBUF + 0x254
PAL_END = GFXBUF + 0x454


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--tables", default="assets/graphics_tables.json")
    ap.add_argument("--assets", default="assets/gfx")
    ap.add_argument("--rom", default="../rom.gba", help="needed for raw-copy entries")
    ap.add_argument("--table", required=True, help="table address, e.g. 0x083A8CAC")
    ap.add_argument("--bg", action="append", default=[], metavar="LAYER:BGCNT",
                    help="enable a background, e.g. 1:0x1D01 (repeatable)")
    ap.add_argument("--scroll", action="append", default=[], metavar="LAYER:X:Y")
    ap.add_argument("--dispcnt", type=lambda s: int(s, 0), default=None,
                    help="override DISPCNT instead of deriving it from --bg")
    ap.add_argument("--oam", help="raw 1 KiB OAM dump to include")
    ap.add_argument("-o", "--out", default="scene.bin")
    args = ap.parse_args()

    rom = open(args.rom, "rb").read()
    tables = {t["address"]: t for t in json.load(open(args.tables))["data"]}
    key = "0x%08X" % int(args.table, 0)
    if key not in tables:
        raise SystemExit("no table at %s" % key)

    vram = bytearray(VRAM_SIZE)
    bg_pal = bytearray(512)
    obj_pal = bytearray(512)
    applied = skipped = 0

    for row in tables[key]["table"]:
        dest = int(row["dest"], 16)
        src = int(row["src"], 16)

        if row["mode"] == "function":
            # A custom loader; nothing to apply without running its code.
            skipped += 1
            continue

        if row["mode"] == "compressed":
            name = row.get("asset")
            if not name:
                skipped += 1
                continue
            blob = open(os.path.join(args.assets, name), "rb").read()
        else:
            blob = rom[src - ROM_BASE: src - ROM_BASE + row["size"]]

        if VRAM_BASE <= dest < VRAM_BASE + VRAM_SIZE:
            off = dest - VRAM_BASE
            vram[off:off + len(blob)] = blob[:VRAM_SIZE - off]
        elif BG_PAL <= dest < OBJ_PAL:
            off = dest - BG_PAL
            bg_pal[off:off + len(blob)] = blob[:512 - off]
        elif OBJ_PAL <= dest < PAL_END:
            off = dest - OBJ_PAL
            obj_pal[off:off + len(blob)] = blob[:512 - off]
        else:
            skipped += 1
            continue
        applied += 1

    bgcnt = [0, 0, 0, 0]
    hofs = [0, 0, 0, 0]
    vofs = [0, 0, 0, 0]
    enabled = 0
    for spec in args.bg:
        layer, value = spec.split(":")
        layer = int(layer, 0)
        bgcnt[layer] = int(value, 0) & 0xFFFF
        enabled |= 0x0100 << layer
    for spec in args.scroll:
        layer, x, y = spec.split(":")
        hofs[int(layer, 0)] = int(x, 0)
        vofs[int(layer, 0)] = int(y, 0)

    dispcnt = args.dispcnt if args.dispcnt is not None else enabled

    oam = bytearray(OAM_SIZE)
    if args.oam:
        raw = open(args.oam, "rb").read()
        oam[:min(len(raw), OAM_SIZE)] = raw[:OAM_SIZE]
        dispcnt |= 0x1000  # OBJ enable

    with open(args.out, "wb") as f:
        f.write(b"WWSC")
        f.write(struct.pack("<I", 1))
        f.write(struct.pack("<HH", dispcnt, 0))
        f.write(struct.pack("<4H", *bgcnt))
        f.write(struct.pack("<4h", *hofs))
        f.write(struct.pack("<4h", *vofs))
        f.write(bytes(bg_pal))
        f.write(bytes(obj_pal))
        f.write(bytes(vram))
        f.write(bytes(oam))

    print("table %s: %d entries applied, %d skipped" % (key, applied, skipped))
    print("DISPCNT=0x%04X  BGCNT=%s" % (dispcnt, ["0x%04X" % c for c in bgcnt]))
    print("wrote %s (%d bytes)" % (args.out, os.path.getsize(args.out)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

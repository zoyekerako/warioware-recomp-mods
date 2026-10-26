#!/usr/bin/env python3
"""Recover the GraphicsTable arrays from a WarioWare Inc. ROM.

A GraphicsTable is an array of 12-byte records terminated by an all-zero one:

    struct GraphicsTable { const void *src; void *dest; s32 size; };

`size` is a discriminator as much as a length:

    -1  COMPRESSED_GFX_SOURCE  src points at a CompressedData descriptor
    -2  FUNCTION_GFX_SOURCE    src is a loader function called with dest
    >0  plain byte copy of that many bytes

`dest` says what the asset actually IS, which the CompressedData descriptor
alone cannot tell you:

    0x06000000..0x0600FFFF   BG VRAM    (charblocks of 0x4000, screenblocks of 0x800)
    0x06010000..0x06017FFF   OBJ VRAM
    0x03004054..0x03004253   BG palette bank  (gGraphicsBuffer.bgPalette + n*32)
    0x03004254..0x03004453   OBJ palette bank (gGraphicsBuffer.objPalette + n*32)

Outputs a JSON index of every table and, with --extract-palettes, dumps each
palette entry as a raw RGB555 file.
"""

import argparse
import json
import os
import struct

ROM_BASE = 0x08000000
ROM_END = 0x08800000

VRAM_BASE = 0x06000000
VRAM_OBJ = 0x06010000
VRAM_END = 0x06018000

# gGraphicsBuffer lives at IWRAM 0x03004000; the palette shadows sit inside it.
GFXBUF = 0x03004000
BG_PAL = GFXBUF + 0x54
OBJ_PAL = GFXBUF + 0x254
PAL_END = GFXBUF + 0x454

ENTRY = 12
MAX_RAW_SIZE = 0x8000


def is_rom_ptr(addr):
    return ROM_BASE <= addr < ROM_END


def classify_dest(dest):
    """Return (kind, index) for a destination address."""
    if VRAM_BASE <= dest < VRAM_OBJ:
        off = dest - VRAM_BASE
        # A charblock base is also a valid screenblock base, so report both.
        return ("bg_vram", {"offset": off,
                            "charblock": off // 0x4000,
                            "screenblock": off // 0x800})
    if VRAM_OBJ <= dest < VRAM_END:
        return ("obj_vram", {"offset": dest - VRAM_OBJ})
    # A palette write is NOT necessarily bank-aligned: the game also patches a
    # few colours in the middle of a bank (e.g. 8 bytes = 4 colours at +0x10).
    # Requiring 32-byte alignment here silently truncates whole tables.
    if BG_PAL <= dest < OBJ_PAL and dest % 2 == 0:
        rel = dest - BG_PAL
        return ("bg_palette", {"bank": rel // 32, "color": (rel % 32) // 2})
    if OBJ_PAL <= dest < PAL_END and dest % 2 == 0:
        rel = dest - OBJ_PAL
        return ("obj_palette", {"bank": rel // 32, "color": (rel % 32) // 2})
    if dest == 0:
        return ("null", {})
    return ("other", {})


def plausible_entry(src, dest, size):
    if size == 0:
        return False
    if size not in (-1, -2) and not (0 < size <= MAX_RAW_SIZE):
        return False
    if not is_rom_ptr(src):
        return False
    # A custom loader computes its own destination, so dest == NULL is valid for
    # size == -2. It cannot be confused with the {0, 0, 0} terminator because
    # that one has size == 0, which is rejected above.
    if size == -2:
        return dest == 0 or classify_dest(dest)[0] not in ("null", "other")
    kind, _ = classify_dest(dest)
    if kind in ("null", "other"):
        return False
    # A raw copy into a palette bank must be an even number of RGB555 entries.
    if kind.endswith("palette") and size > 0 and size % 2:
        return False
    return True


def scan_tables(rom, min_entries=2):
    """Find maximal runs of plausible entries that end in an all-zero record."""
    tables = []
    consumed = set()
    limit = len(rom) - ENTRY

    off = 0
    while off <= limit:
        if off in consumed:
            off += 4
            continue

        entries = []
        cursor = off
        while cursor + ENTRY <= len(rom):
            src, dest, size = struct.unpack_from("<IIi", rom, cursor)
            if src == 0 and dest == 0 and size == 0:
                break
            if not plausible_entry(src, dest, size):
                entries = []
                break
            entries.append((src, dest, size))
            cursor += ENTRY

        if len(entries) >= min_entries:
            tables.append((ROM_BASE + off, entries))
            for c in range(off, cursor + ENTRY, 4):
                consumed.add(c)
            off = cursor + ENTRY
        else:
            off += 4

    return tables


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("rom")
    ap.add_argument("-o", "--out", default="assets/graphics_tables.json")
    ap.add_argument("--gfx-manifest", default="assets/gfx/manifest.json",
                    help="manifest from extract_gfx.py, used to join src pointers")
    ap.add_argument("--extract-palettes", metavar="DIR",
                    help="also dump every palette entry as raw RGB555")
    ap.add_argument("--roots", metavar="FILE",
                    help="file of hex table addresses (one per line) to parse "
                         "directly, bypassing the blind-scan run heuristic")
    ap.add_argument("--min-entries", type=int, default=2,
                    help="reject runs shorter than this (default 2)")
    args = ap.parse_args()

    rom = open(args.rom, "rb").read()

    known = {}
    try:
        for a in json.load(open(args.gfx_manifest))["assets"]:
            known[int(a["address"], 16)] = a
    except (OSError, ValueError, KeyError):
        # No manifest yet: the join is optional, so run without it.
        pass

    tables = scan_tables(rom, args.min_entries)

    if args.roots:
        have = {addr for addr, _ in tables}
        added = 0
        for line in open(args.roots):
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            addr = int(line, 16)
            if addr in have:
                continue
            entries = []
            cursor = addr - ROM_BASE
            while cursor + ENTRY <= len(rom):
                src, dest, size = struct.unpack_from("<IIi", rom, cursor)
                if src == 0 and dest == 0 and size == 0:
                    break
                if not plausible_entry(src, dest, size):
                    entries = []
                    break
                entries.append((src, dest, size))
                cursor += ENTRY
            if entries:
                tables.append((addr, entries))
                added += 1
        if added:
            print("added %d tables from %s" % (added, args.roots))
        tables.sort(key=lambda t: t[0])

    out_tables = []
    kinds = {}
    modes = {"compressed": 0, "function": 0, "raw": 0}
    matched = set()
    palettes = []

    for addr, entries in tables:
        rows = []
        for src, dest, size in entries:
            kind, detail = classify_dest(dest)
            kinds[kind] = kinds.get(kind, 0) + 1
            mode = {-1: "compressed", -2: "function"}.get(size, "raw")
            modes[mode] += 1

            row = {"src": "0x%08X" % src, "dest": "0x%08X" % dest,
                   "kind": kind, "mode": mode}
            row.update(detail)
            if mode == "raw":
                row["size"] = size
            if mode == "compressed" and src in known:
                row["asset"] = known[src]["file"]
                row["asset_bytes"] = known[src]["bytes"]
                matched.add(src)
            rows.append(row)

            if kind.endswith("palette") and mode == "raw":
                palettes.append((src, size, kind, detail.get("bank", 0)))

        out_tables.append({"address": "0x%08X" % addr,
                           "entries": len(rows), "table": rows})

    os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
    with open(args.out, "w") as f:
        json.dump({"tables": len(out_tables), "data": out_tables}, f, indent=1)

    total = sum(t["entries"] for t in out_tables)
    print("found %d graphics tables, %d entries" % (len(out_tables), total))
    print("  by mode: " + ", ".join("%s=%d" % kv for kv in sorted(modes.items())))
    print("  by dest: " + ", ".join("%s=%d" % kv for kv in sorted(kinds.items())))
    if known:
        print("  compressed entries matched to an extracted asset: %d distinct sources"
              % len(matched))
    print("wrote %s" % args.out)

    if args.extract_palettes:
        os.makedirs(args.extract_palettes, exist_ok=True)
        written = set()
        for src, size, kind, bank in palettes:
            name = "pal_%08X_%d.bin" % (src, size)
            if name in written:
                continue
            # Copy exactly `size` bytes from ROM: the game deliberately over-reads
            # past the declared C array, so trimming here would change behaviour.
            blob = rom[src - ROM_BASE: src - ROM_BASE + size]
            with open(os.path.join(args.extract_palettes, name), "wb") as f:
                f.write(blob)
            written.add(name)
        print("extracted %d distinct palette blobs -> %s"
              % (len(written), args.extract_palettes))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

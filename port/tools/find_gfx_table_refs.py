#!/usr/bin/env python3
"""Find GraphicsTable pointers that Beatscript programs load at runtime.

Beatscript instructions are fixed 12-byte records:

    word 0 = (opcode & 0xFF) | (param1 << 8)
    word 1 = param2
    word 2 = param3

Two opcodes take the address of a GraphicsTable as param2:

    0x2D  load_graphics <address>            (param1 = 0, param3 = 0)
    0x3E  load_graphics_async <address>, <limit>

Reading just those two parameters reaches the graphics of scenes whose tables a
blind ROM scan misses, WITHOUT decompiling the Beatscript interpreter.
"""

import argparse
import struct

ROM_BASE = 0x08000000
ROM_END = 0x08800000
MAX_ASYNC_LIMIT = 0x10000


def scan(rom):
    hits = {"load_graphics": [], "load_graphics_async": []}
    for off in range(0, len(rom) - 12, 4):
        word0 = struct.unpack_from("<I", rom, off)[0]
        if word0 not in (0x2D, 0x3E):
            continue
        # param1 must be 0, which word0 == opcode already guarantees.
        addr, param3 = struct.unpack_from("<II", rom, off + 4)
        if not (ROM_BASE <= addr < ROM_END):
            continue
        if word0 == 0x2D:
            if param3 != 0:
                continue
            hits["load_graphics"].append((ROM_BASE + off, addr))
        else:
            # the async form's third parameter is a small byte budget
            if param3 >= MAX_ASYNC_LIMIT:
                continue
            hits["load_graphics_async"].append((ROM_BASE + off, addr))
    return hits


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("rom")
    ap.add_argument("-o", "--out", default="assets/beatscript_table_refs.txt",
                    help="write the unique table addresses here, one per line")
    args = ap.parse_args()

    rom = open(args.rom, "rb").read()
    hits = scan(rom)

    targets = sorted({addr for group in hits.values() for _, addr in group})
    with open(args.out, "w") as f:
        f.write("\n".join("0x%08X" % a for a in targets) + "\n")

    for name, group in hits.items():
        print("%-20s %4d call sites" % (name, len(group)))
    print("unique table addresses: %d  (0x%08X .. 0x%08X)"
          % (len(targets), targets[0], targets[-1]))
    print("wrote %s" % args.out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

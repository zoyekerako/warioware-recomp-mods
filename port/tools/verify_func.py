#!/usr/bin/env python3
"""Compare a compiled function against the original ROM bytes.

Compiles one .c through the agbcc pipeline, pulls a named symbol out of the
resulting object and diffs it against the same address range in the ROM. This
is the check that makes a decompilation "matching" rather than merely plausible.

Literal pools hold absolute addresses that the linker fills in, so bytes at
relocation sites legitimately differ in an unlinked object. Those are reported
separately from real mismatches.
"""

import argparse
import os
import re
import struct
import subprocess
import sys

ROM_BASE = 0x08000000


def run(cmd, **kw):
    return subprocess.run(cmd, shell=True, capture_output=True, text=True, **kw)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("source", help=".c file to compile")
    ap.add_argument("symbol", help="function symbol, e.g. func_08000F28")
    ap.add_argument("--repo", default="../wariowareinc-main")
    ap.add_argument("--rom", default="../rom.gba")
    ap.add_argument("--addr", help="ROM address; default: parsed from the symbol name")
    ap.add_argument("--tmp", default="/tmp/wwverify")
    args = ap.parse_args()

    repo = os.path.abspath(args.repo)
    rom = open(os.path.abspath(args.rom), "rb").read()
    os.makedirs(args.tmp, exist_ok=True)

    if args.addr:
        addr = int(args.addr, 0)
    else:
        m = re.search(r"_([0-9A-Fa-f]{8})$", args.symbol)
        if not m:
            sys.exit("cannot infer an address from %r; pass --addr" % args.symbol)
        addr = int(m.group(1), 16)

    src = os.path.abspath(args.source)
    i_file = os.path.join(args.tmp, "t.i")
    s_file = os.path.join(args.tmp, "t.s")
    o_file = os.path.join(args.tmp, "t.o")

    cpp = ("cc -E -I tools/agbcc -I tools/agbcc/include -I . -iquote include "
           "-nostdinc -undef %s -o %s" % (src, i_file))
    r = run(cpp, cwd=repo)
    if r.returncode:
        sys.exit("preprocessor failed:\n" + r.stderr[:2000])

    cc = ("tools/agbcc/bin/agbcc -mthumb-interwork -Wparentheses -O2 -fhex-asm %s -o %s"
          % (i_file, s_file))
    r = run(cc, cwd=repo)
    if r.returncode:
        sys.exit("agbcc failed:\n" + (r.stderr or r.stdout)[:2000])

    with open(s_file, "a") as f:
        f.write('.text\n\t.align\t2, 0\n.section .note.GNU-stack,"",%progbits\n')

    r = run("arm-none-eabi-as -march=armv4t -o %s %s" % (o_file, s_file), cwd=repo)
    if r.returncode:
        sys.exit("assembler failed:\n" + r.stderr[:2000])

    # Locate the symbol and its size in the object.
    r = run("arm-none-eabi-nm -S --size-sort %s" % o_file)
    off = size = None
    for line in r.stdout.splitlines():
        parts = line.split()
        if len(parts) == 4 and parts[3] == args.symbol:
            off, size = int(parts[0], 16), int(parts[1], 16)
    if off is None:
        r = run("arm-none-eabi-nm %s" % o_file)
        names = [l.split()[-1] for l in r.stdout.splitlines() if l.split()]
        sys.exit("symbol %s not found. Present: %s" % (args.symbol, ", ".join(names[:20])))

    r = run("arm-none-eabi-objcopy -O binary --only-section=.text %s %s.bin"
            % (o_file, o_file))
    built = open(o_file + ".bin", "rb").read()[off:off + size]

    # Relocation sites in this function: those bytes are linker-filled.
    r = run("arm-none-eabi-objdump -r %s" % o_file)
    relocs = set()
    for line in r.stdout.splitlines():
        m = re.match(r"^([0-9a-f]{8})\s+R_ARM_\S+", line)
        if m:
            ro = int(m.group(1), 16)
            if off <= ro < off + size:
                relocs.update(range(ro - off, ro - off + 4))

    orig = rom[addr - ROM_BASE: addr - ROM_BASE + size]

    hard = [i for i in range(min(len(built), len(orig)))
            if built[i] != orig[i] and i not in relocs]
    soft = [i for i in range(min(len(built), len(orig)))
            if built[i] != orig[i] and i in relocs]

    print("symbol      : %s" % args.symbol)
    print("ROM address : 0x%08X" % addr)
    print("size        : built %d B, ROM slice %d B" % (len(built), len(orig)))
    print("relocations : %d byte positions" % len(relocs))
    print("mismatches  : %d real, %d at relocation sites" % (len(hard), len(soft)))

    if not hard and len(built) == len(orig):
        print("\nMATCH")
        return 0

    print("\nNO MATCH")
    for i in hard[:16]:
        print("  +0x%04X: built %02X, rom %02X" % (i, built[i], orig[i]))
    if len(hard) > 16:
        print("  ... %d more" % (len(hard) - 16))
    return 1


if __name__ == "__main__":
    raise SystemExit(main())

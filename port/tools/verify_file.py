#!/usr/bin/env python3
"""Verify a whole translation unit against the ROM.

Stronger than checking one function at a time: it compiles the entire .c and
compares its complete .text against the ROM starting at the unit's base
address. If that matches, every function in the file matches AND so does every
call site between them -- which is exactly what you need before changing a
shared header, because a signature change can silently alter a caller.

Bytes at relocation sites are counted separately; the linker fills those in.
"""

import argparse
import os
import re
import subprocess
import sys

ROM_BASE = 0x08000000


def run(cmd, cwd=None):
    return subprocess.run(cmd, shell=True, capture_output=True, text=True, cwd=cwd)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("source", help=".c file, relative to the repo root")
    ap.add_argument("base", nargs="?",
                   help="ROM address the unit starts at. Omit to derive it: any "
                        "func_XXXXXXXX symbol carries its own address, so the "
                        "base is that address minus the symbol's offset.")
    ap.add_argument("--repo", default="../wariowareinc-main")
    ap.add_argument("--rom", default="../rom.gba")
    ap.add_argument("--tmp", default="/tmp/wwverify_file")
    args = ap.parse_args()

    repo = os.path.abspath(args.repo)
    rom = open(os.path.abspath(args.rom), "rb").read()
    os.makedirs(args.tmp, exist_ok=True)

    i_f = os.path.join(args.tmp, "a.i")
    s_f = os.path.join(args.tmp, "a.s")
    o_f = os.path.join(args.tmp, "a.o")
    b_f = os.path.join(args.tmp, "a.bin")

    r = run("cc -E -I tools/agbcc -I tools/agbcc/include -I . -iquote include "
            "-nostdinc -undef %s -o %s" % (args.source, i_f), cwd=repo)
    if r.returncode:
        sys.exit("preprocessor failed:\n" + r.stderr[:2000])

    r = run("tools/agbcc/bin/agbcc -mthumb-interwork -Wparentheses -O2 -fhex-asm "
            "%s -o %s" % (i_f, s_f), cwd=repo)
    if r.returncode:
        sys.exit("agbcc failed:\n" + (r.stderr or r.stdout)[:2000])

    with open(s_f, "a") as f:
        f.write('.text\n\t.align\t2, 0\n.section .note.GNU-stack,"",%progbits\n')

    r = run("arm-none-eabi-as -march=armv4t -o %s %s" % (o_f, s_f), cwd=repo)
    if r.returncode:
        sys.exit("assembler failed:\n" + r.stderr[:2000])

    if args.base:
        base = int(args.base, 0)
    else:
        # An address-named symbol states where it lives, so the unit's base is
        # that address minus the symbol's offset inside this object.
        base = None
        r = run("arm-none-eabi-nm %s" % o_f)
        for line in r.stdout.splitlines():
            parts = line.split()
            if len(parts) == 3 and re.fullmatch(r"func_[0-9A-Fa-f]{8}", parts[2]):
                base = int(parts[2][5:], 16) - int(parts[0], 16)
                break
        if base is None:
            sys.exit("no func_XXXXXXXX symbol to derive the base from; pass it explicitly")

    run("arm-none-eabi-objcopy -O binary --only-section=.text %s %s" % (o_f, b_f))
    built = open(b_f, "rb").read()
    orig = rom[base - ROM_BASE: base - ROM_BASE + len(built)]

    relocs = set()
    r = run("arm-none-eabi-objdump -r %s" % o_f)
    for line in r.stdout.splitlines():
        m = re.match(r"^([0-9a-f]{8})\s+R_ARM_", line)
        if m:
            o = int(m.group(1), 16)
            relocs.update(range(o, o + 4))

    hard = [i for i in range(min(len(built), len(orig)))
            if built[i] != orig[i] and i not in relocs]
    soft = [i for i in range(min(len(built), len(orig)))
            if built[i] != orig[i] and i in relocs]

    print("unit        : %s" % args.source)
    print("base        : 0x%08X" % base)
    print(".text size  : %d B" % len(built))
    print("relocations : %d byte positions" % len(relocs))
    print("mismatches  : %d real, %d at relocation sites" % (len(hard), len(soft)))

    if hard:
        print("\nNO MATCH")
        for i in hard[:12]:
            print("  +0x%04X (ROM 0x%08X): built %02X, rom %02X"
                  % (i, base + i, built[i], orig[i]))
        if len(hard) > 12:
            print("  ... %d more" % (len(hard) - 12))
        return 1

    print("\nMATCH -- every function in this unit reproduces the ROM")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

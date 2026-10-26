#!/usr/bin/env python3
"""
Carve a new translation unit out of the loose assembly.

The disassembly leaves every not-yet-assigned function as its own file under
asm/, and the linker script names each one in ROM order.  A unit is created by
moving a contiguous run of those files into a subdirectory -- where the build's
`asm/*.s` glob no longer sees them -- writing a .c that includes them in order,
and collapsing the corresponding linker-script lines into that one object.

From there the file behaves like any other unit: functions get replaced by C one
at a time, and tools/verify_file.py checks the whole unit against the ROM.
"""
import argparse
import os
import re
import shutil
import sys

# Lines that only make sense when the file is assembled on its own.
STANDALONE_ONLY = ('.section', '.thumb\n', '.arm\n', '.syntax', '.include', '.end')


def wrap_for_c(path):
    """Turn a standalone .s into the asm("...") form a .c can include.

    The loose files are written to be assembled directly; the ones already
    belonging to a unit are C string literals instead.  Same instructions,
    different packaging.
    """
    body = []
    lines = open(path, errors='replace').read().splitlines()
    i = 0
    while i < len(lines):
        line = lines[i].rstrip()
        stripped = line.strip()
        if any(stripped.startswith(k.rstrip('\n')) and
               (k.endswith('\n') is False or stripped == k.strip())
               for k in STANDALONE_ONLY):
            i += 1
            continue
        if stripped.startswith('glabel '):
            name = stripped.split(None, 1)[1]
            # the .thumb_func / .arm_func directive on the next line picks the
            # macro the unit files use
            kind = 'thumb_func_start'
            if i + 1 < len(lines) and lines[i + 1].strip() == '.arm_func':
                kind = 'arm_func_start'
            if i + 1 < len(lines) and lines[i + 1].strip() in ('.thumb_func', '.arm_func'):
                i += 1
            body.append('%s %s' % (kind, name))
            i += 1
            continue
        body.append(line if stripped else '')
        i += 1

    while body and not body[-1].strip():
        body.pop()

    esc = [b.replace('\\', '\\\\').replace('"', '\\"') for b in body]
    out = ['asm(".syntax unified \\n\\']
    for b in esc:
        out.append('%s \\n\\' % (b if b.strip() else ' '))
    out.append('.syntax divided");')
    open(path, 'w').write('\n'.join(out) + '\n')

REPO = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..',
                    'wariowareinc-main')
LD = os.path.join(REPO, 'wariowareinc.ld')
LINE = re.compile(r'^\s*build/asm/(asm_([0-9a-f]{8})\.s)\.o\(\.text\*?\);\s*$')


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('start', help='first address, e.g. 0x08002620')
    ap.add_argument('end', help='first address NOT in the unit')
    ap.add_argument('name', help='unit name, e.g. code_08002620')
    ap.add_argument('--dry-run', action='store_true')
    args = ap.parse_args()

    start, end = int(args.start, 16), int(args.end, 16)
    unit_c = os.path.join(REPO, 'src', args.name + '.c')
    unit_dir = os.path.join(REPO, 'asm', args.name)

    if os.path.exists(unit_c):
        sys.exit('%s existiert bereits' % unit_c)

    ld_lines = open(LD).read().splitlines(keepends=True)
    taken, out, inserted = [], [], False

    for line in ld_lines:
        m = LINE.match(line)
        if m and start <= int(m.group(2), 16) < end:
            taken.append(m.group(1))
            if not inserted:
                out.append('        build/src/%s.c.o(.text);\n' % args.name)
                inserted = True
            continue
        out.append(line)

    if not taken:
        sys.exit('keine asm-Objekte in [%s, %s)' % (args.start, args.end))

    print('%d Funktionen: %s .. %s' % (len(taken), taken[0], taken[-1]))
    if args.dry_run:
        return

    os.makedirs(unit_dir, exist_ok=True)
    for f in taken:
        src = os.path.join(REPO, 'asm', f)
        if os.path.exists(src):
            dst = os.path.join(unit_dir, f)
            shutil.move(src, dst)
            wrap_for_c(dst)

    with open(unit_c, 'w') as fh:
        # a unit inside a subdirectory is included by its full path, as the
        # existing scene units are
        fh.write('#include "src/%s.h"\n\n' % args.name)
        fh.write('asm(".include \\"include/gba.inc\\"");\n\n')
        for f in taken:
            fh.write('#include "asm/%s/%s"\n\n' % (args.name, f))

    header = os.path.join(REPO, 'src', args.name + '.h')
    if not os.path.exists(header):
        with open(header, 'w') as fh:
            fh.write('#pragma once\n\n#include "global.h"\n')

    open(LD, 'w').write(''.join(out))
    print('angelegt: src/%s.c, src/%s.h, asm/%s/' % (args.name, args.name, args.name))
    print('Linker-Skript: %d Zeilen zu einer zusammengefasst' % len(taken))


if __name__ == '__main__':
    main()

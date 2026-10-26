#!/usr/bin/env python3
"""Print field offsets of a struct as the GBA compiler lays it out.

The host compiler is no help here: its pointers are eight bytes wide, so every
offset past the first pointer would be wrong.  This asks agbcc instead, by
compiling a probe whose initialisers are the offsets and reading them back out
of the generated assembly.
"""

import argparse
import os
import re
import subprocess
import sys

REPO = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..",
                    "wariowareinc-main")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("header", help='header to include, e.g. "types.h"')
    ap.add_argument("struct", help="struct tag, e.g. BeatscriptScene")
    ap.add_argument("fields", nargs="+", help="field names; 'sizeof' asks for the size")
    ap.add_argument("--repo", default=REPO)
    args = ap.parse_args()

    repo = os.path.abspath(args.repo)
    tmp = "/tmp/wwoffsets"
    os.makedirs(tmp, exist_ok=True)

    lines = ['#include "global.h"', '#include "%s"' % args.header,
             "const unsigned long probe[] = {"]
    for f in args.fields:
        if f == "sizeof":
            lines.append("    sizeof(struct %s)," % args.struct)
        else:
            lines.append("    (unsigned long)&((struct %s *)0)->%s,"
                         % (args.struct, f))
    lines.append("};")
    src = os.path.join(tmp, "probe.c")
    open(src, "w").write("\n".join(lines) + "\n")

    i_f, s_f = os.path.join(tmp, "probe.i"), os.path.join(tmp, "probe.s")
    r = subprocess.run("cc -E -I tools/agbcc -I tools/agbcc/include -I . -iquote include "
                       "-nostdinc -undef %s -o %s" % (src, i_f),
                       shell=True, cwd=repo, capture_output=True, text=True)
    if r.returncode:
        sys.exit("preprocessor failed:\n" + r.stderr[:2000])
    r = subprocess.run("tools/agbcc/bin/agbcc -mthumb-interwork -O2 %s -o %s" % (i_f, s_f),
                       shell=True, cwd=repo, capture_output=True, text=True)
    if r.returncode:
        sys.exit("agbcc failed:\n" + (r.stderr or r.stdout)[:2000])

    words = []
    for line in open(s_f):
        m = re.match(r"\s*\.(?:word|long)\s+(\d+)\s*$", line)
        if m:
            words.append(int(m.group(1)))
    if len(words) != len(args.fields):
        sys.exit("probe produced %d words for %d fields:\n%s"
                 % (len(words), len(args.fields), open(s_f).read()[:2000]))
    for name, off in zip(args.fields, words):
        print("  %-32s 0x%04X  (%d)" % (name, off, off))


if __name__ == "__main__":
    main()

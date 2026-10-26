#!/usr/bin/env python3
"""Emit a C struct declaration from the observed accesses.

Everything not touched by any instruction becomes an explicit pad, so the
declaration stays offset-faithful even where the layout is still unknown.
Offsets that carry sub-byte fields are emitted as bitfield runs.
"""
import sys
from collections import defaultdict
from struct_report import analyse, WIDTH_NAME, SIGNED_NAME
from bitfield_report import fields_at


def build(symbol, roots, name, size=None):
    info, hits = analyse(symbol, roots)
    bits = fields_at(symbol, roots)
    lines, pos = [], 0
    offs = sorted(info)

    for off in offs:
        if off < pos:
            continue                       # already covered by a wider member
        if off > pos:
            lines.append(('    u8 pad%X[0x%X];' % (pos, off - pos), pos))
            pos = off
        e = info[off]
        w = max(e['w'])
        sub = sorted(f for f in bits.get(off, ()) if f[1] < 8 * w)
        if sub and w == 1:
            used = 0
            for lo, width, sg in sub:
                if used > lo:
                    continue
                if lo > used:
                    lines.append(('    u32 unk%X_%d : %d;' % (off, used + 1, lo - used), off))
                lines.append(('    u32 unk%X_%d : %d;' % (off, lo + 1, width), off))
                used = lo + width
            if used < 8:
                lines.append(('    u32 unk%X_%d : %d;' % (off, used + 1, 8 - used), off))
            pos = off + 1
            continue
        t = (SIGNED_NAME if e['signed'] else WIDTH_NAME)[w]
        lines.append(('    %s unk%X;' % (t, off), off))
        pos = off + w

    if size and size > pos:
        lines.append(('    u8 pad%X[0x%X];' % (pos, size - pos), pos))
        pos = size

    out = ['struct %s {' % name]
    for text, off in lines:
        out.append('%-34s // 0x%03X' % (text, off))
    out.append('};')
    return '\n'.join(out), pos


if __name__ == '__main__':
    symbol, name = sys.argv[1], sys.argv[2]
    size = int(sys.argv[3], 0) if len(sys.argv) > 3 and sys.argv[3] != '-' else None
    roots = sys.argv[4:]
    text, end = build(symbol, roots, name, size)
    print(text)
    print('\n/* covered through 0x%X */' % end)

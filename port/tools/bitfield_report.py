#!/usr/bin/env python3
"""
Recover bitfield positions and widths from the code that touches them.

Three idioms carry the information:

  read   LDRB/LDRH v ; LSLS #a ; LSRS #b   ->  field at bit (b-a), width (32-b)
         (ASRS instead of LSRS marks a signed field)
  test   LDRB v ; LSLS #a ; CMP #0 ; BLT   ->  1-bit field at bit (31-a)
  write  LDRB v ; MOVS #k ; RSBS #0 ; ANDS ->  clear-mask ~(k-1); the bits set
                                               in (k-1) are the field

The clear-mask is the load-bearing one: it is emitted with RSBS because ~mask
computed in int precision does not fit a Thumb immediate, and it appears only
for a genuine bitfield assignment.
"""
import os, re, sys
from collections import defaultdict
from scan_struct import parse_file, scan, collect, imm, ASM_ROOT


def fields_at(symbol, roots):
    files = collect(roots)
    hits = scan(symbol, files)
    idx = {}
    for path in files:
        insns, _ = parse_file(path)
        pos = {a.upper(): i for i, (a, _, _) in enumerate(insns) if a}
        for a, i in pos.items():
            idx[a] = (insns, i)

    found = defaultdict(set)
    for func, addr, off, width, signed, kind, ops in hits:
        ctx = idx.get((addr or '').upper())
        if not ctx:
            continue
        insns, i = ctx
        after = [(m, o) for _, m, o in insns[i + 1:i + 6]]
        before = [(m, o) for _, m, o in insns[max(0, i - 5):i]]

        if kind == 'r':
            for j, (m, o) in enumerate(after):
                p = [x.strip() for x in o.split(',')]
                if m == 'LSLS' and len(p) == 3 and imm(p[2]) is not None:
                    a = imm(p[2])
                    for m2, o2 in after[j + 1:j + 3]:
                        p2 = [x.strip() for x in o2.split(',')]
                        if m2 in ('LSRS', 'ASRS') and len(p2) == 3 and imm(p2[2]) is not None:
                            b = imm(p2[2])
                            if b >= a:
                                found[off].add((b - a, 32 - b, m2 == 'ASRS'))
                            break
                        if m2 == 'CMP' and imm(p2[-1]) == 0:
                            found[off].add((31 - a, 1, False))
                            break
                    break
                if m in ('ANDS', 'AND') and len(p) == 2:
                    for m3, o3 in before:
                        p3 = [x.strip() for x in o3.split(',')]
                        if m3 == 'MOVS' and len(p3) == 2 and imm(p3[1]) is not None:
                            v = imm(p3[1])
                            if v and (v & (v + 1)) == 0:
                                found[off].add((0, v.bit_length(), False))
                    break
        else:
            for m, o in before:
                p = [x.strip() for x in o.split(',')]
                if m == 'RSBS' and len(p) == 3 and imm(p[2]) == 0:
                    for m2, o2 in before:
                        p2 = [x.strip() for x in o2.split(',')]
                        if m2 == 'MOVS' and len(p2) == 2 and imm(p2[1]) is not None:
                            k = imm(p2[1]) - 1
                            if k > 0:
                                lo = (k & -k).bit_length() - 1
                                hi = k.bit_length() - 1
                                if k == (((1 << (hi - lo + 1)) - 1) << lo):
                                    found[off].add((lo, hi - lo + 1, False))
                    break
    return found


if __name__ == '__main__':
    symbol = sys.argv[1] if len(sys.argv) > 1 else 'gCurrentSceneData'
    found = fields_at(symbol, sys.argv[2:] or ['asm'])
    print('%-8s %s' % ('BYTE', 'FIELDS (bit, width) within that byte'))
    for off in sorted(found):
        fs = sorted(found[off])
        print('0x%-6X %s' % (off, '  '.join(
            '%d:%d%s' % (lo, w, 's' if sg else '') for lo, w, sg in fs)))

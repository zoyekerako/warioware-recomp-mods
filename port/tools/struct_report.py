#!/usr/bin/env python3
"""
Turn raw pointer-relative accesses into a struct layout proposal.

Two things have to be recovered that the offsets alone do not carry:

  * Bitfield units.  A `u32 flags:1` at bit 26 compiles to an LDRB/STRB on the
    fourth byte of the unit, so consecutive byte accesses that carry masks are
    the fingerprint of one wider unit, not of four separate u8 members.
  * Element size of arrays.  An access whose offset is register-computed is
    reported at its base; the stride shows up as the shift feeding the index.

For every offset we therefore summarise the widths seen, whether any access
sign-extends (which forces a signed member), and the masks/shifts applied.
"""
import os, re, sys, json
from collections import defaultdict
from scan_struct import parse_file, scan, collect, imm, ASM_ROOT

WIDTH_NAME = {1: 'u8', 2: 'u16', 4: 'u32'}
SIGNED_NAME = {1: 's8', 2: 's16', 4: 's32'}


def context_index(path):
    """Map instruction address -> (index, instruction list) for context lookup."""
    insns, _ = parse_file(path)
    idx = {}
    for i, (addr, mn, ops) in enumerate(insns):
        if addr:
            idx[addr.upper()] = i
    return idx, insns


def bit_span(mask, width):
    """Bit range covered by a clear-mask, as (lo, count), or None."""
    full = (1 << (width * 8)) - 1
    cleared = (~mask) & full
    if cleared == 0 or cleared == full:
        return None
    lo = (cleared & -cleared).bit_length() - 1
    hi = cleared.bit_length() - 1
    if cleared != (((1 << (hi - lo + 1)) - 1) << lo):
        return None                       # non-contiguous: not a single field
    return lo, hi - lo + 1


def analyse(symbol, roots):
    files = collect(roots)
    hits = scan(symbol, files)
    by_file = defaultdict(list)
    for h in hits:
        by_file[h[0]].append(h)

    # rebuild an address -> file map so context can be fetched
    addr_map = {}
    for path in files:
        idx, insns = context_index(path)
        for a, i in idx.items():
            addr_map[a] = (insns, i)

    info = defaultdict(lambda: {'w': set(), 'signed': False, 'r': 0, 'wr': 0,
                                'funcs': set(), 'bits': set(), 'masks': set()})
    for func, addr, off, width, signed, kind, ops in hits:
        e = info[off]
        e['w'].add(width)
        e['signed'] |= signed
        e['funcs'].add(func)
        e['r' if kind == 'r' else 'wr'] += 1

        ctx = addr_map.get((addr or '').upper())
        if not ctx:
            continue
        insns, i = ctx
        window = insns[max(0, i - 6): i + 7]
        for a2, mn2, ops2 in window:
            o2 = [x.strip() for x in ops2.split(',')] if ops2 else []
            if mn2 == 'ANDS' and len(o2) == 2:
                pass
            if mn2 in ('MOVS',) and len(o2) == 2:
                v = imm(o2[1])
                if v is not None and 0 < v < 256:
                    e['masks'].add(v)
            if mn2 == 'LDR' and len(o2) >= 2 and '=' in ops2:
                v = imm(ops2.split('=')[1].strip())
                if v is not None:
                    e['masks'].add(v)
            if mn2 in ('LSLS', 'ASRS', 'LSRS') and len(o2) == 3:
                v = imm(o2[2])
                if v is not None:
                    e['bits'].add((mn2, v))
    return info, hits


def render(info):
    out = []
    for off in sorted(info):
        e = info[off]
        widths = sorted(e['w'])
        w = max(widths)
        name = (SIGNED_NAME if e['signed'] else WIDTH_NAME)[w]
        note = []
        if len(widths) > 1:
            note.append('widths=' + '/'.join(str(x) for x in widths))
        if e['signed']:
            note.append('sign-extended')
        note.append('r%d w%d' % (e['r'], e['wr']))
        note.append('%d fn' % len(e['funcs']))
        out.append((off, w, name, '; '.join(note), sorted(e['funcs'])[:4]))
    return out


if __name__ == '__main__':
    symbol = sys.argv[1] if len(sys.argv) > 1 else 'gCurrentSceneData'
    roots = sys.argv[2:] or ['asm']
    info, hits = analyse(symbol, roots)
    rows = render(info)
    print('%-8s %-6s %s' % ('OFFSET', 'TYPE', 'EVIDENCE'))
    prev_end = None
    for off, w, name, note, funcs in rows:
        if prev_end is not None and off > prev_end:
            print('%-8s %-6s   << %d byte gap >>' % ('', '', off - prev_end))
        print('0x%-6X %-6s %s   [%s]' % (off, name, note, ', '.join(funcs)))
        prev_end = off + w
    print('\ntotal offsets: %d   total accesses: %d' % (len(rows), len(hits)))

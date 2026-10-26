#!/usr/bin/env python3
"""
Same dataflow as scan_struct, but seeded from a parameter register instead of a
global.  Structs that are only ever reached through a function argument are
invisible to the global scan, yet they are most of what a scene is made of.

Usage: scan_param.py <reg> <asm-file> [<asm-file> ...]
"""
import os, re, sys, json
from collections import defaultdict
from scan_struct import (parse_file, imm, const_of, collect, LOADS, STORES,
                         MEMOP, REG, ASM_ROOT)


def scan(seed_reg, files):
    hits = []
    for path in files:
        insns, pool = parse_file(path)
        func, state = os.path.basename(path), {}
        for addr, mn, ops in insns:
            if mn == '.func':
                func, state = ops, {seed_reg: ('base', 0)}
                continue
            if mn == '.label':
                continue
            o = [x.strip().upper() for x in ops.split(',')] if ops else []
            d = o[0] if o else None

            if mn == 'LDR' and len(o) >= 2:
                src = ops.split(',', 1)[1].strip()
                if src.startswith('=') or src.startswith('_'):
                    w = src[1:] if src.startswith('=') else (pool.get(src.upper()) or pool.get(src))
                    v = imm(w) if w else None
                    state[d] = ('const', v) if v is not None else None
                    continue
            if mn in LOADS or mn in STORES:
                m = MEMOP.match(ops)
                if m:
                    base, off = m.group(2).upper(), m.group(3)
                    off = 0 if off is None else const_of(state, off)
                    st = state.get(base)
                    if st and st[0] == 'base' and off is not None:
                        w, sg = LOADS.get(mn, (STORES.get(mn), False))
                        hits.append((func, addr, st[1] + off, w, sg,
                                     'r' if mn in LOADS else 'w', ops))
                if mn in LOADS:
                    state[d] = None
                continue
            if mn in ('ADDS', 'ADD', 'SUBS', 'SUB') and len(o) == 3:
                st, v = state.get(o[1]), const_of(state, o[2])
                sign = -1 if mn.startswith('SUB') else 1
                if st and st[0] in ('base', 'const') and v is not None:
                    state[d] = (st[0], st[1] + sign * v)
                else:
                    other = state.get(o[2])
                    if other and other[0] == 'base' and st and st[0] == 'const':
                        state[d] = ('base', other[1] + sign * st[1])
                    elif st and st[0] == 'base':
                        state[d] = ('base', st[1])
                    else:
                        state[d] = None
                continue
            if mn in ('ADDS', 'ADD') and len(o) == 2:
                st, v = state.get(d), const_of(state, o[1])
                state[d] = (st[0], st[1] + v) if st and v is not None and st[0] in ('base', 'const') else None
                continue
            if mn == 'MOVS' and len(o) == 2:
                v = imm(o[1])
                state[d] = ('const', v) if v is not None else state.get(o[1])
                continue
            if mn == 'MOV' and len(o) == 2:
                state[d] = state.get(o[1])
                continue
            if mn == 'LSLS' and len(o) == 3:
                st, v = state.get(o[1]), imm(o[2])
                state[d] = ('const', st[1] << v) if st and st[0] == 'const' and v is not None else None
                continue
            if mn in ('BL', 'BLX'):
                for r in ('R0', 'R1', 'R2', 'R3', 'R12', 'IP', 'LR'):
                    state.pop(r, None)
                continue
            if d and REG.match(d) and mn not in ('CMP', 'CMN', 'TST', 'PUSH', 'POP', 'B', 'BX'):
                state[d] = None
    return hits


if __name__ == '__main__':
    reg = sys.argv[1].upper()
    print(json.dumps(scan(reg, collect(sys.argv[2:]))))

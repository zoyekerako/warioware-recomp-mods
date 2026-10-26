#!/usr/bin/env python3
"""
Scan the disassembly for every access made through a given base pointer and
tabulate them into a struct-shaped report.

The compiler narrows a bitfield access to the smallest addressable unit that
contains the field, so a `u32 x:1` at bit 26 surfaces as an LDRB/STRB on byte 3
of the unit.  Raw offsets therefore do not reveal the unit boundary -- the mask
constants do.  We record, for every access, the width, the signedness, and the
mask/shift it is combined with, so bitfield units can be recovered afterwards.

Offsets above 0xFF cannot be encoded in a Thumb load, so the compiler
materialises them in a register (`MOVS Rn,#0xAB; LSLS Rn,#1`).  Tracking
register constants alongside the pointer is what makes those accesses visible.
"""
import os, re, sys, json
from collections import defaultdict

ASM_ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..',
                        'wariowareinc-main')

INSN = re.compile(r'/\*\s*([0-9A-Fa-f]{8})\s*\*/\s*(.*?)\s*(?:\\n\\)?$')
FUNC = re.compile(r'thumb_func_start\s+(\S+)|arm_func_start\s+(\S+)')
POOL = re.compile(r'^_([0-9A-Fa-f]{8}):')
WORD = re.compile(r'\.word\s+(\S+)')
REG  = re.compile(r'^(R\d+|SP|LR|PC)$', re.I)

LOADS  = {'LDR': (4, False), 'LDRB': (1, False), 'LDRH': (2, False),
          'LDRSB': (1, True), 'LDRSH': (2, True)}
STORES = {'STR': 4, 'STRB': 1, 'STRH': 2}
MEMOP  = re.compile(r'^(R\d+|SP|LR),\s*\[(R\d+|SP|PC)(?:,\s*(\S+?))?\]', re.I)


def parse_file(path):
    insns, pool, cur = [], {}, None
    for raw in open(path, encoding='utf-8', errors='replace'):
        line = re.sub(r'\\n\\$', '', raw.rstrip('\n')).strip()
        if not line:
            continue
        m = FUNC.search(line)
        if m:
            insns.append((None, '.func', m.group(1) or m.group(2)))
            continue
        m = POOL.match(line)
        if m:
            cur = '_' + m.group(1)
            insns.append((None, '.label', cur))
        m = INSN.match(line)
        if not m:
            w = WORD.search(line)
            if w and cur:
                pool[cur] = w.group(1)
            continue
        addr, body = m.group(1), re.sub(r'@.*$', '', m.group(2)).strip()
        if not body:
            continue
        if body.startswith('.'):
            w = WORD.search(body)
            if w and cur:
                pool[cur] = w.group(1)
            continue
        p = body.split(None, 1)
        insns.append((addr, p[0].upper(), p[1] if len(p) > 1 else ''))
    return insns, pool


def imm(tok):
    if tok is None:
        return None
    t = tok.strip().lstrip('#')
    neg = t.startswith('-')
    t = t.lstrip('-')
    try:
        v = int(t, 16) if t.upper().startswith('0X') else int(t, 10)
    except ValueError:
        return None
    return -v if neg else v


def const_of(state, tok):
    """Immediate value of an operand, whether literal or held in a register."""
    v = imm(tok)
    if v is not None:
        return v
    s = state.get(tok.upper()) if tok else None
    return s[1] if s and s[0] == 'const' else None


def scan(symbol, files, deref=True):
    hits = []
    for path in files:
        insns, pool = parse_file(path)
        func, state = os.path.basename(path), {}
        for addr, mn, ops in insns:
            if mn == '.func':
                func, state = ops, {}
                continue
            if mn == '.label':
                continue
            o = [x.strip().upper() for x in ops.split(',')] if ops else []
            d = o[0] if o else None

            if mn == 'LDR' and len(o) >= 2:
                src = ops.split(',', 1)[1].strip()
                if src.startswith('='):
                    sym = src[1:]
                    v = imm(sym)
                    state[d] = ('const', v) if v is not None else \
                               (('addr',) if sym == symbol else None)
                    continue
                if src.startswith('_'):
                    w = pool.get(src.upper()) or pool.get(src)
                    v = imm(w) if w else None
                    state[d] = ('const', v) if v is not None else \
                               (('addr',) if w == symbol else None)
                    continue
            if mn in LOADS or mn in STORES:
                m = MEMOP.match(ops)
                if m:
                    base, off = m.group(2).upper(), m.group(3)
                    off = 0 if off is None else const_of(state, off)
                    st = state.get(base)
                    if st and st[0] == 'addr' and off == 0 and mn == 'LDR' and deref:
                        state[d] = ('base', 0)
                        continue
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
                if st and st[0] == 'base' and v is not None:
                    state[d] = ('base', st[1] + sign * v)
                elif st and st[0] == 'const' and v is not None:
                    state[d] = ('const', st[1] + sign * v)
                else:                       # base + unknown index: keep the base
                    other = state.get(o[2])
                    if other and other[0] == 'base' and st and st[0] == 'const':
                        state[d] = ('base', other[1] + sign * st[1])
                    elif st and st[0] == 'base':
                        state[d] = ('base', st[1])
                    else:
                        state[d] = None
                continue
            if mn in ('ADDS', 'ADD', 'SUBS', 'SUB') and len(o) == 2:
                st, v = state.get(d), const_of(state, o[1])
                sign = -1 if mn.startswith('SUB') else 1
                if st and v is not None and st[0] in ('base', 'const'):
                    state[d] = (st[0], st[1] + sign * v)
                else:
                    state[d] = None
                continue
            if mn == 'MOVS' and len(o) == 2:
                v = imm(o[1])
                state[d] = ('const', v) if v is not None else state.get(o[1])
                continue
            if mn in ('MOV',) and len(o) == 2:
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


def collect(roots):
    files = []
    for root in roots:
        p = os.path.join(ASM_ROOT, root)
        if os.path.isfile(p):
            files.append(p)
        else:
            for dp, _, fns in os.walk(p):
                files += [os.path.join(dp, f) for f in sorted(fns) if f.endswith('.s')]
    return files


if __name__ == '__main__':
    symbol = sys.argv[1] if len(sys.argv) > 1 else 'gCurrentSceneData'
    hits = scan(symbol, collect(sys.argv[2:] or ['asm']))
    print(json.dumps(hits))

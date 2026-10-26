#!/usr/bin/env python3
"""
List the functions whose entire body is bitfield traffic on a base pointer.

`MOVS Rn,#k; RSBS Rn,Rn,#0` is the compiler materialising ~(k-1) -- a clear-mask
too wide for a Thumb immediate.  It only ever appears for a bitfield write, so
it is a reliable marker.  Reads are marked by an isolated shift pair
(`LSLS #a; LSRS #b`) or a shift-and-test (`LSLS #a; CMP #0; BGE`).
"""
import os, re, sys, json
from collections import defaultdict
from scan_struct import parse_file, collect, imm, ASM_ROOT


def classify(insns):
    """Split the stream into functions and describe each one."""
    out, cur, body = [], None, []
    for addr, mn, ops in insns:
        if mn == '.func':
            if cur:
                out.append((cur, body))
            cur, body = ops, []
            continue
        if mn == '.label':
            continue
        if cur:
            body.append((addr, mn, ops))
    if cur:
        out.append((cur, body))
    return out


def describe(body):
    n = len(body)
    text = ' | '.join('%s %s' % (m, o) for _, m, o in body)
    has_rsbs = bool(re.search(r'RSBS R\d+, R\d+, #0', text))
    calls = len(re.findall(r'\bBL\b', text))
    hi = bool(re.search(r'\bR(8|9|10|11)\b|\bMOV R\d+, R(8|9|10|11)\b', text))
    branches = len(re.findall(r'\bB(?:EQ|NE|GE|LT|GT|LE|HI|LS|HS|LO|CS|CC|MI|PL)?\s+_', text))
    return dict(n=n, rsbs=has_rsbs, calls=calls, hi=hi, branches=branches)


def still_open():
    """Paths of asm files a .c actually still includes -- the real backlog."""
    src = os.path.join(ASM_ROOT, 'src')
    open_set = set()
    for dp, _, fns in os.walk(src):
        for f in fns:
            if not f.endswith(('.c', '.h')):
                continue
            for m in re.finditer(r'#include\s+"(asm/[^"]+\.s)"',
                                 open(os.path.join(dp, f), errors='replace').read()):
                open_set.add(os.path.normpath(os.path.join(ASM_ROOT, m.group(1))))
    return open_set


if __name__ == '__main__':
    roots = sys.argv[1:] or ['asm']
    rows = []
    backlog = still_open()
    for path in collect(roots):
        if os.path.normpath(path) not in backlog:
            continue
        insns, _ = parse_file(path)
        for name, body in classify(insns):
            if not body:
                continue
            d = describe(body)
            rows.append((name, path, d))
    rows.sort(key=lambda r: r[2]['n'])
    only_bf = '--all' not in sys.argv
    if not only_bf:
        roots = [a for a in roots if not a.startswith('--')]
    bf = [r for r in rows if r[2]['rsbs'] or not only_bf]
    print('functions still in asm : %d' % len(rows))
    print('with bitfield writes   : %d' % len(bf))
    print('  of those, <=20 insns : %d' % len([r for r in bf if r[2]['n'] <= 20]))
    print()
    print('%-22s %-4s %-5s %-4s %-4s %s' % ('FUNCTION', 'INS', 'CALLS', 'BRA', 'HI', 'FILE'))
    for name, path, d in bf:
        if d['n'] <= int(os.environ.get('MAXINS', '24')):
            print('%-22s %-4d %-5d %-4d %-4s %s' % (
                name, d['n'], d['calls'], d['branches'],
                'yes' if d['hi'] else '', os.path.relpath(path, ASM_ROOT)))

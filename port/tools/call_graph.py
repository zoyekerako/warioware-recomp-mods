#!/usr/bin/env python3
"""
Transitive closure of the call graph, over both the assembly and the C.

Before decompiling towards a goal it is worth knowing how big the goal is.
Starting from one function, this reports everything reachable from it, split
into what is already C and what is still assembly -- which is the actual size
of the job.
"""
import os, re, sys
from collections import deque

REPO = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..',
                    'wariowareinc-main')
LABEL = re.compile(r'(?:thumb_func_start|arm_func_start)\s+(\S+)|glabel\s+(\S+)')
CALL_ASM = re.compile(r'\bBL\s+([A-Za-z_]\w*)')
CALL_C = re.compile(r'\b([A-Za-z_]\w*)\s*\(')
DEFINE_C = re.compile(r'^\s*(?:static\s+)?[A-Za-z_]\w*[\w \t\*]*?\**\s*([A-Za-z_]\w*)\s*\([^;{]*\)\s*\{',
                      re.M)


def index():
    """name -> ('asm'|'c', set of callees)"""
    out = {}
    for dp, _, fns in os.walk(os.path.join(REPO, 'asm')):
        for f in sorted(fns):
            if not f.endswith('.s'):
                continue
            txt = open(os.path.join(dp, f), errors='replace').read()
            cur = None
            for line in txt.splitlines():
                m = LABEL.search(line)
                if m:
                    cur = m.group(1) or m.group(2)
                    out.setdefault(cur, ['asm', set()])
                    continue
                if cur:
                    c = CALL_ASM.search(line)
                    if c:
                        out[cur][1].add(c.group(1))

    for dp, _, fns in os.walk(os.path.join(REPO, 'src')):
        for f in sorted(fns):
            if not f.endswith('.c'):
                continue
            txt = open(os.path.join(dp, f), errors='replace').read()
            bodies = list(DEFINE_C.finditer(txt))
            for i, m in enumerate(bodies):
                name = m.group(1)
                end = bodies[i + 1].start() if i + 1 < len(bodies) else len(txt)
                body = txt[m.end():end]
                out[name] = ['c', set(CALL_C.findall(body))]
    return out


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else 'func_080001D4'
    g = index()
    seen, q = set(), deque([root])
    while q:
        n = q.popleft()
        if n in seen or n not in g:
            continue
        seen.add(n)
        q.extend(g[n][1])

    asm = sorted(n for n in seen if g[n][0] == 'asm')
    c = sorted(n for n in seen if g[n][0] == 'c')
    print('von %s aus erreichbar: %d Funktionen' % (root, len(seen)))
    print('  bereits C : %d' % len(c))
    print('  noch asm  : %d' % len(asm))
    if '-v' in sys.argv:
        print('\n'.join('  ' + n for n in asm))


if __name__ == '__main__':
    main()

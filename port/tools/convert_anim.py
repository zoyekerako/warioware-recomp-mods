#!/usr/bin/env python3
"""Convert extracted animation data into a flat binary the port can load.

The decomp stores animations as two C files: an array of AnimationCel blobs
(u16: an entry count followed by that many OAM attribute triples) and a table of
`struct Animation { AnimationCel *cel; u8 duration; }` runs terminated by a NULL
cel. Rather than compile those against the decomp's headers, parse them into a
self-contained blob:

    "WWAN"                        4
    version = 1                   4
    celCount, animCount           8
    celDataHalfwords              4
    celOffset[celCount]           4 each   (halfword index into celData)
    animStart[animCount + 1]      4 each   (index into frames)
    frames: {celIndex, duration}  8 each
    celData[celDataHalfwords]     2 each
    names: animCount NUL-terminated strings
"""

import argparse
import re
import struct


def parse_cels(path):
    text = open(path).read()
    cels = {}
    order = []
    for m in re.finditer(r"AnimationCel\s+(\w+)\s*\[\s*\]\s*=\s*\{(.*?)\};", text, re.S):
        name, body = m.group(1), m.group(2)
        body = re.sub(r"/\*.*?\*/", " ", body, flags=re.S)
        values = [int(v, 0) for v in re.findall(r"0x[0-9A-Fa-f]+|\b\d+\b", body)]
        cels[name] = values
        order.append(name)
    return cels, order


def parse_anims(path):
    text = open(path).read()
    anims = []
    for m in re.finditer(r"struct\s+Animation\s+(\w+)\s*\[\s*\]\s*=\s*\{(.*?)\};", text, re.S):
        name, body = m.group(1), m.group(2)
        body = re.sub(r"/\*.*?\*/", " ", body, flags=re.S)
        frames = []
        for f in re.finditer(r"\{\s*(\w+)\s*,\s*(\d+)\s*\}", body):
            frames.append((f.group(1), int(f.group(2))))
        # END_ANIMATION expands to {NULL, 0} and is the terminator
        frames = [(c, d) for c, d in frames if c != "NULL"]
        if frames:
            anims.append((name, frames))
    return anims


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("cells", help="e.g. graphics/title/title_anim_cells.inc.c")
    ap.add_argument("anims", help="e.g. graphics/title/title_anim.c")
    ap.add_argument("-o", "--out", default="assets/title_anim.bin")
    args = ap.parse_args()

    cels, order = parse_cels(args.cells)
    anims = parse_anims(args.anims)

    index_of = {name: i for i, name in enumerate(order)}
    cel_data = []
    cel_offset = []
    for name in order:
        cel_offset.append(len(cel_data))
        cel_data.extend(cels[name])

    frames = []
    anim_start = []
    for name, fr in anims:
        anim_start.append(len(frames))
        for cel_name, duration in fr:
            if cel_name not in index_of:
                raise SystemExit("unknown cel %s in %s" % (cel_name, name))
            frames.append((index_of[cel_name], duration))
    anim_start.append(len(frames))

    out = bytearray()
    out += b"WWAN" + struct.pack("<I", 1)
    out += struct.pack("<III", len(order), len(anims), len(cel_data))
    out += struct.pack("<%dI" % len(cel_offset), *cel_offset)
    out += struct.pack("<%dI" % len(anim_start), *anim_start)
    for ci, du in frames:
        out += struct.pack("<II", ci, du)
    out += struct.pack("<%dH" % len(cel_data), *cel_data)
    for name, _ in anims:
        out += name.encode() + b"\0"

    open(args.out, "wb").write(out)
    print("%d cels, %d animations, %d frames, %d halfwords of cel data -> %s"
          % (len(order), len(anims), len(frames), len(cel_data), args.out))
    for i, (name, fr) in enumerate(anims):
        total = sum(d for _, d in fr)
        print("  %2d  %-34s %2d frames, %3d ticks" % (i, name, len(fr), total))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

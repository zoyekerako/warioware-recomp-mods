#!/usr/bin/env python3
"""Bulk-extract every CompressedData graphics asset from a WarioWare Inc. ROM.

The game stores tilesets and tilemaps behind a bespoke two-stage codec:

  stage A (outer, always)   halfword RLE with an out-of-band u8 count stream.
                            A count byte of 0 means "255 more of the same
                            segment kind" and does NOT toggle the segment type.
                            Repeat runs replicate the previously emitted
                            halfword, so there is no explicit value byte.

  stage B (inner, optional) nibble-dictionary bitplane coder steered by two
                            separate 32-bit bitmask streams. window1 picks
                            literal vs. coded, window2 picks between a 1-bit
                            and a 2-bit dictionary mode.

Descriptors on ROM (16 bytes each):

  CompressedData { const void *data; const u8 *rleData;
                   u16 rleSize; u16 rleOffset; u32 doubleCompressed; }
  CompressedGFX  { const u16 *data; u16 size; u16 count;
                   const u32 *window1; const u32 *window2; }

This tool scans the whole ROM for valid CompressedData records, decodes each
one and writes it out alongside a JSON manifest.
"""

import argparse
import json
import os
import struct
import sys

ROM_BASE = 0x08000000
ROM_END = 0x08800000
MAX_ASSET_BYTES = 128 * 1024


def rom_off(addr):
    return addr - ROM_BASE


def in_rom(addr, size=1):
    return ROM_BASE <= addr and addr + size <= ROM_END


def decode_inner(rom, addr):
    """Stage B: nibble-dictionary bitplane coder. Returns a list of halfwords."""
    data_addr, size, count, w1_addr, w2_addr = struct.unpack_from("<IHHII", rom, rom_off(addr))

    for a in (data_addr, w1_addr, w2_addr):
        if not in_rom(a, 4):
            raise ValueError("inner: pointer outside ROM")
    if size == 0 or size > MAX_ASSET_BYTES // 2:
        raise ValueError("inner: implausible size")

    w1 = rom_off(w1_addr)
    w2 = rom_off(w2_addr)
    src = rom_off(data_addr)
    out = []
    w2_index = 0

    def window_bit(base, index):
        word = struct.unpack_from("<I", rom, base + ((index >> 5) << 2))[0]
        return (word >> (index & 31)) & 1

    for segment in range(count):
        if window_bit(w1, segment) == 0:
            out.append(struct.unpack_from("<H", rom, src)[0])
            src += 2
            continue

        if window_bit(w2, w2_index) == 0:
            # mode 2: two-entry dictionary, one bit per output nibble
            w2_index += 1
            header = struct.unpack_from("<H", rom, src)[0]
            src += 2
            dictionary = (header & 0xF, (header >> 4) & 0xF)
            for _ in range((header >> 8) + 1):
                encoded = struct.unpack_from("<H", rom, src)[0]
                src += 2
                hwords = [0, 0, 0, 0]
                for nib in range(16):
                    hwords[nib >> 2] |= dictionary[(encoded >> nib) & 1] << ((nib & 3) << 2)
                out.extend(hwords)
            continue

        # mode 1: four-entry dictionary, two bits per output nibble
        w2_index += 1
        dict_header, metadata = struct.unpack_from("<HH", rom, src)
        src += 4
        dictionary = (
            dict_header & 0xF,
            (dict_header >> 4) & 0xF,
            (dict_header >> 8) & 0xF,
            (dict_header >> 12) & 0xF,
        )

        def expand(byte):
            value = 0
            for nib in range(4):
                value |= dictionary[(byte >> (nib << 1)) & 3] << (nib << 2)
            return value

        out.append(expand(metadata >> 8))
        for _ in range((metadata & 0xFF) + 1):
            encoded = struct.unpack_from("<H", rom, src)[0]
            src += 2
            out.append(expand(encoded & 0xFF))
            out.append(expand(encoded >> 8))

    if len(out) != size:
        raise ValueError("inner: size mismatch, expected %d got %d" % (size, len(out)))
    return out


def expand_rle_in_place(source, counts, rle_offset):
    """Stage A for double-compressed assets: source sits at the tail of the
    destination buffer and is overwritten as it is consumed."""
    start = rle_offset - len(source)
    if start < 0:
        raise ValueError("rle: source starts before the destination buffer")

    buf = [0] * rle_offset
    buf[start:rle_offset] = source
    src, dst, literal = start, 0, True

    for raw in counts:
        count = raw if raw != 0 else 0xFF
        if literal:
            if src + count > rle_offset or dst + count > rle_offset:
                raise ValueError("rle: literal run out of bounds")
            buf[dst:dst + count] = buf[src:src + count]
            src += count
            dst += count
        else:
            if dst == 0:
                raise ValueError("rle: repeat before any literal")
            if dst + count > rle_offset:
                raise ValueError("rle: repeat run out of bounds")
            buf[dst:dst + count] = [buf[dst - 1]] * count
            dst += count
        if raw != 0:
            literal = not literal
    return buf[:dst]


def expand_rle(source, counts):
    """Stage A for single-stage assets: plain expansion into a fresh buffer."""
    out = []
    src, literal = 0, True
    for raw in counts:
        count = raw if raw != 0 else 0xFF
        if literal:
            out.extend(source[src:src + count])
            src += count
        else:
            if not out:
                raise ValueError("rle: repeat before any literal")
            out.extend([out[-1]] * count)
        if raw != 0:
            literal = not literal
    return out


def decode_asset(rom, addr, strict=True):
    """Decode one CompressedData descriptor. Returns (hwords, double_compressed).

    `strict` enables the blind-scan-only tight-fit heuristic; pass False for an
    address you already know is real."""
    off = rom_off(addr)
    data_addr, rle_addr = struct.unpack_from("<II", rom, off)
    rle_size, rle_offset = struct.unpack_from("<HH", rom, off + 8)
    double = struct.unpack_from("<I", rom, off + 12)[0]

    if double not in (0, 1):
        raise ValueError("descriptor: doubleCompressed is not a flag")
    if not in_rom(data_addr, 2) or not in_rom(rle_addr, rle_size):
        raise ValueError("descriptor: pointer outside ROM")
    if rle_size == 0 or rle_offset == 0 or rle_offset > MAX_ASSET_BYTES // 2:
        raise ValueError("descriptor: implausible sizes")

    counts = list(rom[rom_off(rle_addr):rom_off(rle_addr) + rle_size])

    if double:
        source = decode_inner(rom, data_addr)
        out = expand_rle_in_place(source, counts, rle_offset)
    else:
        literal_total, literal = 0, True
        for raw in counts:
            count = raw if raw != 0 else 0xFF
            if literal:
                literal_total += count
            if raw != 0:
                literal = not literal
        if not in_rom(data_addr, literal_total * 2):
            raise ValueError("descriptor: literal block outside ROM")
        source = list(struct.unpack_from("<%dH" % literal_total, rom, rom_off(data_addr)))
        out = expand_rle(source, counts)

    # Hard structural rule: the decoded data must fit the destination buffer.
    if not out or len(out) > rle_offset:
        raise ValueError("descriptor: decoded length exceeds destination buffer")

    # Soft rule, blind scan only. Without a tight fit, random ROM bytes can
    # masquerade as a valid asset. But a tilemap may legitimately fill only part
    # of a 4 KiB screenblock window, so this must NOT gate a known-good address.
    if strict and rle_offset - len(out) > 0x400:
        raise ValueError("descriptor: buffer slack too large for a blind match")

    return out, bool(double)


def plausible_descriptor(rom, off):
    """Cheap pre-filter so we do not attempt a full decode on 2M offsets."""
    data_addr, rle_addr = struct.unpack_from("<II", rom, off)
    rle_size, rle_offset = struct.unpack_from("<HH", rom, off + 8)
    double = struct.unpack_from("<I", rom, off + 12)[0]
    return (
        double in (0, 1)
        and rle_size != 0
        and rle_offset != 0
        and rle_offset <= MAX_ASSET_BYTES // 2
        and in_rom(data_addr, 2)
        and in_rom(rle_addr, rle_size)
    )


def scan(rom, start, end, progress=True):
    """Yield (address, hwords, double) for every decodable descriptor."""
    seen = set()
    found = []
    total = (end - start) // 4

    for index, off in enumerate(range(start, end - 16, 4)):
        if progress and index % 100000 == 0:
            pct = 100.0 * index / total
            print("  scanning %5.1f%%  (%d found)" % (pct, len(found)), file=sys.stderr)

        if not plausible_descriptor(rom, off):
            continue

        key = struct.unpack_from("<II", rom, off)
        if key in seen:
            continue

        try:
            hwords, double = decode_asset(rom, ROM_BASE + off)
        except (ValueError, struct.error, IndexError):
            continue

        seen.add(key)
        found.append((ROM_BASE + off, hwords, double))

    return found


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("rom", help="path to the 8 MiB ROM")
    ap.add_argument("-o", "--outdir", default="assets/gfx", help="output directory")
    ap.add_argument("--start", type=lambda s: int(s, 0), default=ROM_BASE,
                    help="first ROM address to scan (default: 0x08000000)")
    ap.add_argument("--end", type=lambda s: int(s, 0), default=ROM_END,
                    help="last ROM address to scan (default: 0x08800000)")
    ap.add_argument("--extra", metavar="FILE",
                    help="file of hex ROM addresses (one per line) to decode "
                         "without the blind-scan tight-fit heuristic")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()

    with open(args.rom, "rb") as f:
        rom = f.read()
    if len(rom) != ROM_END - ROM_BASE:
        print("warning: ROM is %d bytes, expected %d" % (len(rom), ROM_END - ROM_BASE),
              file=sys.stderr)

    os.makedirs(args.outdir, exist_ok=True)
    assets = scan(rom, rom_off(args.start), min(rom_off(args.end), len(rom)),
                  progress=not args.quiet)

    if args.extra:
        have = {a for a, _, _ in assets}
        recovered = 0
        for line in open(args.extra):
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            addr = int(line, 16)
            if addr in have:
                continue
            try:
                hwords, double = decode_asset(rom, addr, strict=False)
            except (ValueError, struct.error, IndexError) as exc:
                print("  %s: %s" % (line, exc), file=sys.stderr)
                continue
            assets.append((addr, hwords, double))
            recovered += 1
        assets.sort(key=lambda t: t[0])
        if recovered:
            print("recovered %d additional assets from %s" % (recovered, args.extra))

    manifest = []
    total_bytes = 0
    for addr, hwords, double in assets:
        # The build rule treats double-compressed blobs as tilesets; single-stage
        # ones are either tilemaps or .rle.4bpp tilesets, which the descriptor
        # alone cannot tell apart.
        ext = "4bpp" if double else "rle.bin"
        name = "gfx_%08X.%s" % (addr, ext)
        payload = struct.pack("<%dH" % len(hwords), *hwords)
        with open(os.path.join(args.outdir, name), "wb") as f:
            f.write(payload)
        total_bytes += len(payload)
        manifest.append({
            "address": "0x%08X" % addr,
            "file": name,
            "bytes": len(payload),
            "double_compressed": double,
        })

    with open(os.path.join(args.outdir, "manifest.json"), "w") as f:
        json.dump({"rom_bytes": len(rom), "assets": manifest}, f, indent=1)

    two_stage = sum(1 for a in manifest if a["double_compressed"])
    print("extracted %d assets (%d two-stage, %d single-stage), %d bytes -> %s"
          % (len(manifest), two_stage, len(manifest) - two_stage, total_bytes, args.outdir))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

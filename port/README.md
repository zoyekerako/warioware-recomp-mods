# Native port and tools

Working directory for the port. The decompilation under `../wariowareinc-main`
stays untouched; only what the port needs lives here.

## Tools

| Tool | Purpose |
|---|---|
| `verify_file.py`, `verify_all.sh` | check one unit, or all of them, against the ROM |
| `verify_func.py` | check a single function; relocation bytes counted separately |
| `new_unit.py` | cut a translation unit out of loose assembly |
| `offsets.py` | struct offsets as the GBA compiler lays them out |
| `gen_pc_stubs.py` | stub file for everything still missing |
| `call_graph.py` | transitive closure over assembly and C calls |
| `scan_struct.py`, `scan_param.py`, `bitfield_report.py`, `gen_struct.py` | reconstruct structs from the disassembly |
| `bs2c.py` | beatscript to C, with a round-trip proof |
| `rom2c.py` | raw ROM block to C, with typed pointer tables |
| `extract_gfx.py`, `extract_gfx_tables.py`, `convert_anim.py`, `preview_4bpp.py` | graphics pipeline |

## Toolchain

`arm-none-eabi-binutils` 2.47 from Homebrew, `agbcc` (Nintendo's GCC 2.8.1 fork)
built from `pret/agbcc` into `../wariowareinc-main/tools/agbcc/`. Two macOS/ARM64
traps:

1. `build.sh` passes `$CCOPT` to `make` **unquoted** — a `CC` with spaces falls
   apart into make options. Wrap clang in a script. agbcc is K&R C from 1998 and
   needs `-std=gnu89` with implicit-function/int errors off.
2. The Homebrew binutils get **SIGKILL**ed (exit 137, `Killed: 9` on
   `arm-none-eabi-as`). Re-sign every Mach-O binary:

```sh
for f in $(find /opt/homebrew/Cellar/arm-none-eabi-binutils/*/ -type f -perm -u+x); do
  file -b "$f" | grep -q Mach-O && { xattr -c "$f"; codesign -f -s - "$f"; }
done
```

## What matching takes

Rules that held up across ~190 byte-exact functions:

1. Signedness is readable: `LSLS #16 / LSRS #16` is `u16`, `LSLS #16 / ASRS #16`
   is `s16`, `#24` for 8 bit. A wrong return type adds two instructions and
   shifts everything after.
2. Header declarations for undecompiled functions are guesses, not facts.
3. Disassemble first, then write C. Varying the C without comparing is guessing.
4. Undeclared functions do not convert their arguments — write the cast by hand.
   Where a prototype is visible it is binding, and an extra cast generates nothing.
5. `MOVS #k; RSBS Rn,Rn,#0` marks a bitfield assignment: the mask `~(k-1)` is
   computed in `int` width and does not fit a Thumb immediate.
6. Bitfield accesses narrow to the smallest containing unit, so unit boundaries
   follow from the masks, not the offsets. `LSLS #a; LSRS #b` reads a field at
   bit `b-a`, width `32-b`.
7. A 16-bit stack parameter is loaded whole (`LDR`), an `s32` stored as a halfword
   becomes `LDRH` from the slot. The ROM gives away the parameter width.
8. The signedness of a 16-bit parameter shows at the caller, not the callee.
9. A local copy of an array base forces hoisting into a callee-saved register.
10. An exit in the middle of `while (1)` avoids a duplicated loop test.
11. `ASRS #1` is `>> 1`, not `/ 2` — signed division adds `ADD Rn, Rn, LSR #31`.
12. Case bodies follow source order; the jump table is sorted by value.
13. A named temporary pulls its load forward.
14. Where the parameter normalization sits tells where the narrowing is.
15. Two loads of the same value mean the test belongs in the `while` head; one
    load means the exit belongs in the middle.
16. Declaration order of two variables with the same initial value picks the
    register.
17. A new return type changes the callers. Verify the whole unit, not the function.

## Scene data structs

`gCurrentSceneData` is a `void *`; every scene lays its own struct over the same
IWRAM buffer. Reconstructed from the disassembly, not guessed: `scan_struct.py`
follows a base pointer through all registers and logs every access with width and
signedness, `bitfield_report.py` derives bit positions from masks, `gen_struct.py`
emits C with named padding.

| Struct | File | Size |
|---|---|---|
| `MainMenuSceneData` | `src/scenes/main_menu.h` | 0x1F8, 92 used offsets |
| `NameSelectSceneData` | `src/scenes/name_select.h` | 0x4F8, 69 used offsets |
| `IntroSceneData` | `src/scenes/intro.h` | 0xA0, 16 used offsets |
| `SceneObjectDef`, `SceneSpriteMotion*` | `src/scene.h` | 0x10, four motion variants |
| `GfxTableTask` | `src/graphics_table.h` | 0x5C |

Each was checked against already byte-exact code before new functions were built
on it.

Three functions stay assembly — register allocation, not semantics:
`scene_set_sprite_motion_sine_vel` (seven live short values is exactly where the
decision tips), `func_0800BEC0` (the ROM splits the range at 3, agbcc tests the
lower bound first), `func_080135E8` (forcing the ROM's register choice costs an
extra callee-saved register).

## PC build

```sh
make -C pc
./pc/wariowarepc                    # viewer, 3x
./pc/wariowarepc --frames 300       # non-interactive self-test
```

| Target | |
|---|---|
| `pc/wariowarepc` | viewer: scene blob plus animation bank through the native sprite pipeline into SDL2. Works. |
| `pc/wariowarepc-game` | the decompiled game code, as far as it gets. Stops at the first unported function and names it. |

**Two compilers on purpose:** gcc-15 for the game code, clang for anything
touching the macOS SDK. The decompiled code lives on GNU C89 semantics — an
implicit declaration colliding with the real definition is a warning there, a
hard error under clang, and adding prototypes is not an option because agbcc's
argument conversions depend on their absence (rule 4). `-fcommon` is needed for
tentative definitions like `gGraphicsBuffer`.

**Pointer widths are the real enemy on 64 bit.** `-m32` would be right and is
impossible on Apple Silicon. Three patterns, same symptom — an address loses its
upper 32 bits:

1. Pointers in `u32` fields → `u32p` in `include/global.h`, which is
   `unsigned long` for the host and plain `u32` for agbcc, so the ROM output does
   not change (`verify_all.sh` after every such edit).
2. `(u32)` casts on pointers truncate even when the target is `u32p`; visible as
   `and x19, x19, #0xffffffff`. Hardware register writes stay genuinely 32 bit.
3. Implicit declarations return `int` under C89. Of 166 implicitly declared
   functions exactly four return a pointer; all four now have declarations.

**The hardware seam is small.** The game never writes hardware directly: it keeps
a shadow of the display state in IWRAM, and one function pushes it to the PPU once
per frame with three DMA bursts.

| | In the ROM | Replacement |
|---|---|---|
| Video out | `flush_graphics_buffer` | `pc/gba_runtime.c` → compositor |
| Keys | `REG_KEY` in `update_key_buffers` | `pc/gba_hw.c` ← SDL events |
| Block transfers | `dma3_set` / `dma3_fill` | `pc/gba_hw.c`, plain copies |
| Frame clock | VBlank IRQ | `gba_hw_frame_wait`, 59.7275 Hz |
| Audio | Sappy driver, still assembly | open |

Measured: compositor 0.31 ms per frame, whole frame 1.7 ms of 16.74 ms, 59.73 Hz
against a 59.7275 Hz target (the deadline is absolute; relative sleeps drift to
about 56 Hz).

## Where the game code stops

```
./pc/wariowarepc-game --frames 60 --scene 1

[stub] func_0800A7D4 is not ported yet -- the game gets this far.
Call path:
1  func_0800A7D4
2  update_active_beatscript_scene
3  func_08016D00
4  title_scene_run
5  main
```

Three failure reports, all via `backtrace_symbols_fd` and `write(2)` because the
game headers shadow `<stdio.h>`: `[stub]` for a missing function, `[fault]` with
address and program counter for a zeroed data block, `[hal]` for implausible DMA
operands. Without the call path a zeroed data block is unfindable — the program
dies with signal 11 and says nothing.

**How a function enters the port.** *Portable*: decompile, verify byte-exact, done
— the PC build takes the same C file. *Not portable*: it stays assembly and the
runtime provides a native equivalent (`-Dasm(x)=` makes the included assembly
disappear; entry in `pc/provided.txt`, implementation in `pc/gba_runtime.c`).
Three patterns so far: self-relocating code (`func_080EE644` copies a routine to
the stack because SRAM cannot be read from the cartridge), ARM blobs (division,
root, RLE), and the sound driver (four entry points, silent no-ops).

### The dispatcher (`func_0800A7D4`)

The beatscript opcode dispatcher, last block before a running title screen: 1574
instructions, 157 jump table entries, 106 distinct case bodies, median 24 bytes,
63 functions called (46 already C).

A draft sits in `wip/func_0800A7D4.c` — 705 lines, **not byte-exact**, not in the
build. Compared at instruction level (mnemonics without registers) 20 differences
remain out of 1350 and the total is off by three, so the structure holds. Missing:
the prologue (the ROM keeps `threadIndex` on the stack and `isSceneMode` in SL,
the draft does the opposite and holds seven long-lived values where the ROM holds
six) and cross-jumping into shared tails. Byte comparison is useless while the
prologue differs — one register decision shifts everything.

## Data is the second mountain

The GBA build takes data as it sits in the ROM: beatscripts as assembled `.bs`,
everything else as raw blocks under `bin/` (3.8 MB in 14 files). Both store
pointers as four bytes. Generated replacements live in `pcdata/`, outside the
decompilation tree.

**`bs2c.py`** lets the real assembler work instead of reimplementing 51 macros:
section bytes give the literals, relocations the symbols, global labels the array
bounds. `--verify` compiles the generated C back with agbcc and compares byte for
byte with the assembled `.bs` — only what matches goes into the build.

**`rom2c.py`** invents no structure: it emits the ROM block byte for byte as a C
array and expresses only the named pointer tables as real C pointers into it.
This unblocked the boot — `init_save_buffer` reaches the stage description
through `D_083A4B58[stage]`, which as a zeroed stub was an access to `0x2`.

## Renderer and sprite pipeline

`renderer/` is a software compositor for the GBA tile pipeline (zlib only, for
PNG). It consumes the same state `flush_graphics_buffer` hands to the hardware
once per frame. Scope: mode 0 plus regular sprites; affine backgrounds and windows
are missing. `render_scene` renders one frame to PNG, `render_sprites` also drives
the native sprite pipeline.

`renderer/sprite_engine.c` mirrors `lib_sprite` in the original's three stages but
with 64-bit-host structs. Deliberately *not* byte-exact: it is the display side,
and it steps aside once `src/lib_sprite.c` is fully decompiled.

## Verification

* Four extracted assets are **byte-identical** to the decompilation's reference
  files: `title_bg_tiles.4bpp` (7778 B), `title_bg_map_logo.tilemap` (1458 B),
  `title_bg_map_border.tilemap` (1788 B), `title_bg_map.tilemap` (1722 B).
* The reconstructed table at 0x083A8CAC matches `data/scenes/title/data.c` entry
  for entry and in order.
* Sample of 120 assets: median nibble entropy 2.30 (random would be 4.00), median
  share of colour index 0 at 39 % (random ~6 %).
* All 23 translation units reproduce the ROM byte-exactly.

## Traps

* Keep palette indices at runtime, do not bake RGB — the fade system mutates the
  512-colour palette every frame.
* Deliberate over-reads: graphics table entries copy 0x200 bytes out of arrays
  declared as 96 bytes. The GBA read neighbouring ROM.
* Uninitialized remains: `title_bg_tiles` decompresses 7778 bytes into an 8192
  byte window; 414 bytes stay whatever VRAM held.
* Header changes test the whole set — a new prototype can change argument
  conversion at *every* call site. `sh tools/verify_all.sh` after each one.

## State

| | |
|---|---|
| Translation units | 23, **all byte-exact against the ROM** |
| Open assembly includes in them | 429 |
| Unassigned functions | ~5470 loose `.s` files |
| Missing symbols in the PC build | 250 (116 functions, 134 data blocks) |
| Asset coverage | 913 of 1050 referenced (87 %) |
| ROM data as C | two beatscripts, one raw block, title graphics |
| ROM data raw | 3.8 MB in 14 `bin/` blocks |

Not extracted yet: ~12,631 animation records in `bin/data_0830C3B0.bin`, 765
instrument records in `bin/data_083AB638.bin`, the palettes, and the intro scene —
its beatscript points at functions and graphics tables all over the ROM, and a
beatscript can only be translated once everything it points at exists.

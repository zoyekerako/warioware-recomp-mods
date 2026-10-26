# gbarecomp build — the playable one

`gbarecomp` is a static recompiler, not a decompiler: it translates ARM/THUMB
into C++ that edits a CPU state. An emulator in C form — useless for the
decompilation next door, excellent as a playable artifact. On top sits a mod
layer that changes no ROM byte.

![Intro](ww_final.png)

## Run

```
./port/gbarecomp/wariowarerecomp \
    --config port/gbarecomp/wariowareinc.toml \
    --rom rom.gba --bios gba_bios.bin
```

**F10** opens the mod menu. `--no-window --frames N --dump-png FILE` renders one
frame headless; `--help` lists the rest. Rebuild: `sh port/gbarecomp/build.sh`
(cmake, SDL2). Known bug: `--screen MODE` breaks argument parsing — use
`GBARECOMP_SCREEN=raw`.

| | |
|---|---|
| Recompile | 0.07 s → 2811 functions, 14 MB C++ |
| Speed | 1800 frames in 3.9 s ≈ **460 FPS**, 7.7× real time |
| Memory faults | `unmapped=0 io_unhandled=0` |
| Cost of the mod | none measurable: 5.14 s vs 5.23 s over 1800 frames |

## What the decompilation contributed

**The data map.** `wariowareinc.ld` says which ROM range is code and which is
data. Without it the function finder decodes graphics as code:

| | functions | `undefined` |
|---|---|---|
| no config, 5480 symbols | 48,223 | 123 |
| with data ranges | 2,811 | **0** |

**The symbols.** 5480 addresses from `asm/`; a run with all of them reported
`manual_seeds_only: 1751`. The shipped build keeps the 24 that appeared as
`bridged`, holding the output at 14 MB instead of 274 MB.

**Three findings from the overlap.**

* The four addresses the runtime cannot resolve statically are exactly the ARM
  blobs (division, root, RLE) the decompilation marks as not portable. Two
  methods, same answer.
* Code runs at `0x080F0224` and `0x080F04CC`, but the decompilation lists that
  range as data (`bin/data_080EFE84.bin`). One of them is wrong.
* gbarecomp's self-healing passes `--export-all-symbols`, a MinGW flag Apple's
  `ld` rejects. Nine functions stay interpreted. At 460 FPS it does not show.

## Audio: two bugs

**The source rate ignored SOUNDBIAS.** `host_window.cpp` hardcoded 65536 Hz; the
game produces 32768 Hz. Double speed, one octave up — long tonal lines
unrecognizable, short effects still intelligible, exactly the reported symptom.
Fix: `set_audio_source_rate()` before every `drain_samples()`
(`audio-rate-fix.patch`).

**The sound FIFO DMA never raised its completion IRQ.** The game arms
`DMA2CNT_H = 0xF600` (bit 14 = IRQ), `IE = 0x2401` enables it, and `IntrMain`
checks DMA2 before VBlank — the engine rides that interrupt.
`run_sound_fifo_dma` lacked the branch that `run_immediate_dma` and
`run_timed_dma` both had, so the engine stopped ticking after init and the FIFO
played whatever IWRAM held 73 KB past the buffer.

```c
if (cnt_h & 0x4000u) request_irq(static_cast<uint16_t>(1u << (8 + channel)));
```

| 8-bit PCM from the DMA stream | mean delta | RMS | share \|v\|>100 |
|---|---|---|---|
| before | 13.41 | 29.3 | 3.3 % |
| after | 2.62 | 15.0 | 0.0 % |
| BIOS chime, as reference | 5.11 | 14.2 | 0.0 % |

DMA restarts after frame 273: 0 before, 46 after, on MP2K's 7-frame period. What
found it: logging every refill with source address **and** frame number.

## Mod menu

![Mod menu](ww_mod_menue.png)

F10, Start, or L+R+Select. D-pad selects, left/right adjusts, L/R jump coarse.
The guest sees 0x03FF — nothing pressed.

The game is not frozen: opening injects a Start pulse, closing an A pulse, so it
enters its *own* pause. A frozen guest makes no sound — its audio engine rides
the DMA2 interrupt.

| Setting | |
|---|---|
| Randomizer | Off / Microgames / With bosses |
| Seed | 1..9999, makes a run repeatable |
| KaizoWare | hardest level from the first microgame, tempo floor 200 % |
| Boss rush | every stage jumps to its boss after one microgame |
| Infinite lives | refills `currentLives` each frame |
| Cutscene skip | lifts the first-playthrough lock |
| Game speed | 50..300 % |
| Microgame readout | number, stage, difficulty on screen |
| Touch input | Off / Drag / Flick |
| Music volume, Sound volume | 0..100 % |
| Menu music | title theme while the menu is open |
| Widescreen (May have bugs) | Off / 16:10 / 16:9 / 21:9 / Max / Auto |
| Scaling | Sharp (integer) / Fit / Stretch |
| Rotation | 0 / 90 / 180 / 270 |
| Backdrop | Ambient / Black |

Presets via environment: `WWMOD_RANDOM`, `WWMOD_SEED`, `WWMOD_SPEED`,
`WWMOD_MUSIC_VOLUME`, `WWMOD_SOUND_VOLUME`, `WWMOD_ROTATION`, `WWMOD_WIDESCREEN`,
`WWMOD_SCALING`, `WWMOD_BLACK_BACKDROP`, `WWMOD_INFINITE_LIVES`,
`WWMOD_SKIP_CUTSCENES`, `WWMOD_KAIZO`, `WWMOD_BOSS_RUSH`, `WWMOD_READOUT`,
`WWMOD_TOUCH`, `WWMOD_TOUCH_TABLE`, `WWMOD_NO_MENU_MUSIC`, `WWMOD_NO_VIEW`.

## Randomizer

`func_08008E1C` starts every microgame; the mod hooks its last call
(`g_runtime_fn_entry_hook` sits in every recompiled prologue, `R14 == 0x080090FE`
identifies the site, R0 points at `{wrapper, script, 0}` on the guest stack):

```
08008F16  STRB R0,[R1]  (0x27C)   microgameID = entry.word0 >> 8
08008F82  STR  R0,[R6,#0xC]       music       = entry+4, else D_083A50E0[x].unk4
080090EA  LDR  R0,[R0]            args[1]     = D_083A50E0[x].unk0
080090FA  BL   set_beatscript_subscenes
```

The choice is a **pure function** of seed, stage, `unk70` and score — required,
because `runtime_should_yield()` may restart a guest function and the hook must
then choose the same thing twice. It also makes a run shareable by seed.

**Music belongs to the microgame.** The first version swapped only `args[1]` and
the ID, leaving the replaced game's music. The mod now writes `unkC` too, but
only when `unk228 == 0` (at 1 and 2 the music comes from a stage medley). Trace,
seed 42: `3 -> 121 SUPER MARIO, music 1031 'm_BGM_BOMB_31_Mario'`.

**Swaps stay inside their group.** `D_083A50E0[i].unk8` splits the 250 entries
into microgames (0..190), bosses (191..214), unlockables (215..240) and rest
screens (241..249). Over 12000 frames with the same input tape:

| | microgame starts |
|---|---|
| off | 0 swaps, healthy |
| group-true | 21 swaps, healthy |
| across all 250 | 7 swaps — **stalled** after the first cross-group one |

An unlockable game never ends by itself; in a microgame slot it hangs the stage.

## KaizoWare and boss rush

Not new mechanics — two fields the game already keeps.

**Difficulty** (+0x174) is reset at every microgame start and read by 35
microgames; the range is 0..2 with 2 the hardest. KaizoWare pulls it back each
frame: 70 raises over 12000 frames against 69 microgame starts, one per start.
Tempo gets a floor of 200 %; same run, 21 starts at 100 % versus 69 with
KaizoWare.

**Boss rush** raises `unk70` to the threshold in `gGameplayData.unk6c->unk0_1`,
which `gameplay_run_script` compares before calling `func_08008E1C()`. Measured
by position of the microgame in the stage:

| position | 0 | 1 | 2 | 3 | 4 | 5 |
|---|---|---|---|---|---|---|
| off | 5 | 4 | 4 | 4 | 3 | 1 |
| on | 9 | – | – | – | – | – |

The first start per stage stays, so it is "one microgame, then the boss".

**Not done: shuffled command words.** `currentMicrogameID` has two readers in the
whole game, both `EORS R0, #0x65` ("am I game 101?"), so the ID does not drive
the text. The table at `0x083A6090` holds title and description for the list
screen, not the word shown at launch — that lives in each microgame's script.

## Cutscene skip: the lock, not the jump

The game skips cutscenes by itself and only forbids it the first time.
`func_0801720C` hands out the permission bit; `func_0800A3A4` never got its own
entry in the generated code, so the mod sets the bit itself (bit 23 at
`gGameplayData+0x04`). The script's own "is this section skippable" gate is
untouched. Counted only where the game *actively* clears the bit, 20000 frames
each: fresh save 1 lock lifted (stage 0), save with progress 0.

## Pointer input

![Pointer](ww_zeiger.png)

WarioWare has no cursor — every microgame reads `gPressedKeys` or `gCurrentKeys`,
so a pointer can only **invent** key presses. Schemes: `tap` (touch = A pulse),
`hold` (A while resting), `drag` (held direction past the dead zone), `flick`
(one edge, for games reading edges), `both` (default).

**The suggestions are in the ROM.** Every microgame description carries the
control glyphs the game displays, escaped into the text
(`.e:1QR:0.3JUMP`). Across all 228 descriptions:

| code | hits | verbs |
|---|---|---|
| `QR` | 132 | JUMP, SHOOT, ENTER, DROP, CATCH, GRAB |
| `GH` | 76 | MOVE, SELECT, STEER, DODGE |
| `OP` | 41 | MOVE, SELECT, BURROW |
| `IJ` | 12 | MOVE, AIM, TRANSFORM |
| `EF` | 4 | DUCK, STEER, HIDE GUT |
| `KL`, `OJ`, `CD`, `MN` | 1 each | ATTACK, GUARD, –, MOVE |

`QR` is the button, the rest directions: button → `tap`, direction → `drag`, both
→ `both`. `gen_touch_ini.py` writes `touch.ini` from that: **89 tap, 89 drag, 44
both, 9 off**, 19 without a suggestion, and one `hold` by hand — `60 CHIRITORIE`
steers a vacuum *while* the button is held, which a pulse misses. Edge versus
level is in each game's code, so `flick` and `hold` cannot be derived from data.

The pointer is read as **state**, not events (`win.pump()` has already drained
the SDL queue). The coordinate math is `blit_game` run backwards, which makes it
checkable: `ww_mod_preview --selftest` walks 3024 points through four rotations,
three scalings and four window shapes — worst error one game pixel.

## Mod menu by mouse and finger

![Menu in portrait](ww_menue_touch.png)

| | |
|---|---|
| Open | menu button, top right; appears on pointer movement, fades after 3 s |
| Pick a row | tap it |
| Change a value | `<` and `>`; the field between them steps forward |
| Change fast | hold an arrow — repeats after 0.3 s, coarse after twenty steps |
| Close | `Close`, or tap outside |

Arrows and values act on touch-down (otherwise nothing could be held); anything
that opens or closes acts on release, so a mis-tap can be dragged off and the
finger that opens the menu does not close it again. A press belongs to whatever
it hit on touch-down — a press on the menu button never reaches the game. Drawing
and hit testing share one calculation: `draw_menu` stores every hit zone while
drawing.

Fixed value field (eleven characters) lines the arrows up across rows; the menu
grew from 37 to 46 columns for it. Spare height goes into row height — 22 to 38
pixels at 675x1200, same font. 18 self-test checks, all passing.

## The game's own menus by click

![Click on "Grid" and what comes of it](ww_menue_klick.png)

| Clicked | What happens |
|---|---|
| Tab `Games`, `Grid`, `Options` | switches and enters |
| Character folder | the hand walks there, the stage starts |
| Cell in the microgame grid | same for the single game |
| Options row | selected and opened |
| `OK` / `No way!` in a prompt | the button is pressed |
| `B <-` bottom right | back one level |

**None of this calls a menu function of the game.** The menu hangs on a drawn
hand that walks with the D-pad, so a click can only invent the key presses a
player would make. Locks stay the game's business: tapping a sealed `?` folder
walks the hand there and gets the same refusal tone.

Hit zones come from the game's own tables — `D_083AA0C4` (28 folders, +4..+7
neighbours, +0xC hand position), `D_083AA294` (grid layout), `D_083A9CE0` (tabs),
`D_083AB2CC` (options), `D_083AB354` (prompt), with state and tab in
`D_03006518`. Measured on a rendered frame: a folder sits four pixels left and
above its hand and fills 25x20. Grid cells come from `func_080136F4` (48x32, hand
42 right and 24 below the corner) and the live `BG_OFS[2].x` decides which column
is where.

Each frame the plan compares where the hand is with where it should go, checks
readiness exactly as `func_08011698` does, and emits **one** key press. The route
is a breadth-first search through the same neighbour network the game walks
(`func_08012768` and its three siblings), so it finds the detour a player would
need and never gets stuck; no route ends the plan silently. Clicking Mona
(folder 5) from a cold start:

```
state 0 tab 2 entry 0 -> left   (target folder 5)
state 0 tab 1 entry 0 -> left
state 0 tab 0 entry 0 -> A
state 1 tab 0 entry 0 -> up
state 1 tab 0 entry 1 -> up
state 1 tab 0 entry 5 -> A
```

Touch-down starts the hand moving, release opens. Since the screen moves by
itself — the hand walks, the grid scrolls — release counts on the same target
**or** within six pixels of the press; without that, every quick tap in a
scrolling grid would be lost. The keyboard or the mod menu cancels a running
route.

**Evidence:** 40 self-test checks against the real ROM tables, all passing, plus
eight runs of the real program on a simulated screen driven by `WWMOD_TEST_SCRIPT`
— tab switch, folders in both layouts, a started stage, the back button, the
options prompt and its cancel, a cell from the grid preview, the BOSS column, a
locked folder, a window tap in 16:9 (where the 22-pixel margin is accounted for),
and the counter-test where the mod menu opens mid-route. All `unmapped=0`.

## Volume: the right field could not be guessed

Music and effects only separate inside the guest — in the mixer both sit in the
same FIFO. `sound_player_table` (0x08406430) holds nine players; the plain-text
title says which is which (`m_BGM_...` versus `s_BASIC_...`).

`set_soundplayer_volume` forwards to a single `STRH R2,[R0,#0x26]` — and setting
that to a quarter lowered the level by **12 %**. It is the target value, not the
level. Found by measuring instead: `WWMOD_MUTE_OFFSET=<hex>` zeroes a `u16` at a
chosen offset every frame, 1200 frames per step, RMS of the second half:

| offset | 0x1E | 0x20 | 0x22 | 0x24 | 0x26 | **0x28** | 0x2A | 0x2C |
|---|---|---|---|---|---|---|---|---|
| level left | 100 % | 100 % | 50 % | 100 % | 88 % | **1.6 %** | 100 % | 99 % |

With `+0x28` the sliders work: 100 % → RMS 3123, 50 % → 1426, 0 % → 50. Writes
go through a shadow value, so the game's own fades survive instead of being
overwritten.

## Menu music: one guest call

Starting a track means running `func_080F2E48(songId)` in the guest. The mod does
it the way the runtime runs IRQ and SWI: save registers, push a return marker,
`runtime_dispatch`, restore. Called only from the present hook, after
`runtime_should_yield` has spent its VBlank marker. `WWMOD_NO_GUEST_CALL=1`
disables it. During gameplay the game's pause stops the beatscript, so the title
theme stands still while the menu is open.

## Window: free scaling, widescreen, portrait

![Filled border](ww_mod_breitbild.png) ![Portrait](ww_mod_hochkant.png)

The surface follows the window (capped at 1920x1200), the game image sits inside
at integer scale, and the border is filled rather than black — the game image
averaged to a coarse grid, blurred, darkened, recomputed every fourth frame.
Rotation by 90/180/270 gives portrait. Four seam points: `ww_mod_owns_view()`,
`ww_mod_view_width()`, `ww_mod_surface_size()`, `ww_mod_compose()`.

### Real widescreen

![Real widescreen](ww_breitbild_gut.png)

`Widescreen` widens the PPU view at runtime. View height stays 160 lines, so
width alone sets the aspect: 256 (16:10), **284** (16:9), 374 (21:9), 480
(runtime limit), or Auto = window width · 160 / height. Borderless needs
`Scaling = Fit`, since 900 lines are not a multiple of 160.

Why "(May have bugs)": WarioWare composed nothing in the extra columns, so the
background map repeats. On wallpaper nobody notices; on a motif with a centre:

![Repeated map](ww_breitbild_echt.png)

The same tennis court three times. Fixable per game by hand, not across 191
microgames plus bosses and rest screens.

A runner bug hid this for a while: `runner/main.cpp` built a `RunOptions` and
then called `run_game(argc, argv)` without it, so the widescreen opt-in, the game
name and the ROM checksum were all ignored. `opts` is passed now.

## Where the code lives

`port/gbarecomp/mod/` — `ww_mod.cpp` (menu, modes, audio, presentation, pointer
input, menu clicks), `ww_font.h` (5x8 font from `genfont.py`), `ww_mod_preview.cpp`
(renders the overlay into a PPM without the game; `--selftest [rom.gba]` checks
pointer math and menu hits), `touch.ini` (one scheme per microgame).

Test hooks: `WWMOD_TEST_SCRIPT=<file>` drives key presses and the pointer (one
line per event: `key`, `tap`, `wtap`, `hold`, `move`, `up`, `shot`, `log`),
`WWMOD_MENU_TRACE=1` logs every invented key press, `WWMOD_TRACE=1` every
microgame start, `WWMOD_TEST_MENU=<frame>` opens and closes the mod menu,
`WWMOD_DUMP_COMPOSE=<path>` writes a composed frame.

gbarecomp itself gains seven calls against weakly defined no-ops; without the mod
linked in it behaves exactly as before. That seam is `mod-seam.patch` and is lost
when gbarecomp is updated, like the other two patches.

## Limits

* Nothing here was **listened to** — this machine had no audio device. The rate
  fix, the volume sliders and the menu music are measured, not heard.
* Everything runs under `SDL_VIDEODRIVER=dummy` with synthetic input. That the
  injected pulses reach the guest is proven; that a real *keyboard* does is not.
* The randomizer is measured, not played through: 21 consecutive swaps without a
  stall is not the same as 191 microgames in all stages.
* Touch is computed, not touched. Whether SDL on a real device delivers fingers
  and emulated mouse events the way `poll_pointer` assumes is open.
* Clicks reach as far as the main menu. Name entry, the epilogue list and the
  title screen keep the old behaviour, where a tap becomes A.

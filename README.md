# WarioWare Inc. on the PC

Two ways to the same game, plus the work behind them.

| | |
|---|---|
| [`port/gbarecomp/`](port/gbarecomp/README.md) | The playable build. `gbarecomp` statically recompiles the ROM to C++; a mod layer on top adds a menu, randomizer, KaizoWare, boss rush, volume sliders, free window shapes with widescreen and portrait, pointer input, and clickable game menus. No ROM byte is changed. |
| [`port/`](port/README.md) | The native port and its tools: ROM verifiers, struct reconstruction, graphics and beatscript pipelines, PC build. |
| `wariowareinc-main/` | The decompilation everything is checked against. Every unit must assemble to the same bytes as the ROM: `sh port/tools/verify_all.sh`. |

## Not in this repo

Supply your own `rom.gba`, `gba_bios.bin`, `rom.sav` and `wariowareinc-main/baserom.gba`.
The translated BIOS under `src/runtime/generated_bios/` is generated — `build.sh` makes it again.
The `gbarecomp` clone is third-party source; the three changes to it sit here as patches:
`mod-seam.patch`, `audio-rate-fix.patch`, `sound-dma-irq-fix.patch`.

## Build

```sh
sh port/gbarecomp/build.sh        # needs cmake and SDL2
./port/gbarecomp/wariowarerecomp \
    --config port/gbarecomp/wariowareinc.toml \
    --rom rom.gba --bios gba_bios.bin
```

**F10** opens the mod menu.

## Origin

Decompilation from the public `wariowareinc` project, playable build from `gbarecomp`.
WarioWare, Inc.: Mega Microgame$ is © 2003 Nintendo. Nothing here comes from Nintendo
that you don't have to extract from your own cartridge.

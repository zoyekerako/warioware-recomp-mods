# PC build.
#
# Two compilers, on purpose:
#   GAME_CC  the decompiled sources rely on GNU C89 -- implicit declarations
#            that later conflict with the real definition are a warning there
#            and a hard error under clang.  They also must not gain prototypes,
#            because agbcc's argument conversions depend on their absence.
#   PC_CC    anything that includes the macOS SDK.  Homebrew GCC's fixed copy
#            of <stdio.h> references a header this SDK does not ship, so the
#            shell, the compositor and the loaders are built with clang.
#
# 32-bit: the GBA is a 32-bit machine and the structs assume 4-byte pointers,
# so -m32 would be the natural choice.  macOS has no 32-bit userland at all on
# Apple Silicon, so it is unavailable -- see README, "Zeigergroessen".

GAME_CC ?= gcc-15
PC_CC   ?= cc

REPO    := ../../wariowareinc-main
COMPAT  := compat
RENDER  := ../renderer

SDL_CFLAGS := $(shell pkg-config --cflags sdl2)
SDL_LIBS   := $(shell pkg-config --libs sdl2)

# -fcommon: the game headers declare objects like gGraphicsBuffer as tentative
# definitions, which every including unit emits.  That merged on the GBA and
# must merge here too; GCC 10 made -fno-common the default.
# WWPC_WIDE_POINTERS widens the u32 fields that hold addresses (see global.h).
# -fno-omit-frame-pointer: the harness reports a call path on every stub and
# every fault, and without frame pointers backtrace() drops most of it.
# -MMD -MP: without header dependencies an edit to a struct leaves stale
# objects behind, and the resulting crash looks like a code bug.
GAME_CFLAGS := -std=gnu89 -O2 -w -fcommon -fno-omit-frame-pointer -MMD -MP \
               -D'asm(x)=' -DWWPC_WIDE_POINTERS \
               -include $(abspath $(COMPAT))/gba_pc_mem.h \
               -I$(abspath $(COMPAT)) -I$(REPO) -I$(REPO)/include \
               -iquote $(REPO)/include
PC_CFLAGS   := -std=c11 -O2 -Wall -Wextra -MMD -MP $(SDL_CFLAGS) -I$(RENDER)

# code_08000f10.c is the hardware abstraction layer; pc/gba_hal.c replaces it.
# data/ and graphics/ carry the ROM tables that have been converted to C; the
# raw bin/ blobs cannot be used here because their pointers are four bytes.
# *.inc.c are include fragments, not units of their own.
GAME_SRCS := $(filter-out %/code_08000f10.c %.inc.c,\
                $(shell find $(REPO)/src $(REPO)/data $(REPO)/graphics -name '*.c'))
# ../pcdata holds ROM data translated to C for this build only -- see its
# README for why it cannot live in the decomp tree.
PCDATA_SRCS := $(shell find ../pcdata -name '*.c')
# Object paths mirror the source tree: two scenes both have a scene_bs.c.
GAME_OBJS := $(patsubst $(REPO)/%.c,obj/game/%.o,$(GAME_SRCS)) \
             $(patsubst ../pcdata/%.c,obj/pcdata/%.o,$(PCDATA_SRCS))
RUNTIME   := obj/gba_runtime.o obj/gba_hal.o obj/generated_stubs.o
# The viewer needs the native sprite engine; the game target must not link it,
# because the decompiled lib_sprite.c defines the same names.
VIEWER_OBJS := obj/assets_io.o obj/gba_hw.o obj/gba_ppu.o obj/sprite_engine.o
GAME_SHELL  := obj/gba_hw.o obj/gba_ppu.o

all: wariowarepc

# The asset viewer: shell only, no game code. This is the one that runs today.
wariowarepc: obj/sdl_main.o $(VIEWER_OBJS)
	$(PC_CC) -o $@ $^ $(SDL_LIBS)

# The game code, driven as far as it currently goes. Stops at the first
# function that has not been decompiled yet and names it.
wariowarepc-game: obj/game_main.o $(GAME_SHELL) $(GAME_OBJS) $(RUNTIME)
	$(PC_CC) -o $@ $^ $(SDL_LIBS)

obj/game/%.o: $(REPO)/%.c | obj/game
	@mkdir -p $(dir $@)
	$(GAME_CC) $(GAME_CFLAGS) -c -o $@ $<

obj/pcdata/%.o: ../pcdata/%.c | obj
	@mkdir -p $(dir $@)
	$(GAME_CC) $(GAME_CFLAGS) -c -o $@ $<

obj/gba_runtime.o obj/gba_hal.o: obj/%.o: %.c | obj
	$(GAME_CC) $(GAME_CFLAGS) -I$(abspath $(RENDER)) -c -o $@ $<

obj/generated_stubs.o: generated_stubs.c | obj
	$(GAME_CC) -std=gnu89 -O2 -w -fcommon -c -o $@ $<

obj/%.o: %.c | obj
	$(PC_CC) $(PC_CFLAGS) -c -o $@ $<

obj/%.o: $(RENDER)/%.c | obj
	$(PC_CC) $(PC_CFLAGS) -c -o $@ $<

obj obj/game:
	mkdir -p $@

# Rebuild the stub list after decompiling more of the game.
stubs: obj/game_main.o
	python3 ../tools/gen_pc_stubs.py --cc $(GAME_CC) \
	        -o generated_stubs.c --provided provided.txt --compat $(COMPAT) \
	        --also obj/game_main.o

clean:
	rm -rf obj wariowarepc wariowarepc-game

-include $(shell find obj -name '*.d' 2>/dev/null)

.PHONY: all clean stubs

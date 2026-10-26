/* What the hardware and the linker script used to provide.
 *
 * The GBA build got three things for free that a PC build has to supply: RAM at
 * fixed addresses (the linker script placed gBeatscriptScene at 0x03004890 and
 * the scene buffer at 0x03003860), the display flush that pushed the IWRAM
 * shadow to the PPU, and a handful of BIOS and library routines.  None of that
 * is game content, so all of it belongs here rather than in a stub.
 *
 * Compiled with the game's own headers so every struct layout matches what the
 * decompiled code expects.
 */
#include "global.h"
#include "types.h"
#include "graphics.h"

#include "../../port/pc/gba_hw.h"
#include "../../port/renderer/gba_ppu.h"

#include <math.h>
#include <string.h>

/* ---- RAM ------------------------------------------------------------- */

/* fill_memory_dma clears 0x35C words here, so the block is 0xD70 bytes. */
u8 D_03003860[0xD70];
/* gCurrentSceneData now comes from data/scenes.c, the real ROM definition. */

struct BeatscriptScene gBeatscriptScene;
struct GraphicsBuffer gGraphicsBuffer;
struct BeatscriptLocalData *gCurrentSceneVariable;
s16 *gCurrentSceneSpritePool;
s16 gCurrentScene;

/* ---- Input ----------------------------------------------------------- */

u16 gCurrentKeys;
u16 gPressedKeys;
u16 gPrevKeys;
u16 gRepeatedKeys;
u16 gKeyRepeatTimer;


/* ---- The display seam ------------------------------------------------- */

/* On hardware this was three DMA bursts: the display registers, then the two
   palette shadows.  VRAM and OAM were already written in place.  Here the same
   state is handed to the software compositor, which the SDL loop then blits. */
static GbaVideoState wwpc_video;

void flush_graphics_buffer(void)
{
    int i;

    wwpc_video.dispcnt = gGraphicsBuffer.DISPCNT;
    for (i = 0; i < 4; i++) {
        wwpc_video.bgcnt[i] = gGraphicsBuffer.BG_CNT[i];
        wwpc_video.bg_hofs[i] = gGraphicsBuffer.BG_OFS[i].x;
        wwpc_video.bg_vofs[i] = gGraphicsBuffer.BG_OFS[i].y;
    }
    memcpy(wwpc_video.bg_palette, gGraphicsBuffer.bgPalette,
           sizeof wwpc_video.bg_palette);
    memcpy(wwpc_video.obj_palette, gGraphicsBuffer.objPalette,
           sizeof wwpc_video.obj_palette);
}

const GbaVideoState *wwpc_video_state(void)
{
    return &wwpc_video;
}

/* DMA3 completes instantly here, so there is never anything pending. */
void trigger_pending_dma3(void)
{
}

/* ---- Heap ------------------------------------------------------------- */

/* The original hands out blocks from a fixed EWRAM arena and tags each with the
   owning memory id so a scene teardown can free everything it allocated at
   once.  A bump allocator with the same tagging is enough until the real one is
   decompiled: nothing here depends on addresses being reused. */
#define WWPC_HEAP_BYTES 0x40000
#define WWPC_HEAP_BLOCKS 1024

static u8 wwpc_heap[WWPC_HEAP_BYTES];
static u32 wwpc_heap_used;

static struct {
    void *base;
    u32 memID;
    u8 live;
} wwpc_blocks[WWPC_HEAP_BLOCKS];

static u32 wwpc_block_count;

void *mem_heap_alloc_id(u32 memID, u32 size)
{
    void *p;

    size = (size + 3u) & ~3u;
    if (wwpc_heap_used + size > WWPC_HEAP_BYTES ||
        wwpc_block_count >= WWPC_HEAP_BLOCKS) {
        return NULL;
    }
    p = &wwpc_heap[wwpc_heap_used];
    wwpc_heap_used += size;

    wwpc_blocks[wwpc_block_count].base = p;
    wwpc_blocks[wwpc_block_count].memID = memID;
    wwpc_blocks[wwpc_block_count].live = TRUE;
    wwpc_block_count++;

    memset(p, 0, size);
    return p;
}

void mem_heap_dealloc(void *ptr)
{
    u32 i;

    for (i = 0; i < wwpc_block_count; i++) {
        if (wwpc_blocks[i].base == ptr) {
            wwpc_blocks[i].live = FALSE;
            return;
        }
    }
}

void mem_heap_dealloc_with_id(u32 memID)
{
    u32 i;

    for (i = 0; i < wwpc_block_count; i++) {
        if (wwpc_blocks[i].memID == memID) {
            wwpc_blocks[i].live = FALSE;
        }
    }
}

/* ---- Fixed-point maths ------------------------------------------------ */

/* agbcc emits calls to __divsi3 for signed division; the game wraps it. */
s32 fast_divsi3(s32 numerator, s32 denominator)
{
    if (denominator == 0) {
        return 0;
    }
    return numerator / denominator;
}

/* 256 steps over a full turn, Q8.8, as the tables in ROM are laid out. */
s8_8 gSineTable[0x100];
s8_8 gCosineTable[0x100];

u16 gBlackPalette[0x100];
u16 gWhitePalette[0x100];

void wwpc_runtime_init(void)
{
    int i;

    for (i = 0; i < 0x100; i++) {
        double a = (double)i * 6.283185307179586 / 256.0;
        gSineTable[i] = (s8_8)(sin(a) * 256.0);
        gCosineTable[i] = (s8_8)(cos(a) * 256.0);
        gBlackPalette[i] = 0x0000;
        gWhitePalette[i] = 0x7FFF;
    }
}

/* ---- Sound driver ------------------------------------------------------ */

/* The Sappy driver is still assembly and has no PC counterpart yet, so its
   four entry points are silent no-ops.  Naming them here rather than letting
   the stub generator abort on them is deliberate: the game runs without sound,
   and stopping the frame loop for it would hide everything downstream. */
void func_080F4090(void) {}
void func_080F3C60(s32 a, s32 b, s32 c, s32 d) { (void)a; (void)b; (void)c; (void)d; }
void func_080F3C78(void) {}
void func_080F3A90(void) {}

u16 D_03003840;   /* frame counter AgbMain increments */
u8 D_03003638;    /* set once soft reset is armed */

/* ---- EWRAM ------------------------------------------------------------ */

/* The GBA has 256 KB of external work RAM.  The ROM symbols that point into it
   -- D_083A3D94, main_save_memory_base, backup_save_memory_base -- are defined
   by data/agb_main_data.c now; the compat header aims their bases at these
   arrays, so the real definitions work unchanged. */
u8 wwpc_bios[0x4000];
u8 wwpc_ewram_block[0x40000];
u8 wwpc_iwram[0x8000];
u8 wwpc_ioram[0x400];
u8 wwpc_palram[0x400];
u8 wwpc_cartram[0x10000];

void *D_03003BBC[1] = { wwpc_ewram_block };

/* ---- Save memory ------------------------------------------------------ */

/* Cartridge SRAM can only be read a byte at a time, and only by code running
   from RAM rather than from the cartridge itself -- which is why func_080EE644
   copies a small routine onto the stack and jumps there.  Neither constraint
   exists on a PC, so both entry points reduce to the byte copy they perform.
   The originals stay in the decomp as assembly; code that relocates itself has
   no C form worth reconstructing. */
void func_080EE644(const u8 *src, u8 *dst, u32 size)
{
    while (size-- != 0) {
        *dst++ = *src++;
    }
}

void func_080EE6A8(const u8 *src, u8 *dst, u32 size)
{
    while (size-- != 0) {
        *dst++ = *src++;
    }
}



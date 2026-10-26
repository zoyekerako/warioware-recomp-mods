#pragma once

#include "global.h"
#include "graphics.h"

// TYPES

/* Task state for a background graphics-table load.  The table is walked one
   entry per call; unk0_1 marks the task as still running. */
struct GfxTableTask {
    u32 unk0_1 : 1;                 // 0x00:0
    u32 unk0_2 : 7;                 // 0x00:1
    u8 pad1[3];                     // 0x01
    u32 unk4;                       // 0x04
    const struct GraphicsTable *unk8; // 0x08
    void *unkC;                     // 0x0C
    void *unk10;                    // 0x10
    u8 pad14[0x48];                 // 0x14
};

/* The two words start_new_task copies into the task before it first runs. */
struct GfxTableTaskArgs {
    const struct GraphicsTable *table; // 0x00
    u32 limit;                         // 0x04
};

// DATA
extern u8 D_083A4494[];

// FUNCTIONS
extern void func_08002124(struct GfxTableTask *, const struct GraphicsTable *, u32);
extern void func_080021C8(struct GfxTableTask *);
extern u16 func_08002468(struct GfxTableTask *);
extern void *mem_heap_alloc(u32 size);
extern s32 start_new_task(u16 memID, void *, void *, s32, s32);

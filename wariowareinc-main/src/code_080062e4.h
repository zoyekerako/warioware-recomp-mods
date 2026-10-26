#pragma once

#include "global.h"
#include "src/lib_sprite.h"
#include "src/memory.h"
#include "src/code_08001a70.h"
#include "src/code_08000f10.h"
#include "graphics.h"

/* The three words start_new_task copies into a scheduled-call task. */
struct ScheduledCall {
    void (*function)(s32);  // 0x00
    s32 param;              // 0x04
    u32 delay;              // 0x08
};

extern struct TaskMethods D_083A4B38;
extern s32 schedule_function_call(u16 memID, void *function, s32 param, u32 delay);

// DATA
extern struct Unk03000010 D_03004454[];
extern u32 D_03004054[];
extern void *D_0300485C;

/* The ARM division helper and the decompressor bracket the block that is
   copied into IWRAM; their symbols delimit it. */
extern u8 fast_udivsi3_rom[];
extern u8 gfx_decompress_rom[];
extern u8 D_03000C18[];
extern void *D_03003FF0;

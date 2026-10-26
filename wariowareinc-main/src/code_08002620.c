#include "code_08002620.h"

asm(".include \"include/gba.inc\"");

/* Copies the square-root routine from ROM into IWRAM, where it runs without
   the cartridge's wait states. The block ends where the next routine starts. */
void func_08002620(void) {
    volatile u32 *dma = &REG_DMA3SAD;

    dma[0] = (u32)math_sqrt_rom;
    dma[1] = (u32)D_030001C0;
    dma[2] = ((u32)fast_udivsi3_rom - (u32)math_sqrt_rom) / 4
             | ((DMACNT_SIZE | DMACNT_ENABLE) << 16);
    dma[2];

    D_03003FE4 = D_030001C0;
}

#include "asm/code_08002620/asm_08002654.s"

#include "asm/code_08002620/asm_080026bc.s"

#include "asm/code_08002620/asm_080027d8.s"

#include "asm/code_08002620/asm_080028c4.s"

#include "asm/code_08002620/asm_080029d0.s"

#include "asm/code_08002620/asm_080029e4.s"

#include "asm/code_08002620/asm_080029ec.s"

#include "asm/code_08002620/asm_08002b68.s"

#include "asm/code_08002620/asm_08002be4.s"

#include "asm/code_08002620/asm_08002bec.s"

#include "asm/code_08002620/asm_08002d10.s"

#include "asm/code_08002620/asm_08002d6c.s"

#include "asm/code_08002620/asm_08002ec8.s"

#include "asm/code_08002620/asm_08002efc.s"

#include "asm/code_08002620/asm_08002fb4.s"

#include "asm/code_08002620/asm_08002fc0.s"

#include "asm/code_08002620/asm_08002fe8.s"

#include "asm/code_08002620/asm_08003014.s"

#include "asm/code_08002620/asm_08003028.s"

#include "asm/code_08002620/asm_08003040.s"

#include "asm/code_08002620/asm_08003058.s"

#include "asm/code_08002620/asm_08003074.s"

#include "asm/code_08002620/asm_0800309c.s"

#include "asm/code_08002620/asm_08003134.s"

#include "asm/code_08002620/asm_0800317c.s"

#include "asm/code_08002620/asm_080031b4.s"

#include "asm/code_08002620/asm_080031e0.s"

#include "asm/code_08002620/asm_08003228.s"

#include "asm/code_08002620/asm_0800323c.s"

#include "asm/code_08002620/asm_08003298.s"

#include "asm/code_08002620/asm_080032e4.s"

#include "asm/code_08002620/asm_08003338.s"

#include "asm/code_08002620/asm_08003368.s"

#include "asm/code_08002620/asm_0800390c.s"

#include "asm/code_08002620/asm_0800397c.s"

#include "asm/code_08002620/asm_08003988.s"

#include "asm/code_08002620/asm_08003998.s"

#include "asm/code_08002620/asm_080039b4.s"

#include "asm/code_08002620/asm_080039c0.s"

#include "asm/code_08002620/asm_080039d0.s"

#include "asm/code_08002620/asm_080039ec.s"

#include "asm/code_08002620/asm_08003a00.s"

#include "asm/code_08002620/asm_08003a0c.s"

#include "asm/code_08002620/asm_08003a70.s"

#include "asm/code_08002620/asm_08003b10.s"

#include "asm/code_08002620/asm_08003b54.s"

#include "asm/code_08002620/asm_08003b58.s"

#include "asm/code_08002620/asm_08003d1c.s"

#include "asm/code_08002620/asm_08003d28.s"

#include "asm/code_08002620/asm_08003d3c.s"

void func_08003DA0(void) {
    dma3_set(rle_decompression, D_030003A8, math_sqrt_rom - rle_decompression,
             0x10, 0x100);

    D_03003FEC = D_030003A8;
    D_03004880 = 0;
}

#include "asm/code_08002620/asm_08003de0.s"

#include "asm/code_08002620/asm_08003df4.s"

/* Stops DMA0 the way the hardware requires: point it at a scratch word, enable
   it with both addresses fixed, then disable it again. */
void func_08003E64(void) {
    volatile u32 dummy;

    D_03000528.unk0_1 = FALSE;
    D_03000528.unk0_4 = FALSE;

    REG_DMA0SAD = (u32)&dummy;
    REG_DMA0DAD = (u32)&dummy;

    REG_DMA0CNT_H = DMACNT_DEST_INC_TYPE_UNCHANGED | DMACNT_SRC_INC_TYPE_UNCHANGED
                    | DMACNT_ENABLE;
    dummy = 0;
    dummy = 0;
    REG_DMA0CNT_H = 0;

    D_03000684 = 0;
}

#include "asm/code_08002620/asm_08003eb0.s"

#include "asm/code_08002620/asm_08003f14.s"

#include "asm/code_08002620/asm_08003f50.s"

#include "asm/code_08002620/asm_08003f8c.s"

#include "asm/code_08002620/asm_08003fb8.s"

#include "asm/code_08002620/asm_08003fd0.s"

#include "asm/code_08002620/asm_08004130.s"

#include "asm/code_08002620/asm_0800418c.s"

#include "asm/code_08002620/asm_080041a0.s"

#include "asm/code_08002620/asm_080041b4.s"

#include "asm/code_08002620/asm_080041cc.s"

/* Thin wrapper: the last two arguments go no further. */
u32p func_080042F4(u16 memID, void *arg1, u16 arg2, u8 arg3, u32 arg4, u32 arg5) {
    return func_0800430C(memID, arg1, arg2, arg3);
}

u32p func_0800430C(u16 memID, void *arg1, u16 arg2, u8 arg3) {
    struct Unk0800430C *task = mem_heap_alloc_id(memID, sizeof(struct Unk0800430C));

    task->unk0 = memID;
    task->unk4 = arg1;
    task->unk8 = arg2;
    task->unkA = arg3;

    task->unkC = mem_heap_alloc_id(memID, arg3 * 32);
    task->unk10 = mem_heap_alloc_id(memID, arg3 * 16);
    task->unk14 = 0;
    task->unk18 = 0;

    func_080043D4(task);

    return (u32p)task;
}

#include "asm/code_08002620/asm_08004378.s"

#include "asm/code_08002620/asm_080043a0.s"

#include "asm/code_08002620/asm_080043b8.s"

/* Clears both buffers, exactly as func_08004EE8 does for its own record. */
void func_080043D4(struct Unk0800430C *task) {
    u32 i;

    for (i = 0; i < task->unkA * 16; i++) {
        task->unkC[i] = 0;
        task->unk10[i] = 0;
    }
}

#include "asm/code_08002620/asm_08004400.s"

#include "asm/code_08002620/asm_0800442c.s"

#include "asm/code_08002620/asm_08004480.s"

#include "asm/code_08002620/asm_080044c4.s"

#include "asm/code_08002620/asm_08004518.s"

#include "asm/code_08002620/asm_0800454c.s"

#include "asm/code_08002620/asm_08004580.s"

#include "asm/code_08002620/asm_08004770.s"

#include "asm/code_08002620/asm_08004798.s"

#include "asm/code_08002620/asm_080047d4.s"

#include "asm/code_08002620/asm_080047e8.s"

#include "asm/code_08002620/asm_08004994.s"

#include "asm/code_08002620/asm_080049a4.s"

#include "asm/code_08002620/asm_080049bc.s"

#include "asm/code_08002620/asm_080049cc.s"

#include "asm/code_08002620/asm_08004a30.s"

#include "asm/code_08002620/asm_08004a40.s"

#include "asm/code_08002620/asm_08004a74.s"

#include "asm/code_08002620/asm_08004a84.s"

#include "asm/code_08002620/asm_08004ae0.s"

#include "asm/code_08002620/asm_08004b00.s"

#include "asm/code_08002620/asm_08004b78.s"

#include "asm/code_08002620/asm_08004bd4.s"

#include "asm/code_08002620/asm_08004bf4.s"

#include "asm/code_08002620/asm_08004c94.s"

#include "asm/code_08002620/asm_08004cbc.s"

#include "asm/code_08002620/asm_08004ce0.s"

#include "asm/code_08002620/asm_08004e28.s"

/* Sets one of these up: the record itself plus the two buffers whose size the
   entry count fixes.  Returns it as a word because that is how the scenes
   store it. */
u32p func_08004E48(u16 memID, void *arg1, u8 arg2, u16 arg3, u8 arg4) {
    struct Unk08004E48 *task = mem_heap_alloc_id(memID, sizeof(struct Unk08004E48));

    task->unk0 = arg1;
    task->unk6 = arg2;
    task->unk4 = arg3;
    task->unk7 = arg4;

    task->unk8 = mem_heap_alloc_id(memID, arg4 * 32);
    task->unkC = mem_heap_alloc_id(memID, arg4 * 16);

    func_08004EE8(task);

    return (u32p)task;
}

#include "asm/code_08002620/asm_08004eac.s"

#include "asm/code_08002620/asm_08004ec8.s"

/* Clears both buffers. */
void func_08004EE8(struct Unk08004E48 *task) {
    u32 i;

    for (i = 0; i < task->unk7 * 16; i++) {
        task->unk8[i] = 0;
        task->unkC[i] = 0;
    }
}

#include "asm/code_08002620/asm_08004f14.s"

#include "asm/code_08002620/asm_08004f2c.s"

#include "asm/code_08002620/asm_0800504c.s"

#include "asm/code_08002620/asm_08005090.s"

#include "asm/code_08002620/asm_08005148.s"

#include "asm/code_08002620/asm_080051d4.s"

#include "asm/code_08002620/asm_08005214.s"

#include "asm/code_08002620/asm_08005320.s"

#include "asm/code_08002620/asm_08005364.s"

#include "asm/code_08002620/asm_080053b0.s"

/* Registers every entry of a NULL-terminated list and reports how many. */
u32 func_08005538(struct SpriteHandler *handler, u32p arg1, void **list, s16 *pool) {
    void **entry;
    u32 count = 0;

    if (*list != NULL) {
        entry = list;
        do {
            func_080053B0(handler, arg1, *entry++, pool);
            count++;
        } while (*entry != NULL);
    }

    return count;
}

void func_08005570(u8 value) {
    D_03003FE8 = value;
}

#include "asm/code_08002620/asm_0800557c.s"

#include "asm/code_08002620/asm_0800559c.s"

#include "asm/code_08002620/asm_080055d4.s"

#include "asm/code_08002620/asm_08005600.s"

void func_0800567C(void) {
    u32 i;

    D_03000698.unk0 = 0;

    for (i = 0; i <= 0x2F; i++) {
        D_03000698.entries[i].unk0_1 = FALSE;
        D_03000698.entries[i].unk8 = -1;
    }
}

/* Ends a task: stop method, the finish callback unless it is being skipped,
   then the task's own allocation and the slot. */
void task_stop(struct Unk03000698Entry *entry, u32 skipCallback) {
    if (entry->methods->stop != NULL) {
        entry->methods->stop(entry->unkC);
    }

    if (skipCallback == 0 && entry->unk10 != NULL) {
        entry->unk10(entry->unk14);
    }

    if (entry->unkC != NULL) {
        mem_heap_dealloc(entry->unkC);
    }

    entry->unk0_1 = FALSE;
    entry->unk8 = -1;
}

/* One pass over the pool: every running task whose delay has expired gets its
   delayed update, and a task that reports done is stopped. */
void task_pool_update_delayed(void) {
    struct Unk03000698Entry *entry;
    u32 i;

    if (D_03000698.unk4 != 0) {
        return;
    }

    for (i = 0, entry = D_03000698.entries; i <= 0x2F; i++, entry++) {
        if (!entry->unk0_1 || entry->unk0_2 != 0) {
            continue;
        }

        if (entry->methods->delayedUpdate == NULL) {
            continue;
        }

        if (entry->unk18 != 0) {
            entry->unk18--;
        } else if (entry->methods->delayedUpdate(entry->unkC)) {
            task_stop(entry, 0);
        }
    }
}

/* The same pass without the delay: these run every frame. */
void task_pool_update_constant(void) {
    struct Unk03000698Entry *entry;
    u32 i;

    if (D_03000698.unk4 != 0) {
        return;
    }

    for (i = 0, entry = D_03000698.entries; i <= 0x2F; i++, entry++) {
        if (!entry->unk0_1 || entry->unk0_2 != 0) {
            continue;
        }

        if (entry->methods->constantUpdate == NULL) {
            continue;
        }

        if (entry->methods->constantUpdate(entry->unkC)) {
            task_stop(entry, 0);
        }
    }
}

/* Claims the first free slot of the task pool, runs the task's start method and
   returns the pool number it was given -- or -1 if the pool is full or the
   start method refused. */
s32 start_new_task(u16 memID, struct TaskMethods *methods, void *info,
                   TaskFinalFunc onFinish, s32 onFinishArg) {
    struct Unk03000698Entry *entry;
    u32 i;

    for (i = 0, entry = D_03000698.entries; i <= 0x2F; i++, entry++) {
        if (!entry->unk0_1) {
            break;
        }
    }

    if (i > 0x2F) {
        return -1;
    }

    entry->unkC = (methods->start != NULL) ? methods->start(info) : NULL;

    if (entry->unkC == TASK_FAILED_TO_START) {
        return -1;
    }

    entry->unk0_1 = TRUE;
    entry->unk0_2 = 0;
    entry->methods = methods;
    entry->unk8 = D_03000698.unk0;
    entry->memID = memID;
    entry->unk10 = onFinish;
    entry->unk14 = onFinishArg;
    entry->unk18 = 0;

    D_03000698.unk0 = (D_03000698.unk0 + 1) & 0x7FFFFFFF;

    return entry->unk8;
}

#include "asm/code_08002620/asm_08005834.s"

#include "asm/code_08002620/asm_08005870.s"

#include "asm/code_08002620/asm_080058ac.s"

#include "asm/code_08002620/asm_080058dc.s"

#include "asm/code_08002620/asm_08005914.s"

#include "asm/code_08002620/asm_08005920.s"

#include "asm/code_08002620/asm_0800596c.s"

#include "asm/code_08002620/asm_080059a8.s"

#include "asm/code_08002620/asm_080059e4.s"

#include "asm/code_08002620/asm_08005a1c.s"

#include "asm/code_08002620/asm_08005a54.s"

#include "asm/code_08002620/asm_08005a90.s"

#include "asm/code_08002620/asm_08005b20.s"

#include "asm/code_08002620/asm_08005b40.s"

#include "asm/code_08002620/asm_08005b70.s"

#include "asm/code_08002620/asm_08005c20.s"

#include "asm/code_08002620/asm_08005cf8.s"

#include "asm/code_08002620/asm_08005de0.s"

#include "asm/code_08002620/asm_08005e0c.s"

#include "asm/code_08002620/asm_08005e48.s"

#include "asm/code_08002620/asm_08005f64.s"

#include "asm/code_08002620/asm_08005fa0.s"

#include "asm/code_08002620/asm_08005fb8.s"

#include "asm/code_08002620/asm_08006084.s"

/* Hands the arena to the allocator: the first word holds the free count. */
void func_08006148(u32 *base, u32 size) {
    D_03000BE0 = base;
    D_03000BE4 = size / 4;
    *base = size / 4 - 1;

    D_03003FD0.unk0 = 0;
    D_03003FD0.unk4 = base;
    D_03003FD0.unk8 = size;
    D_03003FD0.unk10 = 0;
    D_03003FD0.unkC = 0;
}


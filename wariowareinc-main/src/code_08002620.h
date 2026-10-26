#pragma once

#include "global.h"
#include "src/memory_heap.h"
#include "src/task_pool.h"
#include "src/lib_sprite.h"

// TYPES

/* One slot of the table func_0800567C clears; 0x1C bytes, 48 of them. */
struct Unk03000698Entry {
    /* One 16-bit unit: start_new_task clears everything above bit 0 in a
       single halfword write. */
    u16 unk0_1 : 1;               // 0x00:0  belegt
    u16 unk0_2 : 15;              // 0x00:1
    u16 memID;                    // 0x02
    struct TaskMethods *methods;  // 0x04
    s32 unk8;                     // 0x08  laufende Nummer
    void *unkC;                   // 0x0C  Rueckgabe der Startfunktion
    TaskFinalFunc unk10;          // 0x10  laeuft, wenn der Task endet
    s32 unk14;                    // 0x14  dessen Argument
    u16 unk18;                    // 0x18
    u8 pad1A[2];                  // 0x1A
};

struct Unk03000698 {
    u32 unk0;                            // 0x00  laufende Nummer
    u32 unk4;                            // 0x04  sperrt beide Update-Laeufe
    struct Unk03000698Entry entries[48]; // 0x08
};

/* Sound-driver state; func_08003E64 clears two of its flags. */
struct Unk03000528 {
    u8 unk0_1 : 1;   // 0x00:0
    u8 unk0_2 : 2;   // 0x00:1
    u8 unk0_4 : 1;   // 0x00:3
    u8 unk0_5 : 4;   // 0x00:4
};

/* Bookkeeping for the arena func_08006148 hands out. */
struct Unk03003FD0 {
    u32 unk0;   // 0x00
    u32 *unk4;  // 0x04
    u32 unk8;   // 0x08
    u32 unkC;   // 0x0C
    u32 unk10;  // 0x10
};

/* Allocated by func_08004E48: a source pointer, two counters and two buffers
   sized from the last argument. */
struct Unk08004E48 {
    void *unk0;   // 0x00
    u16 unk4;     // 0x04
    u8 unk6;      // 0x06
    u8 unk7;      // 0x07
    u16 *unk8;    // 0x08  unk7 * 16 Eintraege
    u8 *unkC;     // 0x0C  unk7 * 16 Eintraege
};

extern void func_08004EE8(struct Unk08004E48 *);

/* Allocated by func_0800430C: the same shape as struct Unk08004E48 with the
   owning memory id and two more words in front. */
struct Unk0800430C {
    u16 unk0;     // 0x00  memID
    u8 pad2[2];   // 0x02
    void *unk4;   // 0x04
    u16 unk8;     // 0x08
    u8 unkA;      // 0x0A  unk7-Aequivalent: Anzahl / 16
    u8 padB;      // 0x0B
    u16 *unkC;    // 0x0C
    u8 *unk10;    // 0x10
    u32 unk14;    // 0x14
    u32 unk18;    // 0x18
};

extern void func_080043D4(struct Unk0800430C *);
extern u32p func_0800430C(u16 memID, void *arg1, u16 arg2, u8 arg3);
extern void func_080053B0(struct SpriteHandler *handler, u32p arg1, void *entry, s16 *pool);
extern u32 func_08005538(struct SpriteHandler *handler, u32p arg1, void **list, s16 *pool);
extern s32 start_new_task(u16 memID, struct TaskMethods *methods, void *info,
                          TaskFinalFunc onFinish, s32 onFinishArg);
extern void task_stop(struct Unk03000698Entry *entry, u32 skipCallback);
extern void task_pool_update_delayed(void);
extern void task_pool_update_constant(void);

/* The same storage as D_03000698.entries, addressed through its own symbol.
   On the GBA the two overlap by address; the PC build makes them overlap with
   a macro in pc/compat/gba_pc_mem.h. */


// DATA

/* ARM routines that live in ROM and are copied into IWRAM to run fast.
   The blocks are delimited by the next routine's symbol. */
extern u8 math_sqrt_rom[];
extern u8 fast_udivsi3_rom[];
extern u8 gfx_decompress_rom[];
extern u8 rle_decompression[];

/* Their IWRAM landing sites, and the pointers the callers go through. */
extern u8 D_030001C0[];
extern u8 D_030003A8[];
extern void *D_03003FE4;
extern void *D_03003FEC;

extern struct Unk03000698 D_03000698;
extern struct Unk03003FD0 D_03003FD0;
extern u32 *D_03000BE0;
extern u32 D_03000BE4;
extern u8 D_03003FE8;
extern u8 D_03004880;
extern struct Unk03000528 D_03000528;
extern u8 D_03000684;

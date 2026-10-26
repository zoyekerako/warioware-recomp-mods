#include "graphics_table.h"

asm(".include \"include/gba.inc\"");

/* A high bit in the pointer means one more level of indirection. */
void *func_0800210C(void *ptr) {
    if ((s32)ptr < 0) {
        ptr = *(void **)((u32)ptr & 0x7FFFFFFF);
    }

    return ptr;
}

#include "asm/graphics_table/asm_08002124.s"

#include "asm/graphics_table/asm_080021c8.s"

u16 func_08002468(struct GfxTableTask *task) {
    return task->unk0_1;
}

void func_08002470(struct GraphicsTable *entry) {
    entry->dest = NULL;
    entry->src = NULL;
    entry->size = 0;
}

/* Copies entries up to the source's terminator, then terminates the copy. */
void func_0800247C(struct GraphicsTable *dest, const struct GraphicsTable *src) {
    while (src->src != NULL) {
        dest->src = src->src;
        dest->dest = src->dest;
        dest->size = src->size;
        dest++;
        src++;
    }

    dest->dest = NULL;
    dest->src = NULL;
    dest->size = 0;
}

/* As func_0800247C, but stops after at most `count` entries. */
void func_080024A4(struct GraphicsTable *dest, const struct GraphicsTable *src, s32 count) {
    while (src->src != NULL && count != 0) {
        dest->src = src->src;
        dest->dest = src->dest;
        dest->size = src->size;
        dest++;
        src++;
        count--;
    }

    dest->dest = NULL;
    dest->src = NULL;
    dest->size = 0;
}

void func_080024D0(struct GraphicsTable *entry, const void *src, void *dest, s32 size) {
    entry->src = src;
    entry->dest = dest;
    entry->size = size;

    entry++;
    entry->dest = NULL;
    entry->src = NULL;
    entry->size = 0;
}

/* Appends a whole table after the end of an existing one. */
void func_080024E4(struct GraphicsTable *table, const struct GraphicsTable *src) {
    while (table->src != NULL) {
        table++;
    }

    func_0800247C(table, src);
}

/* Appends a bounded copy of a table after the end of an existing one. */
void func_080024FC(struct GraphicsTable *table, const struct GraphicsTable *src, s32 count) {
    while (table->src != NULL) {
        table++;
    }

    func_080024A4(table, src, count);
}

/* Appends a single entry after the end of an existing table. */
void func_08002514(struct GraphicsTable *table, const void *src, void *dest, s32 size) {
    while (table->src != NULL) {
        table++;
    }

    func_080024D0(table, src, dest, size);
}

#include "asm/graphics_table/asm_08002530.s"

void *func_08002568(struct GfxTableTaskArgs *args) {
    struct GfxTableTask *task = mem_heap_alloc(0x5C);

    func_08002124(task, args->table, args->limit);

    return task;
}

u32 func_08002584(struct GfxTableTask *task) {
    func_080021C8(task);

    /* The ROM reads the whole flag byte here rather than the bitfield. */
    return !(*(u8 *)task & 1);
}

s32 start_load_gfx_table_task(u16 memID, const struct GraphicsTable *gfxTable, u32 limit) {
    struct GfxTableTaskArgs args;

    args.table = gfxTable;
    args.limit = limit;

    return start_new_task(memID, D_083A4494, &args, 0, 0);
}

#include "asm/graphics_table/asm_080025bc.s"

void func_080025F8(struct GraphicsTable *entry, const void *src, void *dest, s32 size) {
    entry->src = src;
    entry->dest = dest;
    entry->size = size;
}

void func_08002600(struct GraphicsTable *entry) {
    entry->src = NULL;
    entry->dest = NULL;
    entry->size = 0;
}

void func_0800260C(struct GraphicsTable *entry, const void *src, void *dest, s32 size) {
    entry->src = src;
    entry->dest = dest;
    entry->size = size;
}

void func_08002614(struct GraphicsTable *entry) {
    entry->src = NULL;
    entry->dest = NULL;
    entry->size = 0;
}

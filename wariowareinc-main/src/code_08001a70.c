#include "code_08001a70.h"

asm(".include \"include/gba.inc\"");

void func_08001A70(u32 count, struct Unk03000010 *params) {
    u16 *p;
    u32 i;

    D_03000110 = params;
    D_03000138 = count;
    p = (u16 *)D_03000010;

    for (i = 0; i < count; i++) {
        *p++ = 0x100;
        *p++ = 0;
        *p++ = 0;
        *p++ = 0x100;
        D_03000118[i] = 0;
    }
}

#include "asm/code_08001a70/asm_08001ac0.s"

s32 func_08001B04(u32 memID) {
    s32 id = func_08001AC0(memID);

    if (id >= 0) {
        D_03000140[id] = memID;
    }

    return id;
}

#include "asm/code_08001a70/asm_08001b28.s"

void func_08001B70(u32 memID) {
    s32 i;

    for (i = 0; i <= 0x1F; i++) {
        if (D_03000118[i] != 0 && D_03000140[i] == memID) {
            func_08001B28(i);
        }
    }
}

#include "asm/code_08001a70/asm_08001ba4.s"

#include "asm/code_08001a70/asm_08001c08.s"

#include "asm/code_08001a70/asm_08001c74.s"

#include "asm/code_08001a70/asm_08001cd0.s"

#include "asm/code_08001a70/asm_08001d5c.s"

void func_08001DA4(void) {
    u32 i;

    for (i = 0; i < D_03000138 * 4; i++) {
        D_03000110[i].unk6 = ((u16 *)D_03000010)[i];
    }
}

struct Unk03000010 *func_08001DE0(s32 index) {
    if (index < 0) {
        return NULL;
    }

    return &D_03000010[index];
}

u32 func_08001DFC(void) {
    u32 count = 0;
    u32 i;

    for (i = 0; i < 0x20; i++) {
        if (D_03000118[i] != 0) {
            count++;
        }
    }

    return count;
}

u32 func_08001E20(u32 memID) {
    u32 count = 0;
    u32 i;

    for (i = 0; i <= 0x1F; i++) {
        if (D_03000118[i] != 0 && D_03000140[i] == memID) {
            count++;
        }
    }

    return count;
}

#pragma once

#include "global.h"

// TYPES
/* One OAM affine parameter set: the identity is 0x100, 0, 0, 0x100. */
struct Unk03000010 {
    u16 unk0;
    u16 unk2;
    u16 unk4;
    u16 unk6;
};

// DATA
extern struct Unk03000010 D_03000010[];
extern u8 D_03000118[0x20];
extern u32 D_03000140[];
extern struct Unk03000010 *D_03000110;
extern u32 D_03000138;

// FUNCTIONS
extern s32 func_08001AC0(u32);
extern void func_08001B28(s32);

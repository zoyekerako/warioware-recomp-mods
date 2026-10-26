#pragma once

#include "global.h"
#include "graphics.h"
#include "scenes.h"
#include "src/lib_sprite.h"

// MACROS
#define gIntro CURRENT_SCENE_DATA(struct IntroSceneData)

// TYPES
struct IntroSceneData {
    u8 pad0[4];       // 0x00
    u32 unk4;         // 0x04
    u8 unk8;          // 0x08
    u8 pad9[7];       // 0x09
    u8 unk10;         // 0x10
    u8 pad11[0x27];   // 0x11
    u16 unk38;        // 0x38
    u16 unk3A;        // 0x3A
    u16 unk3C;        // 0x3C
    u8 pad3E[0xA];    // 0x3E
    s16 unk48;        // 0x48
    u32 unk4A_1 : 1;  // 0x4A:0
    u32 unk4A_2 : 1;  // 0x4A:1
    s16 unk4C;        // 0x4C
    s16 unk4E;        // 0x4E
    u16 unk50;        // 0x50
    u8 pad52[2];      // 0x52
    u32 unk54;        // 0x54
    u32 unk58;        // 0x58
    u32 unk5C;        // 0x5C
    u32 unk60;        // 0x60
    u8 pad64[0x38];   // 0x64
    u16 unk9C;        // 0x9C
};

// DATA

// FUNCTIONS

// EXTERNS
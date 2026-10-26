#pragma once

#include "global.h"
#include "sound.h"

// MACROS

// TYPES

/* One entry of a scene's object list.  `anims` is indexed by language, and
   `sprite` points at the slot that receives the created sprite's id -- the
   slot holds -1 while the object is not on screen. */
struct SceneObjectDef {
    struct Animation **anims; // 0x00
    s16 x;                    // 0x04
    s16 y;                    // 0x06
    u16 unk8;                 // 0x08
    u16 unkA;                 // 0x0A
    s16 *sprite;              // 0x0C
};

/* Five halfwords describing a scripted sprite movement; the task pool copies
   the block, so it is built on the stack or in a 0xC-byte heap cell. */
struct SceneSpriteMotion {
    s16 spriteId;   // 0x0
    u16 unk2;       // 0x2
    s16 unk4;       // 0x4
    s16 unk6;       // 0x6
    u16 unk8;       // 0x8
    u16 unkA;       // 0xA
};

/* The sine variants carry a u8 selector plus five extra halfwords. */
struct SceneSpriteMotionSine {
    s16 spriteId;   // 0x00
    u8 unk2;        // 0x02
    u8 pad3;        // 0x03
    s16 unk4;       // 0x04
    s16 unk6;       // 0x06
    u16 unk8;       // 0x08
    u16 unkA;       // 0x0A
    u16 unkC;       // 0x0C
    u16 unkE;       // 0x0E
    u16 unk10;      // 0x10
};

/* The sine-velocity variant fills eight halfwords; which of the four incoming
   coordinates lands where depends on the mode. */
struct SceneSpriteMotionSineVel {
    s16 spriteId;   // 0x00
    s16 unk2;       // 0x02
    s16 unk4;       // 0x04
    s16 unk6;       // 0x06
    s16 unk8;       // 0x08
    u16 unkA;       // 0x0A
    u16 unkC;       // 0x0C
    s16 unkE;       // 0x0E
};

/* The accelerating and sine-wave variants carry one extra halfword. */
struct SceneSpriteMotionAccel {
    s16 spriteId;   // 0x0
    u16 unk2;       // 0x2
    s16 unk4;       // 0x4
    s16 unk6;       // 0x6
    u16 unk8;       // 0x8
    u16 unkA;       // 0xA
    u16 unkC;       // 0xC
};

// DATA
extern u8 D_083A4A80[];
extern u8 D_083A4A90[];
extern u8 D_083A4AA0[];
extern u8 D_083A4AB0[];
extern u8 D_083A4AC0[];
extern u8 D_083A4AD0[];
extern u8 D_083A4AE0[];

// FUNCTIONS

// EXTERNS
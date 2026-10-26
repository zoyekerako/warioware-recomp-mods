#pragma once

#include "global.h"
#include "graphics.h"
#include "scenes.h"
#include "src/lib_sprite.h"
#include "src/code_08000f10.h"
#include "src/palette.h"
#include "src/scenes/gameplay.h"

// MACROS
#define gNameSelect CURRENT_SCENE_DATA(struct NameSelectSceneData)

// TYPES
struct NameSelectSceneData {
    u16 unk0;                      // 0x000
    u8 pad2[0x6];                  // 0x002
    void *unk8;                    // 0x008
    u8 padC[0x80];                 // 0x00C
    u16 unk8C;                     // 0x08C
    u8 pad8E[0xBE];                // 0x08E
    u8 unk14C;                     // 0x14C
    u8 unk14D[3];                  // 0x14D
    s16 unk150;                    // 0x150
    u16 unk152;                    // 0x152
    u16 unk154;                    // 0x154
    u16 unk156;                    // 0x156
    u16 unk158;                    // 0x158
    u16 unk15A;                    // 0x15A
    u16 unk15C;                    // 0x15C
    u16 unk15E;                    // 0x15E
    u16 unk160;                    // 0x160
    u8 pad162[0x4];                // 0x162
    u16 unk166;                    // 0x166
    u8 pad168[0x6];                // 0x168
    s16 unk16E;                    // 0x16E
    s16 unk170[3][13];             // 0x170  three rows of 0x1A bytes
    u8 pad1BE[0x2];                // 0x1BE
    u32 unk1C0;                    // 0x1C0
    u8 pad1C4[0xAA];               // 0x1C4
    u8 unk26E;                     // 0x26E
    u8 pad26F[0x3];                // 0x26F
    s16 unk272;                    // 0x272
    s16 unk274;                    // 0x274
    s16 unk276;                    // 0x276
    s16 unk278;                    // 0x278
    u32 unk27A_1 : 1;              // 0x27A
    u32 unk27A_2 : 7;              // 0x27A
    u32 unk27B_1 : 1;              // 0x27B
    u32 unk27B_2 : 1;              // 0x27B
    u32 unk27B_3 : 6;              // 0x27B
    u16 unk27C;                    // 0x27C
    u8 unk27E[4];                  // 0x27E
    s16 unk282;                    // 0x282
    u16 unk284;                    // 0x284
    u8 pad286[0x2];                // 0x286
    u32 unk288;                    // 0x288
    u16 unk28C;                    // 0x28C
    s16 unk28E[5];                 // 0x28E
    void *unk298[5];               // 0x298
    u8 pad2AC[0xF];                // 0x2AC
    u8 unk2BB;                     // 0x2BB
    u8 unk2BC;                     // 0x2BC
    u8 unk2BD;                     // 0x2BD
    s8 unk2BE;                     // 0x2BE
    u8 pad2BF[0x9];                // 0x2BF
    s16 unk2C8;                    // 0x2C8
    s16 unk2CA;                    // 0x2CA
    u8 unk2CC;                     // 0x2CC
    u8 pad2CD[0x1];                // 0x2CD
    u16 unk2CE;                    // 0x2CE
    s16 unk2D0;                    // 0x2D0
    u8 pad2D2[0x2];                // 0x2D2
    void **unk2D4;                 // 0x2D4
    u32 unk2D8;                    // 0x2D8
    u32 unk2DC;                    // 0x2DC
    u8 unk2E0;                     // 0x2E0
    u8 unk2E1;                     // 0x2E1
    u16 unk2E2;                    // 0x2E2
    u8 pad2E4[0x4];                // 0x2E4
    u16 unk2E8;                    // 0x2E8
    s16 unk2EA;                    // 0x2EA
    u8 pad2EC[0x4];                // 0x2EC
    u32 unk2F0;                    // 0x2F0
    u8 pad2F4[0xA];                // 0x2F4
    u8 unk2FE;                     // 0x2FE
    u8 pad2FF[0x2];                // 0x2FF
    u8 unk301;                     // 0x301
    u8 pad302[0x2];                // 0x302
    u8 unk304;                     // 0x304
    u8 pad305[0x3];                // 0x305
    u32 unk308;                    // 0x308
    u32 unk30C;                    // 0x30C
    u32 unk310;                    // 0x310
    u32 unk314;                    // 0x314
    u8 unk318;                     // 0x318
    u8 unk319;                     // 0x319
    u8 pad31A[0x2];                // 0x31A
    s16 unk31C;                    // 0x31C
    s16 unk31E;                    // 0x31E
    u16 unk320;                    // 0x320
    u32 unk322_1 : 1;              // 0x322
    u32 unk322_2 : 2;              // 0x322
    u32 unk322_4 : 5;              // 0x322
    u8 pad323[0x1];                // 0x323
    s16 unk324;                    // 0x324
    u8 pad326[0x2];                // 0x326
    s16 unk328;                    // 0x328
    u16 unk32A;                    // 0x32A
    u8 pad32C[0xB];                // 0x32C
    u8 unk337;                     // 0x337
    u8 unk338;                     // 0x338
    u8 pad339[0x1];                // 0x339
    s16 unk33A;                    // 0x33A
    u8 pad33C[0x1B8];              // 0x33C
    u8 unk4F4;                     // 0x4F4
};

struct Unk08547E78 {
    u16 x; // 0x0
    s16 y; // 0x2
};

// DATA
extern u8 D_083A93B0[];
extern u8 D_083A94B8[];
extern void (*D_083A9934[])(void);
extern u8 D_083A98E8[];
extern u8 D_083A98F0[];
extern const u16 D_0830C764[];
extern const u16 D_0830C964[];
extern u32 D_03004054[];
extern void (*D_083A9904[])(void);
extern u8 D_083FBB30[];
extern u8 D_083FBB1C[];
extern u8 D_083A9AF0[];
extern u8 D_083A98C8[];
extern struct GameplayData_struct_0 D_083A8588;
extern struct Unk08547E78 D_08547E78;
extern u8 D_0830C64C[];
extern u8 D_0830C67C[];

// FUNCTIONS
// EXTERNS

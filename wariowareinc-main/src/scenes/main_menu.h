#pragma once

#include "global.h"
#include "graphics.h"
#include "scenes.h"
#include "src/lib_sprite.h"

// MACROS
#define gMainMenu CURRENT_SCENE_DATA(struct MainMenuSceneData)

// TYPES
struct MainMenuSceneData {
    u32 unk0;             // 0x000
    u32 unk4;             // 0x004
    u8 unk8;              // 0x008
    u8 pad9[3];           // 0x009
    u32 unkC;             // 0x00C
    u8 unk10;             // 0x010
    u8 pad11[0x27];       // 0x011
    s16 unk38;            // 0x038
    s16 unk3A[28];        // 0x03A
    u8 pad72[6];          // 0x072
    u32 unk78;            // 0x078
    void* unk7C;          // 0x07C
    u16 unk80;            // 0x080
    u16 unk82;            // 0x082
    u32 unk84;            // 0x084
    u32 unk88_1:1;        // 0x088:0
    u32 unk88_2:8;        // 0x088:1
    s16 unk8A;            // 0x08A
    u8 pad8C[0x10];       // 0x08C
    s16 unk9C;            // 0x09C
    u8 pad9E[0xE];        // 0x09E
    s16 unkAC;            // 0x0AC
    u8 padAE[2];          // 0x0AE
    u32 unkB0;            // 0x0B0
    u8 unkB4;             // 0x0B4
    u8 padB5[3];          // 0x0B5
    u32 unkB8;            // 0x0B8
    u32 unkBC;            // 0x0BC
    u16 unkC0;            // 0x0C0
    u16 unkC2;            // 0x0C2
    u16 unkC4;            // 0x0C4
    u16 unkC6;            // 0x0C6
    u32 unkC8;            // 0x0C8
    u8 unkCC;             // 0x0CC
    u8 padCD[3];          // 0x0CD
    void* unkD0;          // 0x0D0
    void* unkD4;          // 0x0D4
    void* unkD8;          // 0x0D8
    u32 unkDC_1:8;        // 0x0DC:0
    u32 unkDC_9:1;        // 0x0DD:0
    s32 unkDC_10:1;       // 0x0DD:1
    s32 unkDC_11:1;       // 0x0DD:2
    s32 unkDC_12:1;       // 0x0DD:3
    s32 unkDC_13:1;       // 0x0DD:4
    s32 unkDC_14:1;       // 0x0DD:5
    s32 unkDC_15:1;       // 0x0DD:6
    u32 unkDC_16:1;       // 0x0DD:7
    u32 unkDC_17:1;       // 0x0DE:0
    u32 unkDC_18:1;       // 0x0DE:1
    u32 unkDC_19:1;       // 0x0DE:2
    u32 unkDC_20:1;       // 0x0DE:3
    u32 unkDC_21:1;       // 0x0DE:4
    u32 unkDC_22:1;       // 0x0DE:5
    u32 unkDC_23:1;       // 0x0DE:6
    u32 unkDC_24:1;       // 0x0DE:7
    u32 unkDC_25:1;       // 0x0DF:0
    u32 unkDC_26:1;       // 0x0DF:1
    s32 unkDC_27:1;       // 0x0DF:2
    u32 unkDC_28:1;       // 0x0DF:3
    u32 unkDC_29:4;       // 0x0DF:4
    u32 unkE0_1:1;        // 0x0E0:0
    u32 unkE0_2:8;        // 0x0E0:1
    u32 unkE0_10:4;       // 0x0E1:1
    u8 padE2[2];          // 0x0E2
    u32 unkE4;            // 0x0E4
    u8 unkE8;             // 0x0E8
    u8 padE9;             // 0x0E9
    s16 unkEA;            // 0x0EA
    s16 unkEC;            // 0x0EC
    u16 unkEE;            // 0x0EE
    u8 unkF0;             // 0x0F0
    u8 unkF1;             // 0x0F1
    u8 unkF2[2];          // 0x0F2
    void (*unkF4)(s32);   // 0x0F4
    u8 unkF8;             // 0x0F8
    u8 unkF9;             // 0x0F9
    u8 unkFA;             // 0x0FA
    u8 unkFB;             // 0x0FB
    u8 unkFC;             // 0x0FC
    u8 unkFD;             // 0x0FD
    u8 unkFE;             // 0x0FE
    u8 padFF;             // 0x0FF
    u16 unk100;           // 0x100
    s16 unk102;           // 0x102
    s16 unk104;           // 0x104
    u8 pad106[2];         // 0x106
    u32 unk108;           // 0x108
    u8 pad10C[8];         // 0x10C
    u32 unk114;           // 0x114
    u32 unk118;           // 0x118
    u32 unk11C;           // 0x11C
    u8 pad120[0x10];      // 0x120
    u32 unk130;           // 0x130
    u32 unk134;           // 0x134
    u8 pad138[4];         // 0x138
    u32 unk13C_1:1;       // 0x13C:0
    u32 unk13C_2:1;       // 0x13C:1
    u32 unk13C_3:30;      // 0x13C:2
    u32 unk140;           // 0x140
    void (*unk144)(void); // 0x144
    u32 unk148;           // 0x148
    u8 unk14C;            // 0x14C
    u8 pad14D[0x13];      // 0x14D
    s16 unk160;           // 0x160
    s16 unk162;           // 0x162
    u32 unk164;           // 0x164
    u32 unk168;           // 0x168
    u32 unk16C;           // 0x16C
    void (*unk170)(void); // 0x170
    void (*unk174)(void); // 0x174
    u32 unk178;           // 0x178
    u32 unk17C;           // 0x17C
    u32 unk180;           // 0x180
    s16 unk184;           // 0x184
    u8 pad186[2];         // 0x186
    u32 unk188;           // 0x188
    u32 unk18C;           // 0x18C
    u8 unk190;            // 0x190
    u8 pad191[3];         // 0x191
    s16 *unk194;          // 0x194
    u32 unk198;           // 0x198
    u32 unk19C;           // 0x19C
    u32 unk1A0;           // 0x1A0
    u16 unk1A4;           // 0x1A4
    s16 unk1A6;           // 0x1A6
    s16 unk1A8;           // 0x1A8
    u8 unk1AA[2];         // 0x1AA
    u32 unk1AC;           // 0x1AC
    u16 unk1B0;           // 0x1B0
    u16 unk1B2;           // 0x1B2
    u32 unk1B4;           // 0x1B4
    u8 unk1B8;            // 0x1B8
    u8 pad1B9;            // 0x1B9
    u16 unk1BA;           // 0x1BA
    u32 unk1BC;           // 0x1BC
    u32 unk1C0;           // 0x1C0
    u16 unk1C4;           // 0x1C4
    u16 unk1C6;           // 0x1C6
    u16 unk1C8;           // 0x1C8
    s16 unk1CA;           // 0x1CA
    u8 pad1CC[4];         // 0x1CC
    u16 unk1D0;           // 0x1D0
    u8 pad1D2[2];         // 0x1D2
    u32 unk1D4;           // 0x1D4
    u8 pad1D8[0x1E];      // 0x1D8
};

struct Unk03006518 {
    u8 unk0;
    u8 unk1;
    u8 unk2;
    u8 unk3;
    u8 unk4;
    s8 unk5;
    u8 unk6;
    u8 unk7;
};

struct Unk083AA0C4 {
    u8 unk0[4];
    s8 unk4;
    s8 unk5;
    s8 unk6;
    s8 unk7;
    u8 unk8[4];
    struct Vector2 *position;
};

struct Unk083AB478 {
    u16 unk0[4];
};

struct Unk03000E60 {
    u32 unk0_1 : 8;
    u32 unk0_9 : 10;
    u32 unk0_19: 4;
    u32 unk4;
    u32 unk8_1 : 8;
    u32 unk8_9 : 10;
    u32 unk8_19: 4;
    u32 unkA;
};

// DATA
extern struct GraphicsTable main_menu_gfx_table[];
extern struct Unk03006518 D_03006518;
extern struct Vector2 D_083AB2CC[2];
extern u16 D_0832027C[];
extern u16 D_083AB470[];
extern struct Unk083AB478 D_083AB478[];
extern struct Unk083AA0C4 D_083AA0C4[];
extern struct Unk083AA0C4 D_083AA294[];
extern void* D_083AA530;
extern void* D_083A4A2C;
extern void* D_083AB394;
extern u8 D_03003630;
extern struct GameplayData_struct_0 D_083AB020;
extern struct Unk03000E60 D_03000E60;
extern u8 *D_083AAD70[];
extern u8 D_083AA568[];
extern void **D_083AB320[];
extern void **D_083AA3C4[];
extern void **D_083AAF20[];
extern void *D_083AAF38[];

// FUNCTIONS
extern void func_08011504(s16 x, s16 y, void (*callback)(void), s32 arg);
extern void func_080119B8(void);
extern void main_menu_scene_init_gfx2(void);
extern void main_menu_scene_init_gfx3(void);
extern void func_08014428(void);
extern void func_08015944(s16, void*);
extern void func_0801208C(void);
extern void func_08012CB4(void);
extern void func_0801364C(void);
extern void func_08013E64(void);
extern void func_080147B0(void);
extern void func_08014B44(void);
extern void func_08014DE8(void);
extern void func_080153E0(void);
extern void func_08015930(void);
extern void func_08015A4C(void);
extern void func_080115DC(void);
extern void func_08003A70(void*);
extern void func_08015DBC();
extern void scene_hide_sprite();
extern void func_08011DFC(void);
extern void func_08015CD8(s32);
extern void func_0801216C(void);
extern void func_080121B8(void);
extern void func_08012274(void);
extern void func_08013184(void);
extern void func_08013624(void);
extern void func_08014FF4(void);
extern void func_0801429C(u32, u32);
extern void func_08014374(void);
extern void func_080143A0(void);
extern void func_08013660(void);
extern void func_080136A4(void);
extern void func_08015C38(void);
extern void func_08013E44(void);
extern void func_08013B88(void);
extern void func_08013AE0(void);

// EXTERNS
extern s32 schedule_function_call(u16 memID, void *function, s32 param, u32 delay);
extern s32 func_08005920(s32);
extern void func_08005834(s32);

extern struct Vector2* D_083A9CE0[];
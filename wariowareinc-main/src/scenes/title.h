#pragma once

#include "global.h"
#include "graphics.h"
#include "scenes.h"
#include "src/lib_sprite.h"

// MACROS
#define gTitle CURRENT_SCENE_DATA(struct TitleSceneData)

// TYPES
struct TitleSceneData {
    u32p unk0;          // 0x0 size:0x4 (haelt einen Zeiger)
    u32p unk4;          // 0x4 size:0x4 (haelt einen Zeiger)
    u8 inputsEnabled;            // 0x8
    u8 unkC[0x2F];      // 0x9 size:0x2F
    u16 unk38;          // 0x38 size:0x2
    u8 unk3A[0x12];     // 0x3A size:0x12
    u32 timeUntilIntro; // 0x4C size:0x4
};

// DATA
extern struct GraphicsTable title_gfx_table[];
extern struct CompressedData *title_buffered_textures[];
extern struct SubScene* D_083A9160;
extern u8 D_083ADB4C[];
extern u8 D_083ADADC[];
extern void *D_083A8C7C[];

// FUNCTIONS
extern void title_scene_run(void);

// EXTERNS
extern void scene_set_current_thread(u32);
extern s32 schedule_function_call(u16 memID, void *function, s32 param, u32 delay);
extern s32 start_new_texture_loader(u16 memID, struct CompressedData **textureList);
extern s32 start_load_gfx_table_task(u16 memID, const struct GraphicsTable *gfxTable, u32 limit);
extern u32p func_08004E48(u16, void*, u8, u16, u8);
extern u32p func_080042F4(u16, void*, u16, u8, u32, u32);

extern u32 title_scene_inputs_enabled();

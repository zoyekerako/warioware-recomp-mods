#pragma once

#include "global.h"
#include "scenes.h"
#include "src/graphics_table.h"
#include "src/palette.h"

// MACROS

/* The init scene addresses its data through the symbol rather than through
   gCurrentSceneData, so it is spelled out here the same way. */
#define gInitScene (*(struct InitSceneData *)D_03003860)

// TYPES
struct InitSceneData {
    u32 state;                        // 0x00
    struct GfxTableTask loader;       // 0x04
    struct PaletteInterpolator fade;  // 0x60
    u16 paletteBackup[0x100];         // 0x78
};

// DATA
extern s16 D_030035E0;
extern s16 gCurrentScene;

// FUNCTIONS
extern void func_0800DB5C(void);
extern u32 func_0800DBA0(void);

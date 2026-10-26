#include "src/scenes/name_select.h"
#include "src/code_08000f10.h"
#include "src/audio.h"

asm(".include \"include/gba.inc\"");

extern u8 D_0830C5AC[];
extern u8 D_083A98D0[];
extern u8 D_083A98B8[];
extern u8 D_083A98D8[];
extern u8 D_083FBBD0[];

void func_0800DE24(void) {
    if (D_030035E0 != 0) {
        func_08016CBC(D_083A9AF0);
    }

    if (func_08016D00()) {
        gCurrentScene = GLOBAL_SCENE_MAIN_MENU;

        if (func_080007C0(2) == 0) {
            gCurrentScene = GLOBAL_SCENE_GAMEPLAY;
            D_03003848 = 0;
            D_03003628 = &D_083A8588;
            D_03003634 = 0;
        }
    }
}

#include "asm/scenes/name_select/asm_0800de84.s"

#include "asm/scenes/name_select/asm_0800dee8.s"

#include "asm/scenes/name_select/asm_0800e5cc.s"

void func_0800E6F8(s32 index) {
    s16 sprite = gNameSelect.unk28C;

    if ((u16)index > 4) {
        sprite_set_visible(gSpriteHandler, sprite, FALSE);
    } else {
        sprite_set_x_y(gSpriteHandler, sprite, (s16)index * 16 + D_08547E78.x, D_08547E78.y);
        sprite_set_visible(gSpriteHandler, sprite, TRUE);
    }
}

void func_0800E760(void) {
}

void func_0800E764(void) {
    sprite_set_visible(gSpriteHandler, gNameSelect.unk2D0, FALSE);
}

void func_0800E78C(void *anim) {
    sprite_set_anim(gSpriteHandler, gNameSelect.unk2D0, anim, 0, 1, 0, 0);
}

void func_0800E7C0(s32 x, s32 y, u16 z) {
    sprite_set_x_y_z(gSpriteHandler, gNameSelect.unk2D0, (s16)x, (s16)y, z);
}

void func_0800E800(s32 x, s32 y) {
    sprite_set_x_y(gSpriteHandler, gNameSelect.unk2D0, x, y);
}

void func_0800E834(void) {
    func_0800E78C(D_0830C5AC);
    func_0800E7C0(0x48, 0x68, 0x4800);
}

#include "asm/scenes/name_select/asm_0800e850.s"

void func_0800EA44(s32 index) {
    gNameSelect.unk14D[index] = 1;
}

#include "asm/scenes/name_select/asm_0800ea5c.s"

#include "asm/scenes/name_select/asm_0800eb50.s"

#include "asm/scenes/name_select/asm_0800ebc8.s"

void func_0800EC80(void) {
    if (gNameSelect.unk282 <= 1) {
        gNameSelect.unk282++;
        gNameSelect.unk166 -= 0x10;
        gNameSelect.unk16E = 0x10;
        play_sound(D_083FBB30);
    }
}

void func_0800ECC8(void) {
    if (gNameSelect.unk282 > 0) {
        gNameSelect.unk282--;
        gNameSelect.unk166 += 0x10;
        gNameSelect.unk16E = 0xFFF0;
        play_sound(D_083FBB30);
    }
}

#include "asm/scenes/name_select/asm_0800ed14.s"

s32 func_0800ED74(s32 value, s32 index) {
    s32 length;

    if (gNameSelect.unk0 == 0) {
        length = D_083A93B0[index];
    } else {
        length = D_083A94B8[index];
    }

    return (value + length) % length;
}

#include "asm/scenes/name_select/asm_0800eda4.s"

#include "asm/scenes/name_select/asm_0800ef4c.s"

#include "asm/scenes/name_select/asm_0800f0f4.s"

void func_0800F294(void) {
    s32 row = gNameSelect.unk282;
    s32 col = gNameSelect.unk27E[row] != 0 ? 3 : 2;

    func_0800EA5C(gNameSelect.unk170[row][col], 0xA);
}

#include "asm/scenes/name_select/asm_0800f2dc.s"

/* Dispatches the pending name-select action; unk27A_1 gates it, unk27A_2 is
   the action id. */
void func_0800F754(void) {
    if (gNameSelect.unk27A_1) {
        switch (gNameSelect.unk27A_2) {
            case 0:
                func_0800F2DC();
                break;
            case 1:
                func_0800FEB0();
                break;
            case 2:
                func_0800E834();
                func_0800E760();
                func_0800E6F8(gNameSelect.unk2BB);
                func_0800F294();
                gNameSelect.unk27A_2 = 0;
                break;
            case 3:
                func_08010950();
                break;
            case 5:
                func_0800E6F8(-1);
                func_0801082C(0);
                break;
            case 4:
                func_0801082C(1);
                break;
            case 6:
                func_0801082C(2);
                break;
            case 7:
                func_0801082C(4);
                break;
            case 8:
                func_0801082C(5);
                break;
        }
    }
}

#include "asm/scenes/name_select/asm_0800f828.s"

#include "asm/scenes/name_select/asm_0800f8a8.s"

void func_0800F984(void) {
    if (gNameSelect.unk2BB != 0) {
        u32 index;

        gNameSelect.unk2BB--;
        index = gNameSelect.unk2BB;

        sprite_delete(gSpriteHandler, gNameSelect.unk28E[index]);
        func_08004B78(gNameSelect.unk8, gNameSelect.unk298[index]);
    }
}

#include "asm/scenes/name_select/asm_0800f9dc.s"

#include "asm/scenes/name_select/asm_0800fa34.s"

void func_0800FAE0(void) {
    dma3_set(*gNameSelect.unk2D4, BG_TILESET_BASE(0xD800), 0x800, 0x10, 0x100);
}

void func_0800FB14(void) {
    start_pal_interp_col_pal_task(1, 0x10, 0x10, 0, (u32)D_0830C764, (u16 *)D_03004054);
    start_pal_interp_col_pal_task(1, 0x10, 0x10, 0, (u32)D_0830C964, (u16 *)&D_03004054[0x80]);
}

void func_0800FB58(void) {
    start_pal_interp_pal_col_task(1, 0x20, 0x10, D_0830C764, 0, (u16 *)D_03004054);
    start_pal_interp_pal_col_task(1, 0x20, 0x10, D_0830C964, 0, (u16 *)&D_03004054[0x80]);
}

void func_0800FB9C(void) {
    func_08003FB8();
}

#include "asm/scenes/name_select/asm_0800fba8.s"

void func_0800FDC8(void) {
    sprite_handler_set_mem_id(gSpriteHandler, 1);

    if (gNameSelect.unk2BB == 5) {
        gNameSelect.unk27A_2 = 5;
    } else {
        gNameSelect.unk27A_2 = 2;
    }
}

#include "asm/scenes/name_select/asm_0800fe18.s"

#include "asm/scenes/name_select/asm_0800feb0.s"

void func_0800FFA8(u16 value) {
    gNameSelect.unk320 = value;
    func_0800EA5C(value ? gNameSelect.unk31E : gNameSelect.unk31C, 1);
}

u32 func_0800FFE0(void) {
    u32 selected = gNameSelect.unk320;
    u32 previous = selected;

    if (gPressedKeys & DPAD_LEFT) {
        selected = 0;
    }

    if (gPressedKeys & DPAD_RIGHT) {
        selected = 1;
    }

    if (selected != previous) {
        func_0800FFA8(selected);
        play_sound(D_083FBB1C);
    }

    return selected;
}

void func_0801002C(void) {
    func_0800C704(gNameSelect.unk8, D_083A98B8);
    func_0800FFA8(0);
}

void func_0801004C(void) {
    func_0800EB50();
    func_0800C720(gNameSelect.unk8, D_083A98B8);
}

#include "asm/scenes/name_select/asm_0801006c.s"

#include "asm/scenes/name_select/asm_0801010c.s"

void func_080101A0(void) {
    func_0800EB50();
    func_0800C720(gNameSelect.unk8, D_083A98C8);

    sprite_delete(gSpriteHandler, gNameSelect.unk31C);
    sprite_delete(gSpriteHandler, gNameSelect.unk31E);

    gNameSelect.unk31C = 0xFFFF;
    gNameSelect.unk31E = -1;
}

#include "asm/scenes/name_select/asm_08010200.s"

void func_080102A4(void) {
    func_0800EB50();
    func_0800C704(gNameSelect.unk8, D_083A98D0);
}

void func_080102C4(void) {
    func_0800C720(gNameSelect.unk8, D_083A98D0);
}

void func_080102E0(void) {
    if (gPressedKeys & (A_BUTTON | B_BUTTON)) {
        func_080108D8(2);
        play_sound(D_083FBBD0);
    }
}

void func_08010308(void) {
    func_0800C704(gNameSelect.unk8, D_083A98D8);
    func_0800FFA8(0);
}

void func_08010328(void) {
    func_0800EB50();
    func_0800C720(gNameSelect.unk8, D_083A98D8);
}

#include "asm/scenes/name_select/asm_08010348.s"

void func_080103CC(void) {
    gNameSelect.unk324 = sprite_create(gSpriteHandler, D_0830C64C, 0, 0x60, 0x50, 0x800, 1, 0, 0);

    func_0800EA5C(gNameSelect.unk324, 1);
    func_0800C704(gNameSelect.unk8, D_083A98E8);
    func_0800E6F8(-1);
}

void func_08010430(void) {
    func_0800EB50();
    sprite_delete(gSpriteHandler, gNameSelect.unk324);
    func_0800C720(gNameSelect.unk8, D_083A98E8);
}

#include "asm/scenes/name_select/asm_08010468.s"

void func_0801053C(void) {
    func_0800C704(gNameSelect.unk8, D_083A98F0);

    gNameSelect.unk328 = sprite_create(gSpriteHandler, D_0830C67C, (s8)gNameSelect.unk0,
                                       0x38, 0x68, 0x800, 0, 0, 0);

    func_0800FFA8(0);
}

void func_0801059C(void) {
    func_0800EB50();
    func_0800C720(gNameSelect.unk8, D_083A98F0);
    sprite_delete(gSpriteHandler, gNameSelect.unk328);
}

#include "asm/scenes/name_select/asm_080105d4.s"

#include "asm/scenes/name_select/asm_0801068c.s"

void func_080107A8(void) {
    void (*handler)(void);

    scene_set_current_thread(0);

    handler = D_083A9904[gNameSelect.unk318];
    if (handler != NULL) {
        handler();
    }

    gNameSelect.unk27A_2 = 3;
}

void func_080107F0(void) {
    scene_set_current_thread(0);

    gNameSelect.unk308 = 0;
    gNameSelect.unk27A_2 = gNameSelect.unk319;
}

#include "asm/scenes/name_select/asm_0801082c.s"

#include "asm/scenes/name_select/asm_080108d8.s"

void func_08010950(void) {
    void (*handler)(void) = D_083A9934[gNameSelect.unk318];

    if (handler != NULL) {
        handler();
    }
}

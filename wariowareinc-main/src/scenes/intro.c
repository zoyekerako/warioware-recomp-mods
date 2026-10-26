#include "src/scenes/intro.h"
#include "src/beatscript.h"
#include "src/audio.h"

asm(".include \"include/gba.inc\"");

extern u8 D_083AB63C[];
extern void func_080166E4(void);

extern void func_080069F4(void);
extern void func_08006A04(void);
extern void func_08006B90(u16);
extern void func_08006B68(void);
extern void func_08006F28(void);
extern void func_08003E64(void);
extern void func_08000F74(void (*callback)(void));
extern void func_08016D7C(void);

extern u8 D_083AB648[];
extern u8 D_083AB754[];
extern s16 D_030035E0;
extern s16 gCurrentScene;

void func_080166AC(void) {
    if (D_030035E0 != 0) {
        func_08016CBC(D_083AB754);
    }

    if (func_08016D00()) {
        gCurrentScene = gIntro.unk38;
    }
}

void func_080166E4(void) {
    run_func_after_task(start_new_texture_loader((u16)get_current_mem_id(), D_083AB648),
                        set_pause_beatscript_scene, 0);
}

void func_08016708(void) {
    run_func_after_task(start_load_gfx_table_task((u16)get_current_mem_id(), D_083AB63C, 0x3000),
                        func_080166E4, 0);
}

#include "asm/scenes/intro/asm_08016730.s"

#include "asm/scenes/intro/asm_08016798.s"

void func_080167D4(void) {
    if (gGraphicsBuffer.unk854_4) {
        stop_all_soundplayers();
        gBeatscriptScene.threads[0].active = FALSE;
    }
}

#include "asm/scenes/intro/asm_08016808.s"

void func_0801684C(void) {
}

u32 func_08016850(void) {
    if (gIntro.unk8 == 0) {
        return 0;
    }

    if (gGraphicsBuffer.unk854_2 == 0) {
        return 1;
    }

    return 0;
}

void func_08016884(void) {
    func_08007EAC();
    func_08003FB8();
}

#include "asm/scenes/intro/asm_08016894.s"

#include "asm/scenes/intro/asm_0801694c.s"

void func_08016A60(u32 value) {
    gIntro.unk4A_1 = value;
}

void func_08016A7C(u32 value) {
    gIntro.unk4A_2 = value;
}

void func_08016A9C(void) {
    if (D_03006520 == 0x1E) {
        func_0801694C();
    }
}

#include "asm/scenes/intro/asm_08016ab4.s"

void func_08016B14(void) {
    u8 *data = (u8 *)gCurrentSceneData;

    func_08007000(data + 0x3C, *(s16 *)(data + 0x48), 0);
}

void func_08016B34(void) {
    if (D_03006520 == 0x32) {
        func_08016B14();
    }
}

#include "asm/scenes/intro/asm_08016b4c.s"

#include "asm/scenes/intro/asm_08016b88.s"

void func_08016BC4(s32 unused, u16 spriteId, u32 sound) {
    play_sound(sound);
    sprite_set_callback_cel(gSpriteHandler, spriteId, -1);
}

#include "asm/scenes/intro/asm_08016bf0.s"

#include "asm/scenes/intro/asm_08016c24.s"

#include "asm/scenes/intro/asm_08016c60.s"

void func_08016CB0(s32 unused1, s32 unused2, u32 sound) {
    play_sound(sound);
}

/* Scene entry point, shared by every beatscript scene: reset the engines, hook
   up the per-frame callback and start the scene's subscene list. */
void func_08016CBC(const struct SubScene *subScene) {
    u32p args[4];

    func_080069F4();
    func_08006A04();
    func_08006B90(0);
    func_08006B68();
    func_08006F28();
    func_08003E64();
    func_08000F74(func_08016D7C);

    args[0] = (u32p)subScene;
    args[1] = 0;
    start_beatscript_scene(0);
    set_beatscript_subscenes(args);
}

/* One frame of the intro: flush, advance every pool, then either keep the
   scene running or hand over to the teardown. */
u32 func_08016D00(void) {
    flush_graphics_buffer();
    trigger_pending_dma3();
    update_paused_beatscript_scene();
    task_pool_update_constant();
    task_pool_update_delayed();
    update_active_beatscript_scene();

    if (beatscript_scene_is_inactive()) {
        func_08016D3C();
        return 1;
    }

    func_08006F68();
    func_08006B00();
    func_080041B4();

    return 0;
}

#include "asm/scenes/intro/asm_08016d3c.s"

void func_08016D7C(void) {
    func_08003EB0();
}

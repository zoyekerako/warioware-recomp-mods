#include "src/scenes/init.h"

asm(".include \"include/gba.inc\"");

/* The scene the game starts in: it hands over to the title, the intro or the
   language select depending on what func_0800DBA0 reports. */
void func_0800DB04(void) {
    if (D_030035E0 != 0) {
        func_0800DB5C();
        return;
    }

    switch (func_0800DBA0()) {
        case 1:
            gCurrentScene = GLOBAL_SCENE_TITLE;
            break;
        case 2:
            gCurrentScene = GLOBAL_SCENE_INTRO;
            break;
        case 3:
            gCurrentScene = GLOBAL_SCENE_LANGUAGE_SELECT;
            break;
    }
}

/* Starts the scene: reset the loader and fade the whole palette in from white. */
void func_0800DB5C(void) {
    gInitScene.state = 0;
    gInitScene.loader.unk0_1 = FALSE;

    pal_interp_init_masked(&gInitScene.fade, 0x20, gWhitePalette,
                           (u16 *)PaletteRAMBase, gInitScene.paletteBackup,
                           0xFFFF, 0);
}

/* Runs the scene: first it waits for the graphics loader, then it blacks the
   display out and fades the palette back in.  Reports 2 once the fade is done,
   which sends the game on to the intro. */
/* Runs the scene.  While state is 0 it drives the graphics loader one step per
   frame; once that reports done it blacks the display out and fades the palette
   back in.  Reports 2 when the fade has finished, which sends the game on to
   the intro. */
u32 func_0800DBA0(void) {
    struct InitSceneData *scene;
    struct GfxTableTask *loader;

    scene = &gInitScene;

    if (scene->state == 0) {
        loader = &scene->loader;
        if (func_08002468(loader)) {
            func_080021C8(loader);
        } else {
            scene->state = 1;
        }
    } else {
        pal_interp_save_output_backup(&scene->fade);

        switch (scene->state) {
            case 1:
                REG_DISPCNT = 0;
                scene->state = 2;
                break;
            case 2:
                if (!scene->fade.isActive) {
                    return 2;
                }
                break;
        }

        pal_interp_update_masked(&scene->fade);
    }

    return 0;
}

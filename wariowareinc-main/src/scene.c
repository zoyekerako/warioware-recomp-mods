#include "scene.h"
#include "lib_sprite.h"
#include "src/audio.h"

/* Returns a pointer; without this the implicit int truncates it. */
extern void *func_0800A228(s32 size);

asm(".include \"include/gba.inc\"");

void scene_set_video_mode(s32 videoMode) {
    u16 *displayControls = &gGraphicsBuffer.DISPCNT;

    *displayControls &= ~DISPCNT_VIDEO_MODE_MASK;
    *displayControls |= videoMode;
}

void scene_show_bg_layer(s32 layer) {
    gGraphicsBuffer.DISPCNT |= (DISPCNT_DISPLAY_BG(layer));
}

void scene_hide_bg_layer(s32 layer) {
    gGraphicsBuffer.DISPCNT &= ~(DISPCNT_DISPLAY_BG(layer));
}

void scene_set_bg_layer_pos(s32 layer, s16 x, s16 y) {
    struct Vector2 *bgOfs = gGraphicsBuffer.BG_OFS;

    bgOfs[layer].x = x;
    bgOfs[layer].y = y;
}

void scene_set_bg_layer_controls(s32 layer, s32 tileset, s32 map, s32 params) {
    u16 *bgControls = gGraphicsBuffer.BG_CNT;

    bgControls[layer] = BGCNT_TILEDATA_ADDR(tileset) | BGCNT_TILEMAP_ADDR(map) | params;
}

void scene_set_bg_layer_priority(s32 layer, s32 priority) {
    u16 *bgControls = gGraphicsBuffer.BG_CNT;

    bgControls[layer] &= ~BGCNT_PRIORITY_MASK;
    bgControls[layer] |= priority;
}

void scene_set_bg_layer_display(s32 layer, s32 show, s32 x, s32 y, s32 tileset, s32 map, s32 params) {
    scene_set_bg_layer_pos(layer, x, y);
    scene_set_bg_layer_controls(layer, tileset, map, params);

    if (show) {
        scene_show_bg_layer(layer);
    } else {
        scene_hide_bg_layer(layer);
    }
}

void scene_show_obj_layer(void) {
    u16 *displayControls = &gGraphicsBuffer.DISPCNT;

    *displayControls |= DISPCNT_DISPLAY_OAM;
}

void scene_hide_obj_layer(void) {
    u16 *displayControls = &gGraphicsBuffer.DISPCNT;

    *displayControls &= ~DISPCNT_DISPLAY_OAM;
}

/* MOSAIC bits 8-15 hold the object mosaic; a negative size leaves it alone. */
void scene_set_obj_mosaic_size(s16 x, s16 y) {
    if (x >= 0) {
        gGraphicsBuffer.unk48 = (gGraphicsBuffer.unk48 & 0xF0FF) | (x << 8);
    }

    if (y >= 0) {
        gGraphicsBuffer.unk48 = (gGraphicsBuffer.unk48 & 0x0FFF) | (y << 12);
    }
}
/* MOSAIC bits 0-7 hold the background mosaic. */
void scene_set_bg_mosaic_size(s16 x, s16 y) {
    if (x >= 0) {
        gGraphicsBuffer.unk48 = (gGraphicsBuffer.unk48 & 0xFFF0) | (u16)x;
    }

    if (y >= 0) {
        gGraphicsBuffer.unk48 = (gGraphicsBuffer.unk48 & 0xFF0F) | (y << 4);
    }
}

s32 scene_set_sprite_motion_indefinite(s32 spriteId, s16 arg1, s32 arg2, s32 arg3, s32 arg4) {
    struct SceneSpriteMotion motion;

    motion.spriteId = spriteId;
    motion.unk2 = arg1;
    motion.unk4 = arg2;
    motion.unk6 = arg3;
    motion.unk8 = arg4;

    return start_new_task((u16)get_current_mem_id(), D_083A4A80, &motion, 0, 0);
}
s32 scene_move_sprite_indefinite(s16 spriteId, u16 arg1, u16 arg2) {
    s16 x;
    s16 y;

    get_sprite_xy(spriteId, &x, &y);

    return scene_set_sprite_motion_indefinite(spriteId, x, y, (s16)arg1, (s16)arg2);
}
s32 scene_set_sprite_motion_decelerate(s32 spriteId, s16 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5) {
    struct SceneSpriteMotion motion;

    motion.spriteId = spriteId;
    motion.unk2 = arg1;
    motion.unk4 = arg2;
    motion.unk6 = arg3;
    motion.unk8 = arg4;
    motion.unkA = arg5;

    return start_new_task((u16)get_current_mem_id(), D_083A4A90, &motion, 0, 0);
}
s32 scene_move_sprite_decelerate(s16 spriteId, u16 arg1, u16 arg2, u16 arg3) {
    s16 x;
    s16 y;

    get_sprite_xy(spriteId, &x, &y);

    return scene_set_sprite_motion_decelerate(spriteId, x, y, (s16)arg1, (s16)arg2, (s16)arg3);
}
s32 scene_set_sprite_motion_accelerate(s32 spriteId, s16 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5, s16 arg6) {
    struct SceneSpriteMotionAccel motion;

    motion.spriteId = spriteId;
    motion.unk2 = arg1;
    motion.unk4 = arg2;
    motion.unk6 = arg3;
    motion.unk8 = arg4;
    motion.unkA = arg5;
    motion.unkC = arg6;

    return start_new_task((u16)get_current_mem_id(), D_083A4AA0, &motion, 0, 0);
}
s32 scene_move_sprite_accelerate(s16 spriteId, u16 arg1, u16 arg2, u16 arg3, u16 arg4) {
    s16 x;
    s16 y;

    get_sprite_xy(spriteId, &x, &y);

    return scene_set_sprite_motion_accelerate(spriteId, x, y, (s16)arg1, (s16)arg2, (s16)arg3,
                                              (s16)arg4);
}
s32 scene_set_sprite_motion_lerp(s32 spriteId, s16 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5) {
    struct SceneSpriteMotion motion;

    motion.spriteId = spriteId;
    motion.unk2 = arg1;
    motion.unk4 = arg2;
    motion.unk6 = arg3;
    motion.unk8 = arg4;
    motion.unkA = arg5;

    return start_new_task((u16)get_current_mem_id(), D_083A4AB0, &motion, 0, 0);
}
s32 scene_move_sprite_lerp(s16 spriteId, u16 arg1, u16 arg2, u16 arg3) {
    s16 x;
    s16 y;

    get_sprite_xy(spriteId, &x, &y);

    return scene_set_sprite_motion_lerp(spriteId, x, y, (s16)arg1, (s16)arg2, arg3);
}
s32 scene_set_sprite_motion_sine_osc(s32 spriteId, u8 arg1, s32 arg2, s32 arg3, s16 arg4,
                                     s16 arg5, s16 arg6, s16 arg7, u16 arg8) {
    struct SceneSpriteMotionSine motion;

    motion.spriteId = spriteId;
    motion.unk2 = arg1;
    motion.unk4 = arg2;
    motion.unk6 = arg3;
    motion.unk8 = arg4;
    motion.unkA = arg5;
    motion.unkC = arg6;
    motion.unkE = arg7;
    motion.unk10 = arg8;

    return start_new_task((u16)get_current_mem_id(), D_083A4AC0, &motion, 0, 0);
}
s32 scene_move_sprite_sine_osc(s16 spriteId, u8 arg1, u16 arg2, u16 arg3, u16 arg4, u16 arg5) {
    s16 x;
    s16 y;

    get_sprite_xy(spriteId, &x, &y);

    return scene_set_sprite_motion_sine_osc(spriteId, arg1, x, y, 0,
                                            (s16)arg2, (s16)arg3, (s16)arg4, arg5);
}
/* Register allocation for the seven live shorts does not reproduce yet:
   the ROM parks the last argument in IP, agbcc keeps it in R5. */
#include "asm/scene/asm_0800c430.s"
s32 scene_move_sprite_sine_vel(s16 spriteId, s32 arg1, u16 arg2, u16 arg3, u16 arg4) {
    s16 x;
    s16 y;

    get_sprite_xy(spriteId, &x, &y);

    return scene_set_sprite_motion_sine_vel(spriteId, arg1, x, y, (s16)arg2, (s16)arg3, arg4);
}
s32 scene_set_sprite_motion_sine_wave(s32 spriteId, s16 arg1, s32 arg2, s32 arg3, s32 arg4,
                                      s32 arg5, u16 arg6) {
    struct SceneSpriteMotionAccel motion;

    motion.spriteId = spriteId;
    motion.unk2 = arg1;
    motion.unk4 = arg2;
    motion.unk6 = arg3;
    motion.unk8 = arg4;
    motion.unkA = arg5;
    motion.unkC = arg6;

    return start_new_task((u16)get_current_mem_id(), D_083A4AE0, &motion, 0, 0);
}
s32 scene_move_sprite_sine_wave(s16 spriteId, u16 arg1, u16 arg2, u16 arg3, u16 arg4) {
    s16 x;
    s16 y;

    get_sprite_xy(spriteId, &x, &y);

    return scene_set_sprite_motion_sine_wave(spriteId, x, y, (s16)arg1, (s16)arg2, (s16)arg3, arg4);
}
void func_0800C610(struct SceneObjectDef *def) {
    *def->sprite = -1;
}
/* Creates the object's sprite if its slot is still empty.  The animation is
   picked by language and then adjusted by one of three helpers. */
void func_0800C61C(void *scene, struct SceneObjectDef *def) {
    struct Animation *anim;

    if (*def->sprite < 0) {
        struct Animation *base = def->anims[get_current_language()];

        switch (def->unkA) {
            case 0:
                anim = func_080049BC(scene, base);
                break;
            case 1:
                anim = func_08004A30(scene, base);
                break;
            case 2:
                anim = func_08004A74(scene, base);
                break;
        }

        *def->sprite = sprite_create(gSpriteHandler, anim, 0, def->x, def->y, def->unk8, 0, 0, 0);
    }
}
void func_0800C69C(void *scene, struct SceneObjectDef *def) {
    if (*def->sprite >= 0) {
        s32 data = sprite_get_data(gSpriteHandler, *def->sprite, 7);

        sprite_delete(gSpriteHandler, *def->sprite);
        func_08004B78(scene, data);
        *def->sprite = -1;
    }
}
void func_0800C6E4(struct SceneObjectDef **list) {
    while (*list != NULL) {
        *(*list)->sprite = -1;
        list++;
    }
}
/* Applies every entry of a NULL-terminated list to the scene. */
void func_0800C704(void *scene, struct SceneObjectDef **list) {
    while (*list != NULL) {
        func_0800C61C(scene, *list++);
    }
}

void func_0800C720(void *scene, struct SceneObjectDef **list) {
    while (*list != NULL) {
        func_0800C69C(scene, *list++);
    }
}
struct SceneSpriteMotion *func_0800C73C(void) {
    struct SceneSpriteMotion *motion = func_0800A228(sizeof(struct SceneSpriteMotion));

    motion->spriteId = func_0800A218();
    motion->unk2 = 0x100;
    motion->unk4 = 0;
    motion->unk6 = 0;
    motion->unk8 = 0x100;

    return motion;
}
void func_0800C764(void *owner) {
    func_08001B28(*(s16 *)owner);
    mem_heap_dealloc(owner);
}

void scene_show_sprite(s32 id) {
    sprite_set_visible(gSpriteHandler, gCurrentSceneSpritePool[id], TRUE);
}

void scene_hide_sprite(s32 id) {
    sprite_set_visible(gSpriteHandler, gCurrentSceneSpritePool[id], FALSE);
}

struct SoundPlayer *scene_play_sound_to_tempo_and_pitch(struct SongHeader *sound) {
    struct SoundPlayer *player = play_sound(sound);

    set_soundplayer_speed(player, (u16)func_0800A044());
    set_soundplayer_pitch(player, gBeatscriptScene.musicPitchSrc1);

    return player;
}

struct SoundPlayer *scene_play_sound_to_tempo(struct SongHeader *sound) {
    struct SoundPlayer *player = play_sound(sound);

    set_soundplayer_speed(player, (u16)func_0800A044());

    return player;
}

/* Picks one entry at random from a NULL-terminated sound list. */
struct SoundPlayer *scene_play_random_sound(struct SongHeader **list) {
    if (list != NULL) {
        u32 count = 0;

        while (list[count] != NULL) {
            count++;
        }

        if (count != 0) {
            struct SongHeader *sound = list[(u16)get_random_range((u16)count)];

            scene_set_music(sound);

            return sound_player_table[sound->soundPlayer].soundPlayer;
        }
    }

    return NULL;
}

struct SoundPlayer *func_0800C874(struct SongHeader **list) {
    if (list != NULL) {
        u32 count = 0;

        while (list[count] != NULL) {
            count++;
        }

        if (count != 0) {
            return play_sound(list[(u16)get_random_range((u16)count)]);
        }
    }

    return NULL;
}

struct SoundPlayer *scene_play_random_sound_to_tempo_and_pitch(struct SongHeader **list) {
    if (list != NULL) {
        u32 count = 0;

        while (list[count] != NULL) {
            count++;
        }

        if (count != 0) {
            return scene_play_sound_to_tempo_and_pitch(list[(u16)get_random_range((u16)count)]);
        }
    }

    return NULL;
}

struct SoundPlayer *func_0800C8F4(struct SongHeader **list) {
    if (list != NULL) {
        u32 count = 0;

        while (list[count] != NULL) {
            count++;
        }

        if (count != 0) {
            return scene_play_sound_to_tempo(list[(u16)get_random_range((u16)count)]);
        }
    }

    return NULL;
}

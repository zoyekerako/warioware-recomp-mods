/* ENTWURF -- noch nicht byte-genau.  Nicht im Build.
 *
 * func_0800A7D4 ist der Opcode-Dispatcher des Beatscripts: 1574 Instruktionen,
 * 105 Faelle ueber eine dichte Sprungtabelle (0x00..0x9C).  Der Entwurf hier
 * gibt die Struktur bereits wieder -- verglichen auf Instruktionsebene
 * (Mnemonics, ohne Register) bleiben 20 Abweichungsstellen bei 1350
 * Instruktionen, und die Gesamtzahl stimmt auf drei Instruktionen genau.
 *
 * Was noch fehlt:
 *
 * 1. Der Prolog.  Das ROM legt `threadIndex` auf den Stack ([SP,#0x1C]) und
 *    haelt `isSceneMode` in SL; dieser Entwurf macht es umgekehrt und laesst
 *    R5 belegt.  Dadurch ist der Rahmen 4 Byte kleiner (0x1C statt 0x20) und
 *    alles danach verschoben.  Das ROM haelt sechs langlebige Werte im
 *    Register und laesst R5 als Kratzregister frei -- der Entwurf haelt
 *    sieben.  Es fehlt also irgendwo ein Zwischenwert, der Druck erzeugt.
 *
 * 2. Die Faelle 0x17..0x24, 0x25 und 0x34 weichen strukturell ab; im ROM
 *    laufen mehrere Zweige in gemeinsame Endstuecke zusammen (_0800B24E,
 *    _0800B2DC, _0800B2F4, _0800B19A, _0800B1EE), was GCC durch
 *    Cross-Jumping selbst erzeugt, hier aber nicht tut.
 *
 * Register im ROM: R4 = Sprite-Pool, R6 = param1, R7 = param2, R8 = param3,
 * SB = thread, SL = isSceneMode, [SP,#0x1C] = threadIndex.
 *
 * Werkzeug zum Weiterarbeiten:
 *   port/tools/verify_file.py src/beatscript.c
 * und der Instruktionsvergleich aus der README ("Der Dispatcher").
 */

/* One beatscript instruction for one thread.  The command byte selects from a
   dense table of about 0x9C opcodes; anything above that, or an opcode with no
   body, simply returns. */
void func_0800A7D4(u32 threadIndex) {
    struct BeatscriptThread *thread;
    struct Beatscript *cmd;
    u32 command;
    u32 param1;
    u32p param2;
    u32p param3;
    u32 isSceneMode;
    s16 *pool;
    u32 i;

    isSceneMode = FALSE;
    if (gBeatscriptScene.mode == 1) {
        isSceneMode = TRUE;
    }

    sprite_handler_set_mem_id(gSpriteHandler, threadIndex + 1);

    thread = &gBeatscriptScene.threads[threadIndex];
    gCurrentSceneVariable = &gBeatscriptScene.localVariables[threadIndex];
    gCurrentSceneSpritePool = gBeatscriptScene.threads[threadIndex].sprites;
    pool = gCurrentSceneSpritePool;

    cmd = (struct Beatscript *)thread->currentCmd;
    thread->currentCmd = cmd + 1;

    param1 = *(u32 *)cmd >> 8;
    param2 = cmd->param2;
    param3 = cmd->param3;
    command = cmd->command;

    switch (command) {
        case 0x00:
            thread->active = FALSE;
            break;

        case 0x01:
            ((void (*)(struct BeatscriptLocalData *, u32p))param2)(
                &D_030049F0[threadIndex], param3);
            break;

        case 0x02:
            ((void (*)(u32p))param2)(param3);
            break;

        case 0x03:
            *(u32 *)param2 = param3;
            break;

        case 0x04:
            *(u16 *)param2 = param3;
            break;

        case 0x05:
            *(u8 *)param2 = param3;
            break;

        case 0x06:
            thread->timeUntilNext += param3 << 8;
            break;

        case 0x07:
            if (isSceneMode) {
                gGameplayData.unk173 = 1;
            }
            break;

        case 0x08:
            if (isSceneMode) {
                gGameplayData.unk173 = 2;
            }
            break;

        case 0x2E:
            scene_play_music((struct SongHeader *)param3);
            break;

        case 0x0A:
            play_sound((struct SongHeader *)param3);
            break;

        case 0x0B:
            if (isSceneMode && gGameplayData.unkC != 0) {
                scene_set_music(gGameplayData.unkC);
            }
            break;

        case 0x0C:
            if (isSceneMode) {
                func_0800A128(2);
            }
            break;

        case 0x1F:
            if (isSceneMode) {
                if (gGameplayData.unk228 == 0) {
                    scene_set_music((struct SongHeader *)param3);
                } else if (gGameplayData.unk221 == 0) {
                    scene_set_music((struct SongHeader *)
                        gGameplayData.unk224->unk4[gGameplayData.unk220]->unk4);
                }
            }
            break;

        case 0x20:
            if (isSceneMode) {
                if (gGameplayData.unk228 == 0) {
                    scene_set_music((struct SongHeader *)param3);
                } else {
                    scene_set_music((struct SongHeader *)
                        gGameplayData.unk224->unk4[gGameplayData.unk220]->unkC);
                }
            }
            break;

        case 0x0D:
            if (isSceneMode && gGameplayData.unk228 == 0 && gGameplayData.unk178 == 0) {
                scene_set_music((struct SongHeader *)param3);
            }
            break;

        case 0x0E:
            if (isSceneMode && gGameplayData.unk228 == 0 && gGameplayData.unk178 == 1) {
                scene_set_music((struct SongHeader *)param3);
            }
            break;

        case 0x09:
            scene_set_music((struct SongHeader *)param3);
            break;

        case 0x10:
            thread->jumpStack[thread->stackCounter] = thread->currentCmd;
            thread->currentCmd = (struct Beatscript *)param2;
            thread->stackCounter = thread->stackCounter + 1;
            break;

        case 0x11:
            thread->stackCounter = thread->stackCounter - 1;
            thread->currentCmd = thread->jumpStack[thread->stackCounter];
            break;

        case 0x12:
            if (isSceneMode && gGameplayData.unk178 == 0) {
                thread->currentCmd = (struct Beatscript *)param2;
            }
            break;

        case 0x13:
            if (isSceneMode && gGameplayData.unk178 == 1) {
                thread->currentCmd = (struct Beatscript *)param2;
            }
            break;

        case 0x14:
            thread->currentCmd = (struct Beatscript *)param2;
            break;

        case 0x15:
            thread->loopStart = thread->currentCmd;
            break;

        case 0x16:
            if (!gBeatscriptScene.bypassLoops) {
                thread->currentCmd = thread->loopStart;
            }
            break;

        case 0x17:
            if (*(u32 *)param2 != param3) {
                thread->currentCmd = beatscript_skip_to_else_or_endif(
                    (struct Beatscript *)thread->currentCmd);
            }
            break;

        case 0x18:
            if (*(u16 *)param2 != (u16)param3) {
                thread->currentCmd = beatscript_skip_to_else_or_endif(
                    (struct Beatscript *)thread->currentCmd);
            }
            break;

        case 0x19:
            if (*(u8 *)param2 != (u8)param3) {
                thread->currentCmd = beatscript_skip_to_else_or_endif(
                    (struct Beatscript *)thread->currentCmd);
            }
            break;

        case 0x1A:
            if (*(u32 *)param2 == param3) {
                thread->currentCmd = beatscript_skip_to_else_or_endif(
                    (struct Beatscript *)thread->currentCmd);
            }
            break;

        case 0x1B:
            if (*(u16 *)param2 == (u16)param3) {
                thread->currentCmd = beatscript_skip_to_else_or_endif(
                    (struct Beatscript *)thread->currentCmd);
            }
            break;

        case 0x1C:
            if (*(u8 *)param2 == (u8)param3) {
                thread->currentCmd = beatscript_skip_to_else_or_endif(
                    (struct Beatscript *)thread->currentCmd);
            }
            break;

        case 0x23:
            if ((*(u32 *)param2 & param3) == 0) {
                thread->currentCmd = beatscript_skip_to_else_or_endif(
                    (struct Beatscript *)thread->currentCmd);
            }
            break;

        case 0x24:
            if ((*(u32 *)param2 & param3) != 0) {
                thread->currentCmd = beatscript_skip_to_else_or_endif(
                    (struct Beatscript *)thread->currentCmd);
            }
            break;

        case 0x1D:
            thread->currentCmd = beatscript_skip_to_endif(
                (struct Beatscript *)thread->currentCmd);
            break;

        case 0x21:
            func_080F3100(gBeatscriptScene.musicPlayer, (u16)param3);
            break;

        case 0x22:
            func_080F30E0(gBeatscriptScene.musicPlayer, (u16)param3);
            break;

        case 0x25:
            for (i = 0; i <= 3; i++) {
                ((u8 *)&gBeatscriptScene.globalVariable)[i] = ((u8 *)param2)[i];
            }
            gBeatscriptScene.globalVariable =
                (gBeatscriptScene.globalVariable >> (param3 >> 27)) & (param3 & 0x07FFFFFF);
            break;

        case 0x26:
            gBeatscriptScene.globalVariable =
                (((u32 (*)(void))param2)() >> (param3 >> 27)) & (param3 & 0x07FFFFFF);
            break;

        case 0x27:
            thread->currentCmd = beatscript_skip_to_case_or_endswitch(
                (struct Beatscript *)thread->currentCmd, *(u32 *)param3);
            break;

        case 0x28:
            thread->currentCmd = beatscript_skip_to_case_or_endswitch(
                (struct Beatscript *)thread->currentCmd, *(u16 *)param3);
            break;

        case 0x29:
            thread->currentCmd = beatscript_skip_to_case_or_endswitch(
                (struct Beatscript *)thread->currentCmd, *(u8 *)param3);
            break;

        case 0x2C:
            thread->currentCmd = beatscript_skip_to_default(
                (struct Beatscript *)thread->currentCmd);
            break;

        case 0x2D:
            load_gfx_table((const struct GraphicsTable *)param2);
            break;

        case 0x3E:
            start_load_gfx_table_task((u16)get_current_mem_id(),
                                      (const struct GraphicsTable *)param2, param3);
            break;

        case 0x2F:
            if (gGameplayData.unk270 == 0 || gGameplayData.currentLives > 3) {
                thread->currentCmd = beatscript_skip_to_else_or_endif(
                    (struct Beatscript *)thread->currentCmd);
            }
            break;

        case 0x30:
            set_beatscript_speed((u16)param3);
            break;

        case 0x31:
            scene_set_music_pitch_env((s16)param3);
            break;

        case 0x32:
            scene_set_music_pitch((s16)param3);
            break;

        case 0x33: {
            u32 duration = param3 & 0x7FFFFFFF;

            if ((s32)param3 < 0) {
                duration = ticks_to_frames(duration);
            }

            interp_lcd_blend_mode((u16)get_current_mem_id(), param2, duration, param1);
            break;
        }

        case 0x34: {
            u32 duration = (param1 >> 9) & 0x3FFF;
            u32 totalPalettes;
            u16 *dest;

            if ((param1 & 0x800000) != 0) {
                duration = ticks_to_frames(duration);
            }

            totalPalettes = (param1 & 0xF) + 1;
            dest = (u16 *)&D_03004054[((param1 >> 4) & 0x1F) * 8];

            if (param2 > 0xFFFF) {
                start_pal_interp_col_pal_task((u16)get_current_mem_id(), (u8)duration,
                                              totalPalettes, (const u16 *)param2,
                                              param3, dest);
            } else if (param3 > 0xFFFF) {
                start_pal_interp_pal_col_task((u16)get_current_mem_id(), (u8)duration,
                                              totalPalettes, (const u16 *)param2,
                                              param3, dest);
            } else {
                start_pal_interp_pal_pal_task((u16)get_current_mem_id(), (u8)duration,
                                              totalPalettes, (const u16 *)param2,
                                              param3, dest);
            }
            break;
        }

        case 0x35: {
            u32 layer = param1 & 3;

            if ((param1 & 4) != 0) {
                scene_show_bg_layer(layer);
            } else {
                scene_hide_bg_layer(layer);
            }

            gGraphicsBuffer.BG_OFS[layer].x = param2;
            gGraphicsBuffer.BG_OFS[layer].y = (s32)param2 >> 16;
            gGraphicsBuffer.BG_CNT[layer] = param3;
            break;
        }

        case 0x36:
            *(u32 *)param2 += param3;
            break;

        case 0x37:
            *(u16 *)param2 += param3;
            break;

        case 0x38:
            *(u8 *)param2 += param3;
            break;

        case 0x39: {
            struct BeatscriptMoveBgArgs args;

            args.unk0_1 = param1;
            args.unk0_3 = ticks_to_frames(param1 >> 2);
            args.unk4 = param2;
            args.unk8 = (s16)param3;
            args.unkC = (s32)param3 >> 16;

            start_new_task((u16)get_current_mem_id(), &D_083A4AF0, &args, NULL, 0);
            break;
        }

        case 0x3B:
            if (func_080007C0(param2) == 0) {
                thread->currentCmd = beatscript_skip_to_else_or_endif(
                    (struct Beatscript *)thread->currentCmd);
            }
            break;

        case 0x3C:
            if (func_080007C0(param2) != 0) {
                thread->currentCmd = beatscript_skip_to_else_or_endif(
                    (struct Beatscript *)thread->currentCmd);
            }
            break;

        case 0x3D:
            func_0800CE90(gBeatscriptScene.scriptBPM);
            break;

        case 0x3F:
            func_08004130((u16)param1, (u16)param2, (u16)param3);
            break;

        case 0x40:
            gGraphicsBuffer.DISPCNT = (gGraphicsBuffer.DISPCNT & 0xFFF8) | param2;
            break;

        case 0x41:
            if (param1 != 0) {
                func_08006C40((u16)ticks_to_frames(param2), (u16)param3);
            } else {
                func_08006BB8((u16)ticks_to_frames(param2), (u16)param3);
            }
            break;

        case 0x42:
            if (gGameplayData.unk238 != 0) {
                thread->currentCmd = beatscript_skip_to_else_or_endif(
                    (struct Beatscript *)thread->currentCmd);
            }
            break;

        case 0x43:
            func_0800CC9C(param2, param3);
            break;

        case 0x47:
            func_0800CD94(param2, param3);
            break;

        case 0x44:
            if (param1 != 0) {
                if (gGameplayData.unk178 == 0) {
                    thread->currentCmd = beatscript_skip_to_else_or_endif(
                        (struct Beatscript *)thread->currentCmd);
                }
            } else if (gGameplayData.unk178 == 1) {
                thread->currentCmd = beatscript_skip_to_else_or_endif(
                    (struct Beatscript *)thread->currentCmd);
            }
            break;

        case 0x45: {
            s32 difference;

            switch (param1 & 0xF) {
                case 1:
                    difference = *(u16 *)param2 - (u16)param3;
                    break;
                case 2:
                    difference = *(u8 *)param2 - (u8)param3;
                    break;
                default:
                    difference = *(u32 *)param2 - param3;
                    break;
            }

            if (difference != 0) {
                if ((param1 & 0x10) == 0) {
                    thread->currentCmd = beatscript_skip_to_loop_end(
                        (struct Beatscript *)thread->currentCmd);
                }
            } else if ((param1 & 0x10) != 0) {
                thread->currentCmd = beatscript_skip_to_loop_end(
                    (struct Beatscript *)thread->currentCmd);
            }
            break;
        }

        case 0x46:
            thread->currentCmd = beatscript_rewind_to_loop_begin(
                (struct Beatscript *)thread->currentCmd - 1);
            break;

        case 0x49:
            func_0800A2D8(1, ticks_to_frames(param3), D_030064E8, param2);
            break;

        case 0x4A:
            *(u32 *)param2 |= 1 << param3;
            break;

        case 0x4B:
            *(u8 *)param2 |= 1 << param3;
            break;

        case 0x4C:
            *(u16 *)param2 |= 1 << param3;
            break;

        case 0x4D:
            *(u32 *)param2 &= ~(1 << param3);
            break;

        case 0x4E:
            *(u8 *)param2 &= ~(1 << param3);
            break;

        case 0x4F:
            *(u16 *)param2 &= ~(1 << param3);
            break;

        case 0x50:
            if (save_is_stage_unlocked(param1) == 0) {
                thread->currentCmd = beatscript_skip_to_else_or_endif(
                    (struct Beatscript *)thread->currentCmd);
            }
            break;

        case 0x51:
            if (save_is_stage_beaten(param1) == 0) {
                thread->currentCmd = beatscript_skip_to_else_or_endif(
                    (struct Beatscript *)thread->currentCmd);
            }
            break;

        case 0x80:
            sprite_set_anim(gSpriteHandler, pool[param2 & 0xFF],
                            (struct Animation *)param3,
                            (s8)(param2 >> 8),
                            (s32)(param2 << 12) >> 28,
                            (s8)(param2 >> 20),
                            param2 >> 28);
            break;

        case 0x81:
            sprite_set_anim_cel(gSpriteHandler, pool[param2],
                                (s8)param3);
            break;

        case 0x82:
            sprite_set_playback(gSpriteHandler, pool[param2],
                                (s32)(param3 << 28) >> 28,
                                (s32)(param3 << 20) >> 24,
                                (param3 >> 12) & 0xF);
            break;

        case 0x83:
            sprite_set_x_y_z(gSpriteHandler,
                             pool[param2 & 0xFFFF],
                             (s16)param3, (s32)param3 >> 16, param2 >> 16);
            break;

        case 0x84:
            sprite_set_x_y(gSpriteHandler, pool[param2],
                           (s16)param3, (s32)param3 >> 16);
            break;

        case 0x85:
            sprite_set_z(gSpriteHandler, pool[param2],
                         (u16)param3);
            break;

        case 0x94: {
            s16 id = pool[param1];

            sprite_set_x_y(gSpriteHandler, id,
                           (u16)(sprite_get_data(gSpriteHandler, id, 4) + param2),
                           sprite_get_data(gSpriteHandler, id, 5) + param3);
            break;
        }

        case 0x95: {
            s16 id = pool[param1];

            sprite_set_z(gSpriteHandler, id,
                         (u16)(sprite_get_data(gSpriteHandler, id, 6) + param2));
            break;
        }

        case 0x86:
            sprite_set_visible(gSpriteHandler, pool[param2],
                               (u16)param3);
            break;

        case 0x87:
            sprite_set_x_y(gSpriteHandler, pool[param2],
                           ((s16 *)param3)[0], ((s16 *)param3)[1]);
            break;

        case 0x88:
            sprite_set_base_palette(gSpriteHandler, pool[param2],
                                    (s8)param3);
            break;

        case 0x89:
            sprite_set_base_tile(gSpriteHandler, pool[param2],
                                 (s16)param3);
            break;

        case 0x93:
            sprite_attr_set(gSpriteHandler, pool[param2], param3);
            break;

        case 0x8A:
            thread->currentTaskID = scene_move_sprite_indefinite(
                pool[param1], (s16)param2, (s32)param2 >> 16);
            break;

        case 0x8B:
            thread->currentTaskID = scene_move_sprite_decelerate(
                pool[param1], (s16)param2, (s32)param2 >> 16,
                (s16)param3);
            break;

        case 0x8C:
            thread->currentTaskID = scene_move_sprite_accelerate(
                pool[param1], (s16)param2, (s32)param2 >> 16,
                (s16)param3, (s32)param3 >> 16);
            break;

        case 0x8D:
            thread->currentTaskID = scene_move_sprite_lerp(
                pool[param1], (s16)param2, (s32)param2 >> 16,
                (u16)ticks_to_frames((u16)param3));
            break;

        case 0x8E:
            thread->currentTaskID = scene_move_sprite_sine_vel(
                pool[param1], param3 >> 16,
                (s16)param2, (s32)param2 >> 16,
                (u16)ticks_to_frames((u16)param3));
            break;

        case 0x8F:
            thread->currentTaskID = scene_move_sprite_sine_wave(
                pool[param1], (s16)param2, (s32)param2 >> 16,
                (s32)param3 >> 16, (u16)ticks_to_frames((u16)param3));
            break;

        case 0x96:
            thread->currentTaskID = scene_move_sprite_decelerate(
                pool[param1],
                ((s16 *)param2)[0], ((s16 *)param2)[1], (s16)param3);
            break;

        case 0x97:
            thread->currentTaskID = scene_move_sprite_accelerate(
                pool[param1],
                ((s16 *)param2)[0], ((s16 *)param2)[1],
                (s16)param3, (s32)param3 >> 16);
            break;

        case 0x98:
            thread->currentTaskID = scene_move_sprite_lerp(
                pool[param1],
                ((s16 *)param2)[0], ((s16 *)param2)[1],
                (u16)ticks_to_frames((u16)param3));
            break;

        case 0x99:
            thread->currentTaskID = scene_move_sprite_sine_vel(
                pool[param1], param3 >> 16,
                ((s16 *)param2)[0], ((s16 *)param2)[1],
                (u16)ticks_to_frames((u16)param3));
            break;

        case 0x9A:
            thread->currentTaskID = scene_move_sprite_sine_wave(
                pool[param1],
                ((s16 *)param2)[0], ((s16 *)param2)[1],
                (s32)param3 >> 16, (u16)ticks_to_frames((u16)param3));
            break;

        case 0x90:
            run_func_after_task(thread->currentTaskID, (TaskFinalFunc)param2, param3);
            break;

        case 0x91: {
            s16 id = pool[param3];

            func_08004B78(*(u32 *)param2, sprite_get_data(gSpriteHandler, id, 7));

            if (param1 != 0) {
                sprite_delete(gSpriteHandler, id);
            } else {
                sprite_set_visible(gSpriteHandler, id, 0);
                sprite_set_anim(gSpriteHandler, id, D_083A5098, 0, 0, 0, 0);
            }
            break;
        }

        case 0x92:
            if (param1 != 0) {
                func_080055D4((u16)get_current_mem_id(), gSpriteHandler,
                              *(u32 *)param2, param3, gCurrentSceneSpritePool);
            } else {
                func_08005538(gSpriteHandler, *(u32 *)param2, (void **)param3,
                              gCurrentSceneSpritePool);
            }
            break;

        case 0x9B: {
            s16 id = pool[param1 & 0xFF];

            sprite_set_callback_cel(gSpriteHandler, id, (s8)(param1 >> 8));
            sprite_set_callback(gSpriteHandler, id, (void *)param2, param3);
            break;
        }

        case 0x9C:
            sprite_delete(gSpriteHandler, pool[param2]);
            break;
    }
}

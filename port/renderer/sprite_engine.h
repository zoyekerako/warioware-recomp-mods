/* Native reimplementation of the game's sprite pipeline (lib_sprite).
 *
 * Behaviour follows the decompiled and byte-verified originals; the structs are
 * laid out for a 64-bit host rather than copying the GBA's 56-byte record, so
 * pointers are real pointers and ids stay 16-bit signed as the game uses them.
 *
 * The pipeline has three stages, matching the original:
 *   sprite_engine_emit()  walks the z-ordered list once per frame
 *   expand_cel()          turns one animation cel into OAM entries
 *   the caller            hands the OAM buffer to gba_ppu_render()
 */
#ifndef SPRITE_ENGINE_H
#define SPRITE_ENGINE_H

#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;

#define SPRITE_NONE ((s16)-1)

/* One animation step: a cel (count-prefixed OAM triples) and how long it runs. */
typedef struct {
    const u16 *cel;
    u8 duration;
} AnimFrame;

/* Playback types, from enum SpritePlaybackType. */
enum {
    SPRITE_PLAYBACK_NORMAL_LOOP = 0,
    SPRITE_PLAYBACK_ALTERNATING_LOOP = 1,
    SPRITE_PLAYBACK_NORMAL_HIDE = 2,
    SPRITE_PLAYBACK_NORMAL_DELETE = 3,
    SPRITE_PLAYBACK_NORMAL_CALLBACK = 4
};

typedef struct {
    unsigned visible : 1;
    unsigned playbackType : 4;
    unsigned celTotal : 8;
    unsigned noUpdate : 1;   /* the original's `update` bit gates animation OFF */
    unsigned allocated : 1;
    unsigned paused : 1;

    s16 xPos, yPos;
    u16 zDepth;

    const AnimFrame *animation;
    u8 celTimer;             /* ticks left on the current cel */
    s8 cel;                  /* current cel index */
    s8 celInc;               /* +1, 0 or -1 */
    s8 loopCel;

    u32 oamAttributes;       /* merged into every OAM entry of the sprite */
    s16 baseTile;
    u8 basePalette;

    s16 prev, next;          /* z-list neighbours */
    u32 memID;

    const s16 *xOrigin, *yOrigin;
    const s16 *affineParams;
} Sprite;

typedef struct {
    u16 objAmount;           /* OAM entries available per frame */
    u16 spriteAmount;
    u32 *oamBuffer;          /* objAmount * 2 words */
    Sprite *sprites;

    s16 zLinkStart, zLinkEnd;
    s16 nextAllocID, lastAllocID;

    u16 xPos, yPos;          /* global sprite offset */
    u16 totalCycles;         /* bit 0 flips the traversal direction each frame */
    u16 paused;
    u32 memID;
} SpriteHandler;

void sprite_handler_create(SpriteHandler *h, u16 objAmount, u32 *oamBuffer,
                           u16 spriteAmount, Sprite *sprites);
void sprite_handler_reset(SpriteHandler *h);

s16 sprite_create(SpriteHandler *h, const AnimFrame *anim, s16 x, s16 y, u16 z);
void sprite_delete(SpriteHandler *h, s16 id);
void sprite_set_z(SpriteHandler *h, s16 id, u16 z);
void sprite_set_anim_cel(SpriteHandler *h, s16 id, s8 cel);

/* One frame: emit OAM for every visible sprite, then advance animations. */
void sprite_engine_emit(SpriteHandler *h);

#endif /* SPRITE_ENGINE_H */

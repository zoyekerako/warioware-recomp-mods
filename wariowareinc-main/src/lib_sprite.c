#include "lib_sprite.h"

struct SpriteHandler* gSpriteHandler = &D_03000BF0;

asm(".include \"include/gba.inc\"");

void sprite_handler_create(struct SpriteHandler *handler, u16 objAmount,
                           u32 *oamBuffer, u16 spriteAmount,
                           struct Sprite *sprites) {
    u32 *dest = oamBuffer;
    u32 i;

    handler->objAmount = objAmount;
    handler->oamBuffer = oamBuffer;

    /* 0x22222222 reads as a disabled OAM entry. Blank the buffer eight words at
       a time, then whatever is left over. */
    for (i = 0; i < (u32)(objAmount >> 3); i++) {
        dest[7] = 0x22222222;
        dest[6] = 0x22222222;
        dest[5] = 0x22222222;
        dest[4] = 0x22222222;
        dest[3] = 0x22222222;
        dest[2] = 0x22222222;
        dest[1] = 0x22222222;
        dest[0] = 0x22222222;
        dest += 8;
    }

    for (i = 0; i < (u32)(objAmount & 7); i++) {
        *dest++ = 0x22222222;
    }

    handler->spriteAmount = spriteAmount;
    handler->sprites = sprites;
    handler->paused = 0;
    handler->xPos = 0;
    handler->yPos = 0;
    handler->totalCycles = 0;
    handler->memID = 0;
    handler->unk20 = 0xFF;
    handler->errorType = 0;

    sprite_handler_reset(handler);
}

extern s32 func_080efc88(struct struct_0804cb88 *args);

s32 func_080EE830(struct struct_0804cb88 *args) {
    const u16 *cel = args->src;
    const u16 *first;
    u16 total = *cel;
    s32 (*expand)(struct struct_0804cb88 *);

    args->objTotal = total;
    if ((u8)total == 0) {
        return 0;
    }

    first = cel + 1;
    args->src = first;
    args->objCount = 0;

    if (args->srcInc < 0) {
        args->src = first + (args->objTotal - 1) * 3;
    }

    expand = func_080efc88;
    return expand(args);
}

void sprite_handler_reset(struct SpriteHandler *handler) {
    u32 total = handler->spriteAmount;
    struct Sprite *sprites = handler->sprites;
    u32 i;

    for (i = 0; i < total; i++) {
        sprites[i].visible = FALSE;
        sprites[i].allocated = FALSE;
    }

    handler->zLinkEnd = 0xFFFF;
    handler->zLinkStart = -1;

    /* Rebuild the free list: each sprite points at its neighbours and both ends
       terminate, so allocation restarts from sprite 0. */
    sprites[0].unk18 = -1;
    sprites[0].unk1A = 1;

    for (i = 1; i < total - 1; i++) {
        sprites[i].unk18 = i - 1;
        sprites[i].unk1A = i + 1;
    }

    sprites[i].unk18 = i - 1;
    sprites[i].unk1A = 0xFFFF;

    handler->nextAllocID = 0;
    handler->lastAllocID = i;
}

#include "asm/lib_sprite/asm_080ee8f4.s"

#include "asm/lib_sprite/asm_080ee9b8.s"

/* progress is a 0..255 fraction of the animation's total run time. Walk the
   cels accumulating their durations until the sum passes the target. */
void sprite_set_anim_progress(struct SpriteHandler *handler, s16 id, u8 progress) {
    struct Sprite *sprite;
    struct Animation *anim;
    s32 target;
    s8 cel;
    u32 elapsed;

    D_03000E70 = SPRITE_OPERATION_SET_ANIM_PROGRESS;
    if (sprite_is_invalid(handler, id)) {
        return;
    }

    sprite = &handler->sprites[id];
    target = (progress * sprite->totalDuration) >> 8;
    anim = sprite->animation;
    cel = 0;
    elapsed = 0;

    do {
        elapsed += anim->duration;
        if (target < elapsed) {
            sprite_set_anim_cel(handler, id, cel);
            return;
        }
        anim++;
        cel++;
    } while (anim->cel != NULL);
}

#include "asm/lib_sprite/asm_080eebb8.s"

void sprite_remove_z_link(struct SpriteHandler *handler, s16 id) {
    struct Sprite *sprites = handler->sprites;
    s16 next = sprites[id].unk1A;
    s16 prev = sprites[id].unk18;

    if (sprites[id].unk18 >= 0)
        sprites[prev].unk1A = next;
    else
        handler->zLinkStart = next;

    if (next >= 0)
        sprites[next].unk18 = prev;
    else
        handler->zLinkEnd = prev;
}

s16 sprite_handler_alloc_id(struct SpriteHandler *handler) {
    u16 id = handler->nextAllocID;
    s16 index = handler->nextAllocID;

    if (index >= 0) {
        u16 next = handler->sprites[index].unk1A;

        handler->nextAllocID = next;
        if ((s16)next < 0)
            handler->lastAllocID = 0xFFFF;
    }

    return id;
}

void sprite_handler_dealloc_id(struct SpriteHandler *handler, s16 id) {
    if (id >= 0) {
        if (handler->lastAllocID >= 0)
            handler->sprites[handler->lastAllocID].unk1A = id;
        else
            handler->nextAllocID = id;

        handler->sprites[id].unk1A = 0xFFFF;
        handler->lastAllocID = id;
    }
}

u8 sprite_anim_get_cel_total(struct Animation *anim) {
    u8 count = 0;

    if (anim->cel != NULL) {
        do {
            count++;
        } while (anim[count].cel != NULL);
    }

    return count;
}

u16 sprite_get_anim_duration(struct Animation *anim) {
    u16 total = 0;

    while (anim->cel != NULL) {
        total += anim->duration;
        anim++;
    }

    return total;
}

s16 sprite_create(struct SpriteHandler *handler, struct Animation *anim,
                  u8 startCel, u16 x, u16 y, u16 z,
                  u8 celInc, u8 loopCel, u16 playback) {
    struct Sprite *sprite;
    s16 id = sprite_handler_alloc_id(handler);

    if (id < 0) {
        return -1;
    }

    sprite = &handler->sprites[id];

    sprite->allocated = TRUE;
    sprite->animation = anim;
    sprite->celInc = celInc;
    sprite->loopCel = loopCel;
    sprite->xPos = x;
    sprite->yPos = y;
    sprite->zDepth = z;
    sprite->celTotal = sprite_anim_get_cel_total(anim);
    sprite->playbackType = playback & 0xFF;
    sprite->oamAttributes = 0;
    sprite->baseTile = 0;
    sprite->basePalette = 0;
    sprite->callbackCel = 0xFF;
    sprite->update = FALSE;
    sprite->callbackFunc = NULL;
    sprite->totalDuration = sprite_get_anim_duration(anim);
    sprite->yOrigin = D_083EBA74;
    sprite->xOrigin = D_083EBA74;
    sprite->unk30 = handler->memID;

    sprite_update_z_link(handler, id);
    sprite_set_anim_cel(handler, id, startCel);

    sprite->visible = (playback >> 15) ^ 1;

    return id;
}

/* As sprite_create, but the caller supplies the OAM attribute word instead of
   starting it at zero. */
s16 sprite_create_w_attr(struct SpriteHandler *handler, struct Animation *anim,
                         u8 startCel, u16 x, u16 y, u16 z,
                         u8 celInc, u8 loopCel, u16 playback, u32 attr) {
    struct Sprite *sprite;
    s16 id = sprite_handler_alloc_id(handler);

    if (id < 0) {
        return -1;
    }

    sprite = &handler->sprites[id];

    sprite->allocated = TRUE;
    sprite->animation = anim;
    sprite->celInc = celInc;
    sprite->loopCel = loopCel;
    sprite->xPos = x;
    sprite->yPos = y;
    sprite->zDepth = z;
    sprite->celTotal = sprite_anim_get_cel_total(anim);
    sprite->playbackType = playback & 0xFF;
    sprite->oamAttributes = attr;
    sprite->baseTile = 0;
    sprite->basePalette = 0;
    sprite->callbackCel = 0xFF;
    sprite->update = FALSE;
    sprite->callbackFunc = NULL;
    sprite->totalDuration = sprite_get_anim_duration(anim);
    sprite->yOrigin = D_083EBA74;
    sprite->xOrigin = D_083EBA74;
    sprite->unk30 = handler->memID;

    sprite_update_z_link(handler, id);
    sprite_set_anim_cel(handler, id, startCel);

    sprite->visible = (playback >> 15) ^ 1;

    return id;
}

/* Duplicates a sprite verbatim, stamps the clone with the handler's current
   memory ID and links it into the z-list. Returns the new id, or -1. */
s16 sprite_clone(struct SpriteHandler *handler, s16 id) {
    u8 *src;
    u8 *dest;
    s16 i;
    s16 newId;

    D_03000E70 = SPRITE_OPERATION_CLONE;
    if (sprite_is_invalid(handler, id)) {
        return -1;
    }

    newId = sprite_handler_alloc_id(handler);
    if (newId < 0) {
        return -1;
    }

    dest = (u8 *)&handler->sprites[newId];
    src = (u8 *)&handler->sprites[id];

    for (i = 0; i <= 0x37u; i++) {
        *dest++ = *src++;
    }

    handler->sprites[newId].unk30 = handler->memID;
    sprite_update_z_link(handler, newId);

    return newId;
}

/* Overwrites one sprite with another's contents, but keeps the destination's
   own memory ID and re-sorts it into the z-list under its new depth. */
void sprite_copy(struct SpriteHandler *handler, s16 destId, s16 srcId) {
    u8 *src;
    u8 *dest;
    s16 i;
    u32 memID;

    D_03000E70 = SPRITE_OPERATION_COPY;
    if (sprite_is_invalid(handler, destId)) {
        return;
    }
    if (sprite_is_invalid(handler, srcId)) {
        return;
    }

    memID = handler->sprites[destId].unk30;
    sprite_remove_z_link(handler, destId);

    dest = (u8 *)&handler->sprites[destId];
    src = (u8 *)&handler->sprites[srcId];

    for (i = 0; i <= 0x37u; i++) {
        *dest++ = *src++;
    }

    sprite_update_z_link(handler, destId);
    handler->sprites[destId].unk30 = memID;
}

void sprite_delete(struct SpriteHandler *handler, s16 id) {
    D_03000E70 = SPRITE_OPERATION_DELETE;
    if (sprite_is_invalid(handler, id)) {
        return;
    }

    handler->sprites[id].visible = FALSE;
    handler->sprites[id].allocated = FALSE;

    sprite_remove_z_link(handler, id);
    sprite_handler_dealloc_id(handler, id);
}

void sprite_set_x_y_z(struct SpriteHandler *handler, s16 id, u16 x, u16 y, u16 z) {
    D_03000E70 = SPRITE_OPERATION_SET_XYZ;
    if (sprite_is_invalid(handler, id)) {
        return;
    }

    handler->sprites[id].xPos = x;
    handler->sprites[id].yPos = y;

    if (handler->sprites[id].zDepth != z) {
        sprite_remove_z_link(handler, id);
        handler->sprites[id].zDepth = z;
        sprite_update_z_link(handler, id);
    }
}

void sprite_set_x_y(struct SpriteHandler *handler, s16 id, s16 x, s16 y) {
    D_03000E70 = SPRITE_OPERATION_SET_XY;
    if (sprite_is_invalid(handler, id)) {
        return;
    }

    handler->sprites[id].xPos = x;
    handler->sprites[id].yPos = y;
}

void sprite_set_x(struct SpriteHandler *handler, s16 id, u16 x) {
    D_03000E70 = SPRITE_OPERATION_SET_X;
    if (sprite_is_invalid(handler, id)) {
        return;
    }

    handler->sprites[id].xPos = x;
}

void sprite_set_y(struct SpriteHandler *handler, s16 id, u16 y) {
    D_03000E70 = SPRITE_OPERATION_SET_Y;
    if (sprite_is_invalid(handler, id)) {
        return;
    }

    handler->sprites[id].yPos = y;
}

void sprite_set_z(struct SpriteHandler *handler, s16 id, u16 z) {
    D_03000E70 = SPRITE_OPERATION_SET_Z;
    if (sprite_is_invalid(handler, id)) {
        return;
    }

    if (handler->sprites[id].zDepth != z) {
        sprite_remove_z_link(handler, id);
        handler->sprites[id].zDepth = z;
        sprite_update_z_link(handler, id);
    }
}

/* Returns the sprite's current animation cel, or -1 for an invalid handle.
   Kept under its address name because assembly call sites reference it. */
s32 func_080EF31C(struct SpriteHandler *handler, s16 id) {
    D_03000E70 = SPRITE_OPERATION_GET_ANIM_CEL;
    if (sprite_is_invalid(handler, id)) {
        return -1;
    }

    return handler->sprites[id].unkD;
}

/* Returns how far the sprite is through its animation, as a 0..255 fraction.
   Kept under its address name because assembly call sites reference it. */
u8 func_080EF358(struct SpriteHandler *handler, s16 id) {
    struct Sprite *sprite;
    struct Animation *anim;
    u32 elapsed;
    u32 i;

    D_03000E70 = SPRITE_OPERATION_GET_ANIM_PROGRESS;
    if (sprite_is_invalid(handler, id)) {
        return 0;
    }

    sprite = &handler->sprites[id];
    anim = sprite->animation;
    elapsed = 0;

    for (i = 0; i < sprite->unkD; i++) {
        elapsed += anim->duration;
        anim++;
    }

    elapsed += anim->duration - sprite->unkC;

    return (elapsed << 8) / sprite->totalDuration;
}

void sprite_set_visible(struct SpriteHandler *handler, s16 id, u16 isVisible) {
    D_03000E70 = SPRITE_OPERATION_SET_VISIBLE;
    if (sprite_is_invalid(handler, id)) {
        return;
    }

    handler->sprites[id].visible = isVisible;
}

void sprite_attr_set(struct SpriteHandler *handler, s16 id, u32 attr) {
    D_03000E70 = SPRITE_OPERATION_SET_ATTR;
    if (sprite_is_invalid(handler, id)) {
        return;
    }

    handler->sprites[id].oamAttributes = attr;
}

void sprite_attr_orr(struct SpriteHandler *handler, s16 id, u32 attr) {
    D_03000E70 = SPRITE_OPERATION_ORR_ATTR;
    if (sprite_is_invalid(handler, id)) {
        return;
    }

    handler->sprites[id].oamAttributes |= attr;
}

void sprite_attr_and(struct SpriteHandler *handler, s16 id, u32 attr) {
    D_03000E70 = SPRITE_OPERATION_AND_ATTR;
    if (sprite_is_invalid(handler, id)) {
        return;
    }

    handler->sprites[id].oamAttributes &= attr;
}

void sprite_set_base_tile(struct SpriteHandler *handler, s16 id, s16 baseTile) {
    D_03000E70 = SPRITE_OPERATION_SET_BASE_TILE;
    if (sprite_is_invalid(handler, id)) {
        return;
    }

    handler->sprites[id].baseTile = baseTile;
}

void sprite_set_base_palette(struct SpriteHandler *handler, s16 id, s8 basePalette) {
    D_03000E70 = SPRITE_OPERATION_SET_BASE_PALETTE;
    if (sprite_is_invalid(handler, id)) {
        return;
    }

    handler->sprites[id].basePalette = basePalette;
}

void sprite_set_anim(struct SpriteHandler *handler, s16 id, struct Animation *anim, s8 startCel, s8 direction, s8 loopCel, u16 playbackType) {
    struct Sprite *sprite;

    D_03000E70 = SPRITE_OPERATION_SET_ANIM;
    if (sprite_is_invalid(handler, id)) {
        return;
    }

    sprite = &handler->sprites[id];
    sprite->celTotal = sprite_anim_get_cel_total(anim);
    sprite->animation = anim;
    sprite->totalDuration = sprite_get_anim_duration(anim);

    if (startCel >= 0) {
        sprite->celInc = direction;
        sprite->loopCel = loopCel;
        sprite->playbackType = playbackType;
        sprite_set_anim_cel(handler, id, startCel);
    }
}

void sprite_set_enable_updates(struct SpriteHandler *handler, s16 id, u16 canUpdate) {
    D_03000E70 = SPRITE_OPERATION_SET_ENABLE_UPDATES;
    if (sprite_is_invalid(handler, id)) {
        return;
    }
    
    handler->sprites[id].update = canUpdate;
}

void sprite_set_callback(struct SpriteHandler *handler, s16 id, void *func, u32 arg) {
    D_03000E70 = SPRITE_OPERATION_SET_CALLBACK;
    if (sprite_is_invalid(handler, id)) {
        return;
    }

    handler->sprites[id].callbackFunc = func;
    handler->sprites[id].callbackArg = arg;
}

void sprite_set_playback(struct SpriteHandler *handler, s16 id, u8 celInc, u8 loopCel, u16 playback) {
    struct Sprite *sprite;

    D_03000E70 = SPRITE_OPERATION_SET_PLAYBACK;

    if (sprite_is_invalid(handler, id)) {
        return;
    }

    sprite = &handler->sprites[id];
    
    sprite->celInc = celInc;
    sprite->loopCel = loopCel;
    sprite->playbackType = playback & 0xff;
    sprite->visible = ((playback & 0x8000) == 0);
}

void sprite_set_origin_x_y(struct SpriteHandler *handler, s16 id, s16 *xOrigin, s16 *yOrigin) {
    struct Sprite *sprite;

    D_03000E70 = SPRITE_OPERATION_SET_ORIGIN;
    if (sprite_is_invalid(handler, id)) {
        return;
    }

    sprite = &handler->sprites[id];
    sprite->xOrigin = xOrigin;
    if (xOrigin == NULL) {
        sprite->xOrigin = D_083EBA74;
    }

    sprite->yOrigin = yOrigin;
    if (yOrigin == NULL) {
        sprite->yOrigin = D_083EBA74;
    }
}

void sprite_set_affine_params(struct SpriteHandler *handler, s16 id, s32 affineIndex, s16 *affineParams) {
    struct Sprite *sprite;

    D_03000E70 = SPRITE_OPERATION_SET_AFFINE;
    if (sprite_is_invalid(handler, id)) {
        return;
    }

    sprite = &handler->sprites[id];
    if (affineIndex >= 0) {
        sprite->affineParams = affineParams;
        sprite->oamAttributes &= ~0x02000000; // Clear affine flag.
        sprite->oamAttributes &= ~0x00003E00; // Clear affine param index.
        sprite->oamAttributes &= ~(2 | 1);
        sprite->oamAttributes |= 0x2000000; // Affine flag.
        sprite->oamAttributes |= affineIndex << 9; // Affine param index.
        sprite->oamAttributes |= (2 | 1); // Affine | Double-Size flag.
    } else {
        sprite->oamAttributes &= ~0x02000000;
        sprite->oamAttributes &= ~0x00003E00;
        sprite->oamAttributes &= ~(2 | 1);
    }
}

s32 sprite_get_data(struct SpriteHandler *handler, s16 id, u32 requestedDataType) {
    struct Sprite *sprite;
    s32 output;

    D_03000E70 = SPRITE_OPERATION_GET_DATA;
    if (sprite_is_invalid(handler, id)) {
        return 0;
    }

    sprite = &handler->sprites[id];
    switch (requestedDataType) {
        case SPRITE_DATA_VISIBLE:
            output = sprite->visible;
            break;
        case SPRITE_DATA_PLAYBACK_TYPE:
            output = sprite->playbackType;
            break;
        case SPRITE_DATA_TOTAL_CELS:
            output = sprite->celTotal;
            break;
        case SPRITE_DATA_UPDATE_FLAG:
            output = sprite->update;
            break;
        case SPRITE_DATA_X_POS:
            output = sprite->xPos;
            break;
        case SPRITE_DATA_Y_POS:
            output = sprite->yPos;
            break;
        case SPRITE_DATA_Z_DEPTH:
            output = sprite->zDepth;
            break;
        case SPRITE_DATA_ANIMATION:
            output = (u32)sprite->animation;
            break;
        case SPRITE_DATA_UNKC:
            output = sprite->unkC;
            break;
        case SPRITE_DATA_UNKD:
            output = sprite->unkD;
            break;
        case SPRITE_DATA_CEL_INC:
            output = sprite->celInc;
            break;
        case SPRITE_DATA_LOOP_CEL:
            output = sprite->loopCel;
            break;
        case SPRITE_DATA_ATTRS10:
            output = sprite->oamAttributes;
            break;
        case SPRITE_DATA_BASE_TILE:
            output = sprite->baseTile;
            break;
        case SPRITE_DATA_CALLBACK_FUNC:
            output = (u32)sprite->callbackFunc;
            break;
        case SPRITE_DATA_CALLBACK_ARG:
            output = sprite->callbackArg;
            break;
        case SPRITE_DATA_UNK30:
            output = sprite->unk30;
            break;
        case SPRITE_DATA_ORIGIN_X:
            output = (u32)sprite->xOrigin;
            break;
        case SPRITE_DATA_ORIGIN_Y:
            output = (u32)sprite->yOrigin;
            break;
        default:
            output = 0;
            break;
    }

    return output;
}

u32 sprite_set_callback_cel(struct SpriteHandler *handler, s16 id, s8 cel) {
    struct Sprite *sprite;

    if (id < 0) {
        return;
    }
    
    sprite = &handler->sprites[id];
    sprite->callbackCel = cel;
}

void sprite_id_set_data(struct SpriteHandler *handler, u32 memID, u32 targetDataType, u32 arg) {
    s16 spriteID = handler->zLinkStart;

    while (spriteID >= 0) {
        s16 nextID = handler->sprites[spriteID].unk1A;

        if (handler->sprites[spriteID].unk30 == memID) {
            switch (targetDataType) {
                case SPRITE_ACT_DELETE:
                    sprite_delete(handler, spriteID);
                break;
                case SPRITE_ACT_SET_VISIBLE:
                    sprite_set_visible(handler, spriteID, (u16)arg);
                    break;
                case SPRITE_ACT_SET_UPDATE:
                    sprite_set_enable_updates(handler, spriteID, (u16)arg);
                    break;
                case SPRITE_ACT_SET_ATTR:
                    sprite_attr_set(handler, spriteID, arg);
                    break;
                case SPRITE_ACT_ORR_ATTR:
                    sprite_attr_orr(handler, spriteID, arg);
                    break;
                case SPRITE_ACT_AND_ATTR:
                    sprite_attr_and(handler, spriteID, arg);
                    break;
                case SPRITE_ACT_SET_BASE_TILE:
                    sprite_set_base_tile(handler, spriteID, (s16)arg);
                    break;
                case SPRITE_ACT_SET_BASE_PALETTE:
                    sprite_set_base_palette(handler, spriteID, (s8)arg);
                    break;
                case SPRITE_ACT_SET_ORIGIN_XY:
                    sprite_set_origin_x_y(handler, spriteID, *(s16 **)arg, *(s16 **)((u32 *)arg + 1));
                    break;
            }
        }
        
        spriteID = nextID;
    }    
}

u32 func_080EF998(struct SpriteHandler *handler) {
    handler->unk20++;
    if (handler->unk20 == 0) {
        handler->unk20 = 0x100;
    }

    return handler->unk20;
}

void sprite_handler_set_mem_id(struct SpriteHandler *handler, u32 memID) {
    handler->memID = memID;
}

u32 sprite_handler_get_mem_id(struct SpriteHandler *handler) {
    return handler->memID;
}

void sprite_id_delete(struct SpriteHandler *handler, u32 memID) {
    sprite_id_set_data(handler, memID, SPRITE_ACT_DELETE, 0);
}

void sprite_id_set_visible(struct SpriteHandler *handler, u32 memID, u16 isVisible) {
    sprite_id_set_data(handler, memID, SPRITE_ACT_SET_VISIBLE, isVisible);
}

void sprite_id_set_enable_updates(struct SpriteHandler *handler, u32 memID, u16 canUpdate) {
    sprite_id_set_data(handler, memID, SPRITE_ACT_SET_UPDATE, canUpdate);
}

void sprite_id_set_attr(struct SpriteHandler *handler, u32 memID, u32 attr) {
    sprite_id_set_data(handler, memID, SPRITE_ACT_SET_ATTR, attr);
}

void sprite_id_orr_attr(struct SpriteHandler *handler, u32 memID, u32 attr) {
    sprite_id_set_data(handler, memID, SPRITE_ACT_ORR_ATTR, attr);
}

void sprite_id_and_attr(struct SpriteHandler *handler, u32 memID, u32 attr) {
    sprite_id_set_data(handler, memID, SPRITE_ACT_AND_ATTR, attr);
}

void sprite_id_set_base_tile(struct SpriteHandler *handler, u32 memID, s16 baseTile) {
    sprite_id_set_data(handler, memID, SPRITE_ACT_SET_BASE_TILE, baseTile);
}

void sprite_id_set_base_palette(struct SpriteHandler *handler, u32 memID, s8 basePalette) {
    sprite_id_set_data(handler, memID, SPRITE_ACT_SET_BASE_PALETTE, basePalette);
}

void sprite_id_set_origin_x_y(struct SpriteHandler *handler, u32 memID, s16 *xOrigin, s16 *yOrigin) {
    s16 *origin[2] = { xOrigin, yOrigin };

    sprite_id_set_data(handler, memID, SPRITE_ACT_SET_ORIGIN_XY, (uintptr_t)&origin);
}

void sprite_handler_set_global_pause(struct SpriteHandler *handler, u16 isPaused) {
    handler->paused = isPaused;
}

void sprite_handler_set_global_x_y(struct SpriteHandler *handler, u16 x, u16 y) {
    handler->xPos = x;
    handler->yPos = y;
}

#include "asm/lib_sprite/asm_080efa60.s"

/* Number of sprites currently linked into the z-list. */
s32 func_080EFC20(struct SpriteHandler *handler) {
    s32 count = 0;
    struct Sprite *sprites = handler->sprites;
    s32 id = handler->zLinkStart;

    while (id != -1) {
        count++;
        id = sprites[id].unk1A;
    }

    return count;
}

/* Number of linked sprites belonging to one memory ID. */
s32 func_080EFC50(struct SpriteHandler *handler, u32 memID) {
    s32 count = 0;
    struct Sprite *sprites = handler->sprites;
    s32 id = handler->zLinkStart;

    while (id != -1) {
        if (sprites[id].unk30 == memID) {
            count++;
        }
        id = sprites[id].unk1A;
    }

    return count;
}
#include "sprite_engine.h"
#include <string.h>

#define SCREEN_W 240
#define SCREEN_H 160

/* Standard GBA OAM dimensions, indexed by shape*4 + size. Width in the low
 * byte, height in the high byte -- the same 24-byte table the game copies onto
 * the stack before every emit. */
static const u16 kObjDim[12] = {
    0x0808, 0x1010, 0x2020, 0x4040,   /* square */
    0x0810, 0x0820, 0x1020, 0x2040,   /* wide   */
    0x1008, 0x2008, 0x2010, 0x4020    /* tall   */
};

/* ---- free list ---------------------------------------------------------- */

static s16 alloc_id(SpriteHandler *h)
{
    s16 id = h->nextAllocID;

    if (h->nextAllocID >= 0) {
        h->nextAllocID = h->sprites[h->nextAllocID].next;
        if (h->nextAllocID < 0)
            h->lastAllocID = SPRITE_NONE;
    }
    return id;
}

static void dealloc_id(SpriteHandler *h, s16 id)
{
    if (id < 0) return;

    if (h->lastAllocID >= 0)
        h->sprites[h->lastAllocID].next = id;
    else
        h->nextAllocID = id;

    h->sprites[id].next = SPRITE_NONE;
    h->lastAllocID = id;
}

/* ---- z-ordered doubly linked list --------------------------------------- */

static void remove_z_link(SpriteHandler *h, s16 id)
{
    Sprite *s = h->sprites;
    s16 next = s[id].next;
    s16 prev = s[id].prev;

    if (prev >= 0) s[prev].next = next; else h->zLinkStart = next;
    if (next >= 0) s[next].prev = prev; else h->zLinkEnd = prev;
}

/* Insert by zDepth, walking back from the tail while the sprite sorts in
 * front. Lower zDepth draws on top, so the list runs front to back. */
static void update_z_link(SpriteHandler *h, s16 id)
{
    Sprite *s = h->sprites;
    u16 z = s[id].zDepth;
    s16 prev = h->zLinkEnd;

    while (prev != SPRITE_NONE && z < s[prev].zDepth)
        prev = s[prev].prev;

    if (prev >= 0) {
        s16 next = s[prev].next;
        s[prev].next = id;
        s[id].prev = prev;
        s[id].next = next;
        if (next >= 0) s[next].prev = id;
    } else {
        s16 start = h->zLinkStart;
        if (start >= 0) {
            s[start].prev = id;
            s[id].next = start;
        } else {
            s[id].next = SPRITE_NONE;
        }
        s[id].prev = SPRITE_NONE;
    }

    if (s[id].prev == SPRITE_NONE) h->zLinkStart = id;
    if (s[id].next == SPRITE_NONE) h->zLinkEnd = id;
}

/* ---- lifecycle ---------------------------------------------------------- */

void sprite_handler_reset(SpriteHandler *h)
{
    u16 total = h->spriteAmount;
    Sprite *s = h->sprites;
    u16 i;

    for (i = 0; i < total; i++) {
        s[i].visible = 0;
        s[i].allocated = 0;
    }

    h->zLinkStart = SPRITE_NONE;
    h->zLinkEnd = SPRITE_NONE;

    s[0].prev = SPRITE_NONE;
    s[0].next = 1;
    for (i = 1; i < total - 1u; i++) {
        s[i].prev = (s16)(i - 1);
        s[i].next = (s16)(i + 1);
    }
    s[i].prev = (s16)(i - 1);
    s[i].next = SPRITE_NONE;

    h->nextAllocID = 0;
    h->lastAllocID = (s16)i;
}

void sprite_handler_create(SpriteHandler *h, u16 objAmount, u32 *oamBuffer,
                           u16 spriteAmount, Sprite *sprites)
{
    u16 i;

    memset(h, 0, sizeof *h);
    h->objAmount = objAmount;
    h->oamBuffer = oamBuffer;
    h->spriteAmount = spriteAmount;
    h->sprites = sprites;

    memset(sprites, 0, sizeof(Sprite) * spriteAmount);

    /* 0x22222222 reads as a disabled OAM entry, which is how the game blanks
     * the buffer at startup. */
    for (i = 0; i < objAmount; i++)
        oamBuffer[i] = 0x22222222u;

    sprite_handler_reset(h);
}

static u8 anim_cel_total(const AnimFrame *anim)
{
    u8 n = 0;
    if (anim->cel != 0) {
        do { n++; } while (anim[n].cel != 0);
    }
    return n;
}

/* Moves a sprite to a new cel, clamping or reflecting when it runs off either
 * end. Transcribed from asm/lib_sprite/asm_080ee9b8.s -- the exact clamping
 * matters, since several title animations rely on the bounce arithmetic.
 *
 *   loop / hide / callback : fall back to loopCel, or the last cel if loopCel
 *                            is itself out of range
 *   alternating            : reflect. Past the end -> 2*total - cel - 2,
 *                            before the start -> -cel, each clamped
 *   delete                 : the sprite is destroyed and nothing is applied
 */
void sprite_set_anim_cel(SpriteHandler *h, s16 id, s8 cel)
{
    Sprite *s = &h->sprites[id];
    s8 loopCel = s->loopCel;
    s8 total = (s8)s->celTotal;
    int apply = 1;

    if (s->loopCel < 0)
        loopCel = 0;

    if (cel < 0 || cel >= total) {
        switch (s->playbackType) {
        case SPRITE_PLAYBACK_NORMAL_LOOP:
        case SPRITE_PLAYBACK_NORMAL_HIDE:
        case SPRITE_PLAYBACK_NORMAL_CALLBACK:
            if (cel >= total)
                cel = (loopCel < total) ? loopCel : (s8)(total - 1);
            if (cel < 0)
                cel = (loopCel < total) ? loopCel : (s8)(total - 1);
            if (s->playbackType == SPRITE_PLAYBACK_NORMAL_HIDE)
                s->visible = 0;
            break;

        case SPRITE_PLAYBACK_ALTERNATING_LOOP:
            if (cel < 0) {
                cel = (s8)(-cel);
                if (cel >= total)
                    cel = (s8)(total - 1);
            } else {
                cel = (s8)(total - (cel - total + 2));
                if (cel < 0)
                    cel = 0;
            }
            s->celInc = (s8)(-s->celInc);
            break;

        case SPRITE_PLAYBACK_NORMAL_DELETE:
            sprite_delete(h, id);
            return;

        default:
            apply = 0;
            break;
        }
    }

    if (!apply)
        return;

    s->cel = cel;
    s->celTimer = s->animation[cel].duration;
}

s16 sprite_create(SpriteHandler *h, const AnimFrame *anim, s16 x, s16 y, u16 z)
{
    s16 id = alloc_id(h);
    Sprite *s;

    if (id < 0) return SPRITE_NONE;

    s = &h->sprites[id];
    memset(s, 0, sizeof *s);

    s->allocated = 1;
    s->visible = 1;
    s->animation = anim;
    s->celTotal = anim_cel_total(anim);
    s->celInc = 1;
    s->loopCel = 0;
    s->playbackType = SPRITE_PLAYBACK_NORMAL_LOOP;
    s->xPos = x;
    s->yPos = y;
    s->zDepth = z;
    s->memID = h->memID;
    s->cel = 0;
    s->celTimer = anim[0].duration;

    update_z_link(h, id);
    return id;
}

void sprite_delete(SpriteHandler *h, s16 id)
{
    if (id < 0 || id >= (s16)h->spriteAmount) return;
    if (!h->sprites[id].allocated) return;

    h->sprites[id].visible = 0;
    h->sprites[id].allocated = 0;
    remove_z_link(h, id);
    dealloc_id(h, id);
}

void sprite_set_z(SpriteHandler *h, s16 id, u16 z)
{
    if (id < 0 || !h->sprites[id].allocated) return;
    if (h->sprites[id].zDepth == z) return;

    remove_z_link(h, id);
    h->sprites[id].zDepth = z;
    update_z_link(h, id);
}

/* ---- cel expansion ------------------------------------------------------ */

/* Writes one animation cel into the OAM buffer and returns how many entries it
 * actually placed. Entries that fall entirely off screen are skipped but still
 * consume a source entry, exactly as the original ARM routine does. */
static u16 expand_cel(const u16 *cel, u32 *dest, int destStep,
                      s16 xPos, s16 yPos, u32 attr10, u32 attr2Base,
                      const s16 *affine)
{
    u16 total = cel[0];
    const u16 *src = cel + 1;
    u16 written = 0;
    u16 i;

    for (i = 0; i < total; i++, src += 3) {
        u16 a0 = src[0], a1 = src[1], a2 = src[2];
        int shape = a0 >> 14;
        int size = a1 >> 14;
        u16 dim = kObjDim[shape * 4 + size];
        int w, hgt;
        int x, y;

        /* Cel coordinates are signed offsets: 8 bits in attr0, 9 in attr1. */
        y = (s32)((s8)(a0 & 0xFF));
        x = (s32)(((s16)(a1 << 7)) >> 7);

        a0 |= (u16)(attr10 & 0xFF00);
        a1 |= (u16)((attr10 >> 16) & 0xFE00);

        w = dim & 0xFF;
        hgt = dim >> 8;
        if ((a0 & 0x0300) == 0x0300) { w *= 2; hgt *= 2; }

        if (affine != 0 && (attr10 & 0x200)) {
            int ox = x + (w >> 2);
            int oy = y + (hgt >> 2);
            int nx = ((ox * affine[0] + oy * affine[1]) >> 8) - (w >> 1);
            int ny = ((ox * affine[2] + oy * affine[3]) >> 8) - (hgt >> 1);
            x = nx;
            y = ny;
        }

        x += xPos;
        y += yPos;

        /* Same clip test as the original: reject when the shifted origin leaves
         * the visible band. */
        if ((unsigned)(x + w) >= (unsigned)(w + SCREEN_W)) continue;
        if ((unsigned)(y + hgt) >= (unsigned)(hgt + SCREEN_H)) continue;

        a0 = (u16)((a0 & 0xFF00) | (y & 0x00FF));
        a1 = (u16)((a1 & 0xFE00) | (x & 0x01FF));
        a2 = (u16)(a2 + attr2Base);

        dest[0] = (u32)a0 | ((u32)a1 << 16);
        dest[1] = (u32)a2;
        dest += destStep;
        written++;
    }
    return written;
}

/* ---- per-frame emit ----------------------------------------------------- */

void sprite_engine_emit(SpriteHandler *h)
{
    int backwards = h->totalCycles & 1;
    u16 budget = h->objAmount;
    u32 *oam;
    s16 id;

    if (backwards) {
        id = h->zLinkEnd;
        oam = h->oamBuffer + (h->objAmount - 1) * 2;
    } else {
        id = h->zLinkStart;
        oam = h->oamBuffer;
    }

    while (id >= 0) {
        Sprite *s = &h->sprites[id];
        s16 nextId = backwards ? s->prev : s->next;

        if (s->visible && s->animation != 0) {
            const u16 *cel = s->animation[s->cel].cel;

            /* A sprite whose cel does not fit the remaining budget is dropped
             * whole, never truncated. */
            if (cel != 0 && budget >= cel[0]) {
                s16 x = (s16)((s->xOrigin ? *s->xOrigin : 0) + h->xPos);
                s16 y = (s16)((s->yOrigin ? *s->yOrigin : 0) + h->yPos);
                u32 attr2 = (u32)((s->basePalette << 12) | (s->baseTile & 0x3FF));
                u16 n = expand_cel(cel, oam, backwards ? -2 : 2,
                                   (s16)(s->xPos - x), (s16)(s->yPos - y),
                                   s->oamAttributes | ((u32)(s->zDepth >> 14) << 18),
                                   attr2, s->affineParams);
                budget = (u16)(budget - n);
                oam += backwards ? -(int)n * 2 : (int)n * 2;
            }
        }

        if (!h->paused && !s->noUpdate) {
            if (s->visible || (s->oamAttributes & 0x01000000u)) {
                if (s->celTimer != 0 && --s->celTimer == 0)
                    sprite_set_anim_cel(h, id, (s8)(s->celInc + s->cel));
            }
        }

        id = nextId;
    }

    h->totalCycles++;
}

/* func_080EFA60 -- the per-frame Sprite -> OAM emitter of lib_sprite.
 *
 * STATUS: hand-decompiled from asm/lib_sprite/asm_080efa60.s (221 Thumb
 * instructions). Every struct offset has been checked against
 * src/lib_sprite.h, but this is NOT yet proven to match the ROM -- that needs
 * agbcc plus objdiff. Reviewed draft, not a verified match.
 *
 * Once per frame it walks the z-linked sprite list and hands each visible
 * sprite's current animation cel to func_080EE830, which expands the cel into
 * OAM entries. Two details are worth knowing before porting anything above it:
 *
 *  - Bit 0 of handler->totalCycles flips the traversal every frame. On even
 *    frames it runs zLinkStart -> zLinkEnd writing OAM forwards (srcInc +6,
 *    destInc +8); on odd frames it runs the list backwards from zLinkEnd and
 *    fills the OAM buffer from the far end (srcInc -6, destInc -8). A cel
 *    source entry is 6 bytes (three OAM halfwords), an OAM slot is 8.
 *
 *  - objLimit is the remaining OAM budget for the whole frame. A sprite whose
 *    cel needs more entries than are left is skipped WHOLE, not clipped, so
 *    running out of OAM drops entire sprites rather than truncating one.
 */

#include "global.h"
#include "lib_sprite.h"

/* 24-byte OAM dimension table, copied into the argument block on every call so
 * that func_080EE830 can index it. */
extern const u8 D_083EBA76[24];

/* Expands one animation cel into OAM entries; returns how many it wrote. */
extern u16 func_080EE830(struct struct_0804cb88 *args);

extern void sprite_set_anim_cel(struct SpriteHandler *handler, s16 id, s8 cel);

void func_080EFA60(struct SpriteHandler *handler)
{
    struct struct_0804cb88 args;
    u32 *oam;
    s16 id;
    s16 i;

    args.objLimit = handler->objAmount;

    for (i = 0; i <= 0x17; i++)
        args.objDim[i] = D_083EBA76[i];

    if (handler->totalCycles & 1) {
        id = handler->zLinkEnd;
        oam = handler->oamBuffer + (handler->objAmount - 1) * 2;
        args.srcInc = -6;
        args.destInc = -8;
    } else {
        id = handler->zLinkStart;
        oam = handler->oamBuffer;
        args.srcInc = 6;
        args.destInc = 8;
    }

    while (id >= 0) {
        struct Sprite *sprite = &handler->sprites[id];
        s16 next = (handler->totalCycles & 1) ? sprite->unk18 : sprite->unk1A;

        if (sprite->visible) {
            struct OamCel *cel = (struct OamCel *)sprite->animation[sprite->unkD].cel;

            if (args.objLimit >= cel->total) {
                s16 x = *sprite->xOrigin + handler->xPos;
                s16 y = *sprite->yOrigin + handler->yPos;
                u16 written;

                args.src = (const u16 *)cel;
                args.dest = oam;
                args.xPos = sprite->xPos - x;
                args.yPos = sprite->yPos - y;
                args.attr10 = sprite->oamAttributes | ((sprite->zDepth >> 14) << 18);
                args.attr2 = ((s8)sprite->basePalette << 12) | sprite->baseTile;

                /* The middle two affine terms are negated on the way in. */
                args.affine[0] = sprite->affineParams[0];
                args.affine[1] = -sprite->affineParams[1];
                args.affine[2] = -sprite->affineParams[2];
                args.affine[3] = sprite->affineParams[3];

                written = func_080EE830(&args);
                args.objLimit -= written;

                if (handler->totalCycles & 1)
                    oam -= written * 2;
                else
                    oam += written * 2;
            }
        }

        /* Animation tick. A paused handler freezes every sprite; per sprite the
         * `update` bit gates it too. A hidden sprite still advances when bit 24
         * of oamAttributes is set, so off-screen animations stay in sync. */
        if (!handler->paused && !sprite->update) {
            if (sprite->visible || (sprite->oamAttributes & 0x01000000)) {
                if (--sprite->unkC == 0)
                    sprite_set_anim_cel(handler, id, sprite->celInc + sprite->unkD);
            }
        }

        id = next;
    }

    handler->totalCycles++;
}

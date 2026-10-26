/* Drives the native sprite pipeline against a scene blob and writes frames out
 * as PNGs. This is the visible end of the chain:
 *
 *   ROM -> extracted assets -> scene blob -> compositor
 *                           -> animation blob -> sprite engine -> OAM
 */

#include "gba_ppu.h"
#include "png_write.h"
#include "sprite_engine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCENE_HEADER 1060
#define MAX_SPRITES 200
#define MAX_OBJ 128

typedef struct {
    u32 animCount;
    AnimFrame **anims;   /* each NULL-cel terminated */
    char **names;
    u16 *celData;
    void *block;
} AnimBank;

static u32 rd32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }

static int anim_bank_load(AnimBank *bank, const char *path)
{
    FILE *f = fopen(path, "rb");
    long size;
    u8 *raw;
    u32 celCount, animCount, celHw, frameCount;
    const u8 *celOff, *animStart, *frames;
    const u8 *names;
    u32 i;

    if (!f) { perror(path); return -1; }
    fseek(f, 0, SEEK_END); size = ftell(f); fseek(f, 0, SEEK_SET);
    raw = malloc((size_t)size);
    if (!raw || fread(raw, 1, (size_t)size, f) != (size_t)size) { fclose(f); return -1; }
    fclose(f);

    if (memcmp(raw, "WWAN", 4) != 0) { fprintf(stderr, "%s: bad magic\n", path); return -1; }

    celCount = rd32(raw + 8);
    animCount = rd32(raw + 12);
    celHw = rd32(raw + 16);

    celOff = raw + 20;
    animStart = celOff + celCount * 4;
    frameCount = rd32(animStart + animCount * 4);
    frames = animStart + (animCount + 1) * 4;
    names = frames + frameCount * 8 + celHw * 2;

    bank->block = raw;
    bank->animCount = animCount;
    bank->celData = (u16 *)(void *)(frames + frameCount * 8);
    bank->anims = calloc(animCount, sizeof *bank->anims);
    bank->names = calloc(animCount, sizeof *bank->names);

    for (i = 0; i < animCount; i++) {
        u32 a = rd32(animStart + i * 4);
        u32 b = rd32(animStart + (i + 1) * 4);
        u32 n = b - a, k;
        AnimFrame *fr = calloc(n + 1, sizeof *fr);

        for (k = 0; k < n; k++) {
            u32 celIndex = rd32(frames + (a + k) * 8);
            u32 duration = rd32(frames + (a + k) * 8 + 4);
            fr[k].cel = bank->celData + rd32(celOff + celIndex * 4);
            fr[k].duration = (u8)duration;
        }
        fr[n].cel = 0;
        bank->anims[i] = fr;
        bank->names[i] = (char *)(uintptr_t)names;
        names += strlen((const char *)names) + 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    const char *scenePath = (argc > 1) ? argv[1] : "/tmp/title_scene.bin";
    const char *animPath = (argc > 2) ? argv[2] : "assets/title_anim.bin";
    const char *outPrefix = (argc > 3) ? argv[3] : "preview/sprites";
    int frames = (argc > 4) ? atoi(argv[4]) : 4;
    int step = (argc > 5) ? atoi(argv[5]) : 16;
    int scale = (argc > 6) ? atoi(argv[6]) : 3;

    FILE *f;
    u8 head[SCENE_HEADER];
    GbaVideoState st;
    u8 *vram;
    u32 *oam;
    Sprite *sprites;
    SpriteHandler handler;
    AnimBank bank;
    uint32_t *frame;
    size_t o;
    int n, tick = 0;

    f = fopen(scenePath, "rb");
    if (!f) { perror(scenePath); return 1; }
    if (fread(head, 1, sizeof head, f) != sizeof head || memcmp(head, "WWSC", 4) != 0) {
        fprintf(stderr, "%s: not a scene blob\n", scenePath); return 1;
    }

    memset(&st, 0, sizeof st);
    o = 8;
    memcpy(&st.dispcnt, head + o, 2); o += 4;
    memcpy(st.bgcnt, head + o, 8); o += 8;
    memcpy(st.bg_hofs, head + o, 8); o += 8;
    memcpy(st.bg_vofs, head + o, 8); o += 8;
    memcpy(st.bg_palette, head + o, 512); o += 512;
    memcpy(st.obj_palette, head + o, 512);

    vram = malloc(GBA_VRAM_SIZE);
    if (!vram || fread(vram, 1, GBA_VRAM_SIZE, f) != GBA_VRAM_SIZE) {
        fprintf(stderr, "%s: truncated VRAM\n", scenePath); return 1;
    }
    fclose(f);

    if (anim_bank_load(&bank, animPath) != 0) return 1;

    oam = calloc(MAX_OBJ * 2, sizeof *oam);
    sprites = calloc(MAX_SPRITES, sizeof *sprites);
    sprite_handler_create(&handler, MAX_OBJ, oam, MAX_SPRITES, sprites);

    /* Lay the title screen's animated pieces out across the canvas. The z
     * values decide draw order: lower is nearer the front. */
    {
        struct { const char *want; s16 x, y; u16 z; } place[] = {
            { "title_anim_press_start", 120, 120, 0x1000 },
            { "title_anim_trash_can",    40,  60, 0x2000 },
            { "title_anim_hourglass",   200,  60, 0x2000 },
            { "title_anim_treasure_chest", 40, 110, 0x3000 },
            { "title_anim_purple_gem",  200, 110, 0x3000 },
        };
        size_t p, a;
        for (p = 0; p < sizeof place / sizeof *place; p++) {
            for (a = 0; a < bank.animCount; a++) {
                if (strcmp(bank.names[a], place[p].want) == 0) {
                    s16 id = sprite_create(&handler, bank.anims[a],
                                           place[p].x, place[p].y, place[p].z);
                    printf("  sprite %2d  %-30s at (%3d,%3d) z=0x%04X\n",
                           id, place[p].want, place[p].x, place[p].y, place[p].z);
                    break;
                }
            }
        }
    }

    st.dispcnt |= DISPCNT_OBJ_ENABLE | DISPCNT_OBJ_1D;
    frame = malloc(sizeof(uint32_t) * GBA_SCREEN_W * GBA_SCREEN_H);

    for (n = 0; n < frames; n++) {
        char path[512];
        int k, drawn = 0, i;

        for (k = 0; k < step; k++, tick++) {
            memset(oam, 0x22, MAX_OBJ * 2 * sizeof *oam);
            sprite_engine_emit(&handler);
        }

        for (i = 0; i < MAX_OBJ; i++)
            if ((oam[i * 2] & 0x0300) != 0x0200) drawn++;

        gba_ppu_render(&st, vram, (const u8 *)oam, frame);
        snprintf(path, sizeof path, "%s_%02d.png", outPrefix, n);
        if (write_png(path, frame, GBA_SCREEN_W, GBA_SCREEN_H, scale) != 0) {
            fprintf(stderr, "failed to write %s\n", path);
            return 1;
        }
        printf("frame %2d (tick %4d): %3d OAM entries -> %s\n", n, tick, drawn, path);
    }
    return 0;
}

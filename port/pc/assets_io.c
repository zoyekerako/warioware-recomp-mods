#include "assets_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCENE_HEADER_BYTES 1060

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static uint8_t *read_whole_file(const char *path, long *sizeOut)
{
    FILE *f = fopen(path, "rb");
    long size;
    uint8_t *raw;

    if (!f) {
        perror(path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);

    raw = malloc((size_t)size);
    if (!raw || fread(raw, 1, (size_t)size, f) != (size_t)size) {
        fprintf(stderr, "%s: short read\n", path);
        free(raw);
        fclose(f);
        return NULL;
    }
    fclose(f);
    if (sizeOut) {
        *sizeOut = size;
    }
    return raw;
}

int scene_blob_load(SceneBlob *scene, const char *path)
{
    FILE *f = fopen(path, "rb");
    uint8_t head[SCENE_HEADER_BYTES];
    size_t o = 8;   /* magic and version */

    memset(scene, 0, sizeof *scene);

    if (!f) {
        perror(path);
        return -1;
    }
    if (fread(head, 1, sizeof head, f) != sizeof head ||
        memcmp(head, "WWSC", 4) != 0) {
        fprintf(stderr, "%s: not a scene blob\n", path);
        fclose(f);
        return -1;
    }

    memcpy(&scene->video.dispcnt, head + o, 2); o += 4;   /* +2 padding */
    memcpy(scene->video.bgcnt, head + o, 8);    o += 8;
    memcpy(scene->video.bg_hofs, head + o, 8);  o += 8;
    memcpy(scene->video.bg_vofs, head + o, 8);  o += 8;
    memcpy(scene->video.bg_palette, head + o, 512);  o += 512;
    memcpy(scene->video.obj_palette, head + o, 512);

    scene->vram = malloc(GBA_VRAM_SIZE);
    scene->oam = malloc(GBA_OAM_SIZE);
    if (!scene->vram || !scene->oam) {
        fprintf(stderr, "%s: out of memory\n", path);
        scene_blob_free(scene);
        fclose(f);
        return -1;
    }

    if (fread(scene->vram, 1, GBA_VRAM_SIZE, f) != GBA_VRAM_SIZE) {
        fprintf(stderr, "%s: truncated VRAM\n", path);
        scene_blob_free(scene);
        fclose(f);
        return -1;
    }
    if (fread(scene->oam, 1, GBA_OAM_SIZE, f) != GBA_OAM_SIZE) {
        free(scene->oam);
        scene->oam = NULL;   /* the blob carried backgrounds only */
    }
    fclose(f);
    return 0;
}

void scene_blob_free(SceneBlob *scene)
{
    free(scene->vram);
    free(scene->oam);
    scene->vram = NULL;
    scene->oam = NULL;
}

int anim_bank_load(AnimBank *bank, const char *path)
{
    uint8_t *raw = read_whole_file(path, NULL);
    uint32_t celCount, animCount, celHalfwords, frameCount, i;
    const uint8_t *celOffsets, *animOffsets, *frames, *names;
    const uint16_t *celData;

    memset(bank, 0, sizeof *bank);
    if (!raw) {
        return -1;
    }
    if (memcmp(raw, "WWAN", 4) != 0) {
        fprintf(stderr, "%s: not an animation bank\n", path);
        free(raw);
        return -1;
    }

    celCount     = rd32(raw + 8);
    animCount    = rd32(raw + 12);
    celHalfwords = rd32(raw + 16);

    celOffsets  = raw + 20;
    animOffsets = celOffsets + celCount * 4;
    frameCount  = rd32(animOffsets + animCount * 4);
    frames      = animOffsets + (animCount + 1) * 4;
    celData     = (const uint16_t *)(const void *)(frames + frameCount * 8);
    names       = frames + frameCount * 8 + celHalfwords * 2;

    bank->block = raw;
    bank->animCount = animCount;
    bank->anims = calloc(animCount, sizeof *bank->anims);
    bank->names = calloc(animCount, sizeof *bank->names);
    if (!bank->anims || !bank->names) {
        anim_bank_free(bank);
        return -1;
    }

    for (i = 0; i < animCount; i++) {
        uint32_t first = rd32(animOffsets + i * 4);
        uint32_t last  = rd32(animOffsets + (i + 1) * 4);
        uint32_t n = last - first, k;
        AnimFrame *list = calloc(n + 1, sizeof *list);

        if (!list) {
            anim_bank_free(bank);
            return -1;
        }
        for (k = 0; k < n; k++) {
            uint32_t celIndex = rd32(frames + (first + k) * 8);
            uint32_t duration = rd32(frames + (first + k) * 8 + 4);

            list[k].cel = celData + rd32(celOffsets + celIndex * 4);
            list[k].duration = (uint8_t)duration;
        }
        list[n].cel = NULL;          /* terminator */

        bank->anims[i] = list;
        bank->names[i] = (char *)(void *)(uintptr_t)names;
        names += strlen((const char *)names) + 1;
    }
    return 0;
}

void anim_bank_free(AnimBank *bank)
{
    uint32_t i;

    if (bank->anims) {
        for (i = 0; i < bank->animCount; i++) {
            free(bank->anims[i]);
        }
    }
    free(bank->anims);
    free(bank->names);
    free(bank->block);
    memset(bank, 0, sizeof *bank);
}

int anim_bank_find(const AnimBank *bank, const char *name)
{
    uint32_t i;

    for (i = 0; i < bank->animCount; i++) {
        if (strcmp(bank->names[i], name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

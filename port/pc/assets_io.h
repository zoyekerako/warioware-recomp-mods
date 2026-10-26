/* Loaders for the two blob formats the extraction tools emit.
 *
 * "WWSC" is one scene: the display registers and palette shadows the game
 * would have flushed, followed by a VRAM and an OAM image.
 * "WWAN" is one animation bank: cels, and the frame lists that reference them.
 */
#ifndef ASSETS_IO_H
#define ASSETS_IO_H

#include "../renderer/gba_ppu.h"
#include "../renderer/sprite_engine.h"

typedef struct {
    GbaVideoState video;
    uint8_t *vram;   /* GBA_VRAM_SIZE bytes */
    uint8_t *oam;    /* GBA_OAM_SIZE bytes, or NULL when the blob had none */
} SceneBlob;

typedef struct {
    uint32_t animCount;
    AnimFrame **anims;   /* one NULL-cel terminated list per animation */
    char **names;
    void *block;         /* the raw file, owning every pointer above */
} AnimBank;

int scene_blob_load(SceneBlob *scene, const char *path);
void scene_blob_free(SceneBlob *scene);

int anim_bank_load(AnimBank *bank, const char *path);
void anim_bank_free(AnimBank *bank);

/* Index of the named animation, or -1. */
int anim_bank_find(const AnimBank *bank, const char *name);

#endif

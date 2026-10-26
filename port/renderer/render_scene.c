/* Loads a scene blob produced by tools/build_scene.py, renders one frame with
 * the software compositor and writes it out as a PNG.
 *
 * This is the headless verification path. A future SDL2 shell would call
 * gba_ppu_render() the same way and blit the result instead of encoding it. */

#include "gba_ppu.h"
#include "png_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HEADER_BYTES 1060

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s <scene.bin> <out.png> [scale]\n", argv[0]);
        return 2;
    }
    int scale = (argc > 3) ? atoi(argv[3]) : 1;

    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 1; }

    uint8_t head[HEADER_BYTES];
    if (fread(head, 1, sizeof head, f) != sizeof head) {
        fprintf(stderr, "%s: truncated header\n", argv[1]);
        fclose(f); return 1;
    }
    if (memcmp(head, "WWSC", 4) != 0) {
        fprintf(stderr, "%s: not a scene blob\n", argv[1]);
        fclose(f); return 1;
    }

    GbaVideoState st;
    memset(&st, 0, sizeof st);
    size_t o = 8;
    memcpy(&st.dispcnt, head + o, 2); o += 4;      /* +2 padding */
    memcpy(st.bgcnt, head + o, 8); o += 8;
    memcpy(st.bg_hofs, head + o, 8); o += 8;
    memcpy(st.bg_vofs, head + o, 8); o += 8;
    memcpy(st.bg_palette, head + o, 512); o += 512;
    memcpy(st.obj_palette, head + o, 512); o += 512;

    uint8_t *vram = malloc(GBA_VRAM_SIZE);
    uint8_t *oam = malloc(GBA_OAM_SIZE);
    if (!vram || !oam) { fprintf(stderr, "out of memory\n"); return 1; }

    if (fread(vram, 1, GBA_VRAM_SIZE, f) != GBA_VRAM_SIZE) {
        fprintf(stderr, "%s: truncated VRAM\n", argv[1]);
        fclose(f); return 1;
    }
    int have_oam = fread(oam, 1, GBA_OAM_SIZE, f) == GBA_OAM_SIZE;
    fclose(f);

    uint32_t *frame = malloc(sizeof(uint32_t) * GBA_SCREEN_W * GBA_SCREEN_H);
    if (!frame) { fprintf(stderr, "out of memory\n"); return 1; }

    gba_ppu_render(&st, vram, have_oam ? oam : NULL, frame);

    if (write_png(argv[2], frame, GBA_SCREEN_W, GBA_SCREEN_H, scale) != 0) {
        fprintf(stderr, "failed to write %s\n", argv[2]);
        return 1;
    }

    /* A quick signal that the frame is not just backdrop. */
    uint32_t first = frame[0];
    long distinct_rows = 0;
    for (int y = 0; y < GBA_SCREEN_H; ++y) {
        for (int x = 0; x < GBA_SCREEN_W; ++x) {
            if (frame[y * GBA_SCREEN_W + x] != first) { ++distinct_rows; break; }
        }
    }
    printf("rendered %dx%d (scale %d) -> %s\n",
           GBA_SCREEN_W * scale, GBA_SCREEN_H * scale, scale, argv[2]);
    printf("rows differing from the backdrop: %ld / %d\n", distinct_rows, GBA_SCREEN_H);

    free(frame); free(vram); free(oam);
    return 0;
}

/* Software compositor for the GBA tile/sprite pipeline.
 *
 * This deliberately consumes the same state that flush_graphics_buffer hands
 * to hardware once per frame -- display registers, palette shadows, VRAM and
 * OAM -- so the port can swap the hardware write for a call in here without
 * changing anything upstream.
 *
 * No external dependencies. Mode 0 (four text backgrounds) plus regular
 * sprites; affine backgrounds and windowing are not implemented yet.
 */
#ifndef GBA_PPU_H
#define GBA_PPU_H

#include <stdint.h>

#define GBA_SCREEN_W 240
#define GBA_SCREEN_H 160
#define GBA_VRAM_SIZE 0x18000
#define GBA_OAM_SIZE 0x400
#define GBA_OBJ_VRAM_OFFSET 0x10000

/* DISPCNT */
#define DISPCNT_MODE_MASK   0x0007
#define DISPCNT_FORCE_BLANK 0x0080
#define DISPCNT_BG_ENABLE(n) (0x0100 << (n))
#define DISPCNT_OBJ_ENABLE  0x1000
#define DISPCNT_OBJ_1D      0x0040

/* BGCNT */
#define BGCNT_PRIORITY(c)   ((c) & 3)
#define BGCNT_CHARBLOCK(c)  (((c) >> 2) & 3)
#define BGCNT_MOSAIC(c)     (((c) >> 6) & 1)
#define BGCNT_256COLOR(c)   (((c) >> 7) & 1)
#define BGCNT_SCREENBLOCK(c) (((c) >> 8) & 0x1F)
#define BGCNT_SIZE(c)       (((c) >> 14) & 3)

typedef struct {
    uint16_t dispcnt;
    uint16_t bgcnt[4];
    int16_t bg_hofs[4];
    int16_t bg_vofs[4];
    uint16_t bg_palette[256];  /* 16 banks of 16 RGB555 entries */
    uint16_t obj_palette[256];
} GbaVideoState;

/* Renders one frame into `out` as 0xAABBGGRR (little-endian RGBA8888),
 * GBA_SCREEN_W * GBA_SCREEN_H pixels. `oam` may be NULL to skip sprites. */
void gba_ppu_render(const GbaVideoState *state,
                    const uint8_t *vram,
                    const uint8_t *oam,
                    uint32_t *out);

/* RGB555 -> RGBA8888, replicating the high bits so 0x1F maps to 0xFF. */
uint32_t gba_rgb555_to_rgba(uint16_t color);

#endif /* GBA_PPU_H */

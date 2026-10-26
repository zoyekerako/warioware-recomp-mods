#include "gba_ppu.h"
#include <string.h>

/* A text background is built from 32x32-tile screenblocks. Sizes 1 and 3 place
 * a second block to the right, sizes 2 and 3 one below (and 3 a fourth after
 * that), so the map coordinate has to be split into block + in-block offset. */
static const int kBgWidthTiles[4]  = {32, 64, 32, 64};
static const int kBgHeightTiles[4] = {32, 32, 64, 64};

uint32_t gba_rgb555_to_rgba(uint16_t color)
{
    unsigned r = color & 0x1F;
    unsigned g = (color >> 5) & 0x1F;
    unsigned b = (color >> 10) & 0x1F;
    r = (r << 3) | (r >> 2);
    g = (g << 3) | (g >> 2);
    b = (b << 3) | (b >> 2);
    return 0xFF000000u | (b << 16) | (g << 8) | r;
}

/* Returns the palette index of one pixel of a tile, 0 meaning transparent. */
static unsigned tile_pixel(const uint8_t *vram, unsigned char_base,
                           unsigned tile, int x, int y, int is256)
{
    if (is256) {
        unsigned off = char_base + tile * 64u + (unsigned)y * 8u + (unsigned)x;
        if (off >= GBA_VRAM_SIZE) return 0;
        return vram[off];
    } else {
        unsigned off = char_base + tile * 32u + (unsigned)y * 4u + (unsigned)(x >> 1);
        if (off >= GBA_VRAM_SIZE) return 0;
        uint8_t packed = vram[off];
        /* low nibble is the left pixel */
        return (x & 1) ? (packed >> 4) : (packed & 0x0F);
    }
}

static void render_bg_layer(const GbaVideoState *st, const uint8_t *vram,
                            int layer, uint32_t *out, uint8_t *covered)
{
    uint16_t cnt = st->bgcnt[layer];
    unsigned char_base = (unsigned)BGCNT_CHARBLOCK(cnt) * 0x4000u;
    unsigned screen_base = (unsigned)BGCNT_SCREENBLOCK(cnt) * 0x800u;
    int is256 = BGCNT_256COLOR(cnt);
    int size = BGCNT_SIZE(cnt);
    int map_w = kBgWidthTiles[size];
    int map_h = kBgHeightTiles[size];

    int hofs = st->bg_hofs[layer];
    int vofs = st->bg_vofs[layer];

    for (int py = 0; py < GBA_SCREEN_H; ++py) {
        int sy = ((py + vofs) % (map_h * 8) + map_h * 8) % (map_h * 8);
        int ty = sy >> 3, fy = sy & 7;

        for (int px = 0; px < GBA_SCREEN_W; ++px) {
            int idx = py * GBA_SCREEN_W + px;
            if (covered[idx]) continue;

            int sx = ((px + hofs) % (map_w * 8) + map_w * 8) % (map_w * 8);
            int tx = sx >> 3, fx = sx & 7;

            /* Pick the screenblock this tile lives in. */
            unsigned block = 0;
            int bx = tx, by = ty;
            if (bx >= 32) { block += 1; bx -= 32; }
            if (by >= 32) { block += (map_w > 32) ? 2 : 1; by -= 32; }

            unsigned entry_off = screen_base + block * 0x800u
                               + ((unsigned)by * 32u + (unsigned)bx) * 2u;
            if (entry_off + 1 >= GBA_VRAM_SIZE) continue;

            uint16_t entry = (uint16_t)(vram[entry_off] | (vram[entry_off + 1] << 8));
            unsigned tile = entry & 0x03FF;
            int hflip = (entry >> 10) & 1;
            int vflip = (entry >> 11) & 1;
            unsigned bank = (entry >> 12) & 0x0F;

            int ix = hflip ? (7 - fx) : fx;
            int iy = vflip ? (7 - fy) : fy;

            unsigned pal = tile_pixel(vram, char_base, tile, ix, iy, is256);
            if (pal == 0) continue;  /* transparent */

            unsigned color = is256 ? pal : (bank * 16u + pal);
            out[idx] = gba_rgb555_to_rgba(st->bg_palette[color & 0xFF]);
            covered[idx] = 1;
        }
    }
}

/* OBJ dimensions indexed by [shape][size]. */
static const int kObjW[3][4] = {{8,16,32,64},{16,32,32,64},{8,8,16,32}};
static const int kObjH[3][4] = {{8,16,32,64},{8,8,16,32},{16,32,32,64}};

static void render_sprites(const GbaVideoState *st, const uint8_t *vram,
                           const uint8_t *oam, int priority,
                           uint32_t *out, uint8_t *covered)
{
    int obj_1d = (st->dispcnt & DISPCNT_OBJ_1D) != 0;

    /* OAM entry 0 is drawn last so lower indices win, matching hardware. */
    for (int i = 127; i >= 0; --i) {
        const uint8_t *e = oam + i * 8;
        uint16_t a0 = (uint16_t)(e[0] | (e[1] << 8));
        uint16_t a1 = (uint16_t)(e[2] | (e[3] << 8));
        uint16_t a2 = (uint16_t)(e[4] | (e[5] << 8));

        int affine = (a0 >> 8) & 1;
        int disabled = (a0 >> 9) & 1;
        if (!affine && disabled) continue;
        if (affine) continue;  /* rotation/scaling not implemented */

        int is256 = (a0 >> 13) & 1;
        int shape = (a0 >> 14) & 3;
        int sz = (a1 >> 14) & 3;
        if (shape > 2) continue;

        if (((a2 >> 10) & 3) != (unsigned)priority) continue;

        int w = kObjW[shape][sz], h = kObjH[shape][sz];
        int y = a0 & 0xFF;
        int x = a1 & 0x1FF;
        if (y >= GBA_SCREEN_H) y -= 256;
        if (x >= GBA_SCREEN_W) x -= 512;

        int hflip = (a1 >> 12) & 1;
        int vflip = (a1 >> 13) & 1;
        unsigned tile = a2 & 0x03FF;
        unsigned bank = (a2 >> 12) & 0x0F;

        int tiles_w = w / 8;
        for (int row = 0; row < h; ++row) {
            int py = y + row;
            if (py < 0 || py >= GBA_SCREEN_H) continue;
            int sy = vflip ? (h - 1 - row) : row;

            for (int col = 0; col < w; ++col) {
                int px = x + col;
                if (px < 0 || px >= GBA_SCREEN_W) continue;
                int idx = py * GBA_SCREEN_W + px;
                if (covered[idx]) continue;

                int sx = hflip ? (w - 1 - col) : col;

                /* 1D mapping lays the sprite's tiles out consecutively; 2D
                 * mapping keeps them on a 32-tile-wide grid in OBJ VRAM. */
                unsigned t = obj_1d
                    ? tile + (unsigned)((sy >> 3) * tiles_w + (sx >> 3)) * (is256 ? 2u : 1u)
                    : tile + (unsigned)(sy >> 3) * 32u + (unsigned)(sx >> 3) * (is256 ? 2u : 1u);

                unsigned pal = tile_pixel(vram, GBA_OBJ_VRAM_OFFSET, t,
                                          sx & 7, sy & 7, is256);
                if (pal == 0) continue;

                unsigned color = is256 ? pal : (bank * 16u + pal);
                out[idx] = gba_rgb555_to_rgba(st->obj_palette[color & 0xFF]);
                covered[idx] = 1;
            }
        }
    }
}

void gba_ppu_render(const GbaVideoState *state, const uint8_t *vram,
                    const uint8_t *oam, uint32_t *out)
{
    uint8_t covered[GBA_SCREEN_W * GBA_SCREEN_H];
    memset(covered, 0, sizeof covered);

    if (state->dispcnt & DISPCNT_FORCE_BLANK) {
        for (int i = 0; i < GBA_SCREEN_W * GBA_SCREEN_H; ++i) out[i] = 0xFFFFFFFFu;
        return;
    }

    /* Backdrop is BG palette entry 0. */
    uint32_t backdrop = gba_rgb555_to_rgba(state->bg_palette[0]);
    for (int i = 0; i < GBA_SCREEN_W * GBA_SCREEN_H; ++i) out[i] = backdrop;

    /* Front to back: for each priority, sprites sit above the backgrounds of
     * the same priority; among equal-priority backgrounds the lower index wins.
     * Painting front first and masking with `covered` gives that ordering. */
    for (int prio = 0; prio < 4; ++prio) {
        if (oam && (state->dispcnt & DISPCNT_OBJ_ENABLE))
            render_sprites(state, vram, oam, prio, out, covered);

        for (int layer = 0; layer < 4; ++layer) {
            if (!(state->dispcnt & DISPCNT_BG_ENABLE(layer))) continue;
            if (BGCNT_PRIORITY(state->bgcnt[layer]) != prio) continue;
            render_bg_layer(state, vram, layer, out, covered);
        }
    }
}

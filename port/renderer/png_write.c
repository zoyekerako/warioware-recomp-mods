#include "png_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

static uint32_t be32(uint32_t v)
{
    return ((v & 0xFFu) << 24) | ((v & 0xFF00u) << 8)
         | ((v >> 8) & 0xFF00u) | ((v >> 24) & 0xFFu);
}

static void put_chunk(FILE *f, const char *tag, const uint8_t *data, size_t len)
{
    uint32_t n = be32((uint32_t)len);
    fwrite(&n, 4, 1, f);

    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, (const Bytef *)tag, 4);
    if (len) crc = crc32(crc, data, (uInt)len);

    fwrite(tag, 1, 4, f);
    if (len) fwrite(data, 1, len, f);

    uint32_t c = be32((uint32_t)crc);
    fwrite(&c, 4, 1, f);
}

int write_png(const char *path, const uint32_t *rgba, int w, int h, int scale)
{
    if (scale < 1) scale = 1;
    int ow = w * scale, oh = h * scale;

    size_t raw_len = (size_t)oh * (1 + (size_t)ow * 3);
    uint8_t *raw = malloc(raw_len);
    if (!raw) return -1;

    size_t p = 0;
    for (int y = 0; y < oh; ++y) {
        raw[p++] = 0;  /* filter: None */
        const uint32_t *row = rgba + (size_t)(y / scale) * w;
        for (int x = 0; x < ow; ++x) {
            uint32_t px = row[x / scale];
            raw[p++] = (uint8_t)(px & 0xFF);
            raw[p++] = (uint8_t)((px >> 8) & 0xFF);
            raw[p++] = (uint8_t)((px >> 16) & 0xFF);
        }
    }

    uLongf comp_len = compressBound((uLong)raw_len);
    uint8_t *comp = malloc(comp_len);
    if (!comp) { free(raw); return -1; }
    if (compress2(comp, &comp_len, raw, (uLong)raw_len, 9) != Z_OK) {
        free(raw); free(comp); return -1;
    }
    free(raw);

    FILE *f = fopen(path, "wb");
    if (!f) { free(comp); return -1; }

    static const uint8_t sig[8] = {0x89,'P','N','G','\r','\n',0x1A,'\n'};
    fwrite(sig, 1, 8, f);

    uint8_t ihdr[13];
    uint32_t bw = be32((uint32_t)ow), bh = be32((uint32_t)oh);
    memcpy(ihdr, &bw, 4);
    memcpy(ihdr + 4, &bh, 4);
    ihdr[8] = 8;    /* bit depth */
    ihdr[9] = 2;    /* truecolour */
    ihdr[10] = ihdr[11] = ihdr[12] = 0;
    put_chunk(f, "IHDR", ihdr, sizeof ihdr);
    put_chunk(f, "IDAT", comp, comp_len);
    put_chunk(f, "IEND", NULL, 0);

    fclose(f);
    free(comp);
    return 0;
}


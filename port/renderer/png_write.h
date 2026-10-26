#ifndef PNG_WRITE_H
#define PNG_WRITE_H
#include <stdint.h>
/* Writes an RGBA framebuffer as a truecolour PNG, nearest-neighbour scaled.
   Returns 0 on success. */
int write_png(const char *path, const uint32_t *rgba, int w, int h, int scale);
#endif

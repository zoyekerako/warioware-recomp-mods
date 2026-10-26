/* The hardware seam.
 *
 * On the GBA the game never draws directly: it writes a shadow of the display
 * state into IWRAM and one function -- flush_graphics_buffer -- pushes that
 * shadow to the PPU once per frame with three DMA bursts.  Everything else the
 * hardware provides is read through four narrow doors: the key register, the
 * DMA3 channel, the VBlank wait, and the sound driver.
 *
 * That is why this layer is small.  It owns the memory the hardware would own
 * and offers those same doors, so decompiled code can be linked against it
 * unchanged as more of it becomes available.
 */
#ifndef GBA_HW_H
#define GBA_HW_H

#include <stdint.h>

#define GBA_VRAM_BYTES 0x18000
#define GBA_OAM_BYTES  0x400
#define GBA_PAL_ENTRIES 256

/* Key bits, as the KEYINPUT register numbers them. */
enum {
    GBA_KEY_A = 0, GBA_KEY_B, GBA_KEY_SELECT, GBA_KEY_START,
    GBA_KEY_RIGHT, GBA_KEY_LEFT, GBA_KEY_UP, GBA_KEY_DOWN,
    GBA_KEY_R, GBA_KEY_L
};

/* The blocks that live behind 0x05000000, 0x06000000 and 0x07000000. */
extern uint8_t  gba_vram[GBA_VRAM_BYTES];
extern uint8_t  gba_oam[GBA_OAM_BYTES];
extern uint16_t gba_pal_bg[GBA_PAL_ENTRIES];
extern uint16_t gba_pal_obj[GBA_PAL_ENTRIES];

/* KEYINPUT is active-low: a set bit means the button is up.  The game reads it
   as ~REG_KEY, so this returns the raw register value, not the pressed mask. */
uint16_t gba_hw_keyinput(void);
void gba_hw_set_key(int key, int down);

/* DMA3 is the only channel the game uses for bulk copies.  `unit` is 0x10 for
   halfword transfers and 0x20 for words, matching the original call sites. */
void gba_hw_dma3_set(const void *src, void *dst, uint32_t bytes, uint16_t unit,
                     uint32_t bytesPerBurst);
void gba_hw_dma3_fill(uint32_t value, void *dst, uint32_t bytes, uint16_t unit,
                      uint32_t bytesPerBurst);

/* Frame pacing.  The GBA refreshes at 280896 cycles of a 16.78 MHz clock. */
#define GBA_FRAME_HZ 59.7275
void gba_hw_frame_begin(void);
void gba_hw_frame_wait(void);

#endif

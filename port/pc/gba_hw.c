#include "gba_hw.h"

#include <SDL.h>
#include <string.h>

uint8_t  gba_vram[GBA_VRAM_BYTES];
uint8_t  gba_oam[GBA_OAM_BYTES];
uint16_t gba_pal_bg[GBA_PAL_ENTRIES];
uint16_t gba_pal_obj[GBA_PAL_ENTRIES];

/* Active-low, so "nothing pressed" is all ones in the low ten bits. */
static uint16_t key_state = 0x03FF;

uint16_t gba_hw_keyinput(void)
{
    return key_state;
}

void gba_hw_set_key(int key, int down)
{
    if (key < 0 || key > GBA_KEY_L) {
        return;
    }
    if (down) {
        key_state &= (uint16_t)~(1u << key);
    } else {
        key_state |= (uint16_t)(1u << key);
    }
}

/* The hardware transfers in fixed-size units and the source may be held still
   for a fill; both are just memory moves here.  bytesPerBurst only mattered for
   interrupt latency on real hardware, so it is accepted and ignored. */
void gba_hw_dma3_set(const void *src, void *dst, uint32_t bytes, uint16_t unit,
                     uint32_t bytesPerBurst)
{
    (void)unit;
    (void)bytesPerBurst;
    memcpy(dst, src, bytes);
}

void gba_hw_dma3_fill(uint32_t value, void *dst, uint32_t bytes, uint16_t unit,
                      uint32_t bytesPerBurst)
{
    (void)bytesPerBurst;

    if (unit == 0x20) {
        uint32_t *p = dst;
        uint32_t n = bytes / 4;
        while (n--) {
            *p++ = value;
        }
    } else {
        uint16_t *p = dst;
        uint32_t n = bytes / 2;
        uint16_t v = (uint16_t)value;
        while (n--) {
            *p++ = v;
        }
    }
}

/* Frame pacing against an absolute deadline.
 *
 * Measuring each frame from the previous one lets every overshoot accumulate,
 * and SDL_Delay on a desktop scheduler routinely overshoots by a millisecond,
 * which is how a 59.73 Hz target drifts down to 56 Hz.  Advancing a fixed
 * deadline instead makes the average rate exact: a frame that ran long is paid
 * back by the next one waiting less.
 */
static Uint64 next_deadline;
static double ticks_per_frame;

static double frame_ticks(void)
{
    if (ticks_per_frame == 0.0) {
        ticks_per_frame = (double)SDL_GetPerformanceFrequency() / GBA_FRAME_HZ;
    }
    return ticks_per_frame;
}

void gba_hw_frame_begin(void)
{
    if (next_deadline == 0) {
        next_deadline = SDL_GetPerformanceCounter() + (Uint64)frame_ticks();
    }
}

void gba_hw_frame_wait(void)
{
    Uint64 freq = SDL_GetPerformanceFrequency();
    Uint64 now = SDL_GetPerformanceCounter();
    double per_frame = frame_ticks();

    if (now < next_deadline) {
        /* Sleep the bulk, spin the last two milliseconds: sleeping the whole
           way is what overshoots, and spinning the whole way burns a core. */
        double remaining_ms = (double)(next_deadline - now) * 1000.0 / (double)freq;

        if (remaining_ms > 2.0) {
            SDL_Delay((Uint32)(remaining_ms - 2.0));
        }
        while (SDL_GetPerformanceCounter() < next_deadline) {
            /* spin */
        }
    }

    next_deadline += (Uint64)per_frame;

    /* After a long stall -- a window drag, a breakpoint -- catching up frame by
       frame would fast-forward the game.  Drop the backlog instead. */
    now = SDL_GetPerformanceCounter();
    if (next_deadline < now) {
        next_deadline = now + (Uint64)per_frame;
    }
}

/* The PC replacement for src/code_08000f10.c.
 *
 * That file is the game's entire hardware abstraction layer -- it is the only
 * translation unit that touches REG_ registers -- so it is the one file a port
 * replaces rather than links.  The byte-exact original stays in the decomp and
 * is still verified against the ROM; this is its counterpart on a machine with
 * no DMA controller and no VBlank interrupt.
 *
 * The arithmetic halves (the random generator and the sine interpolation) are
 * reproduced statement for statement, because the game's behaviour depends on
 * their exact results.  Only the four hardware entry points differ.
 */
#include "global.h"
#include "types.h"
#include "src/code_08000f10.h"

#include "../../port/pc/gba_hw.h"

#include <string.h>
#include <execinfo.h>
#include <unistd.h>

/* ---- VBlank ----------------------------------------------------------- */

static volatile u16 sVBlankFlag;
static void (*sVBlankCallback)(void);

void func_08000F10(void)
{
    sVBlankFlag = FALSE;
    sVBlankCallback = NULL;
}

/* The SDL frame loop calls this where the hardware raised the interrupt. */
void func_08000F28(void)
{
    if (sVBlankCallback != NULL) {
        sVBlankCallback();
    }
    sVBlankFlag = TRUE;
}

/* On hardware this spun until the interrupt fired.  Here the frame loop has
   already run func_08000F28 before handing control back, so the flag is set
   and there is nothing to wait for -- spinning would hang. */
void vblank_wait(void)
{
    sVBlankFlag = FALSE;
}

void func_08000F74(void (*callback)(void))
{
    sVBlankCallback = callback;
}

/* ---- Input ------------------------------------------------------------ */

/* Identical to the original; only the source of `keys` differs. */
void update_key_buffers(void)
{
    u16 keys = (u16)(~gba_hw_keyinput());

    gPressedKeys = keys & ~gCurrentKeys;
    gCurrentKeys = keys;

    if (gPressedKeys != 0) {
        gRepeatedKeys = gPressedKeys;
        gKeyRepeatTimer = 0;
    }
    if (gPressedKeys == 0) {
        gRepeatedKeys = 0;
        if (gPrevKeys == keys) {
            gKeyRepeatTimer++;
            if (gKeyRepeatTimer > 14) {
                gKeyRepeatTimer = 12;
                gRepeatedKeys = keys;
            }
        } else {
            gKeyRepeatTimer = 0;
        }
        gPrevKeys = gCurrentKeys;
    }
}

/* ---- Block transfers --------------------------------------------------- */

/* No GBA DMA3 transfer can exceed the largest addressable block, so a larger
   size means the operands are wrong -- today usually because a ROM data symbol
   is still a stub and a pointer difference is meaningless.  Refusing the copy
   turns an undebuggable fault into a named diagnostic. */
#define WWPC_DMA_MAX 0x40000

/* stdio is shadowed for the game build -- see pc/compat/stdio.h -- so the
   diagnostic goes out through write(2). */
static void wwpc_say(const char *s)
{
    u32 n = 0;

    while (s[n]) {
        n++;
    }
    write(2, s, n);
}

static void wwpc_reject(const char *what, u32 bytes)
{
    static int reported;
    char digits[12];
    int i = (int)sizeof digits;

    if (reported >= 8) {
        return;
    }
    reported++;

    digits[--i] = '\0';
    do {
        digits[--i] = (char)('0' + bytes % 10);
        bytes /= 10;
    } while (bytes != 0);

    wwpc_say("[hal] ");
    wwpc_say(what);
    wwpc_say(": ");
    wwpc_say(&digits[i]);
    wwpc_say(" Bytes abgelehnt -- Operanden noch Stub\n");
    /* Same service as the function stubs: the call path names what asked. */
    wwpc_say("Aufrufpfad:\n");
    {
        void *frames[20];
        int n = backtrace(frames, 20);
        backtrace_symbols_fd(frames, n, 2);
    }
}

/* The original programmed DMA3 and let the controller run; the burst size only
   bounded interrupt latency, so it has no effect on the result here. */
void dma3_set(const void *source, void *destination, u32 bytesToSet, u16 unit,
              u32 bytesPerInterrupt)
{
    (void)unit;
    (void)bytesPerInterrupt;

    if (bytesToSet > WWPC_DMA_MAX) {
        wwpc_reject("dma3_set", bytesToSet);
        return;
    }
    memcpy(destination, source, bytesToSet);
}

void dma3_fill(u32 value, void *destination, u32 bytesToFill, u16 unit,
               u32 bytesPerInterrupt)
{
    (void)bytesPerInterrupt;

    if (bytesToFill > WWPC_DMA_MAX) {
        wwpc_reject("dma3_fill", bytesToFill);
        return;
    }

    if (unit == 0x20) {
        u32 *p = destination;
        u32 n = bytesToFill / 4;

        while (n--) {
            *p++ = value;
        }
    } else {
        u16 *p = destination;
        u32 n = bytesToFill / 2;
        u16 v = (u16)value;

        while (n--) {
            *p++ = v;
        }
    }
}

/* ---- Maths ------------------------------------------------------------- */

static u16 sRandomSeed;

void set_random_seed(u16 seed)
{
    sRandomSeed = seed;
}

u16 get_random_u16(void)
{
    sRandomSeed = (sRandomSeed * 109) + 1021;
    return sRandomSeed;
}

u16 get_random_range(u16 max)
{
    sRandomSeed = (sRandomSeed * 109) + 1021;
    return (sRandomSeed * max) >> 16;
}

u32 sine_lerp(u32 fullAngle)
{
    u32 a0, a1, af;
    u8 angle;

    fullAngle &= 0xFFF;
    angle = (fullAngle >> 4);

    a0 = angle & 0xFF;
    a1 = (angle + 1) & 0xFF;
    af = fullAngle & 0xF;
    return ((gSineTable[a0] * (0x10 - af)) + (gSineTable[a1] * af)) >> 4;
}

u32 cosine_lerp(u32 fullAngle)
{
    u32 a0, a1, af;
    u8 angle;

    fullAngle &= 0xFFF;
    angle = (fullAngle >> 4);

    a0 = angle & 0xFF;
    a1 = (angle + 1) & 0xFF;
    af = fullAngle & 0xF;

    return ((gCosineTable[a0] * (0x10 - af)) + (gCosineTable[a1] * af)) >> 4;
}

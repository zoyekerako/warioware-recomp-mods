#include "code_080062e4.h"

asm(".include \"include/gba.inc\"");

#include "asm/code_080062e4/asm_080062e4.s"

#include "asm/code_080062e4/asm_080063b4.s"

#include "asm/code_080062e4/asm_08006410.s"

#include "asm/code_080062e4/asm_08006418.s"

#include "asm/code_080062e4/asm_0800656c.s"

#include "asm/code_080062e4/asm_080065c0.s"

#include "asm/code_080062e4/asm_08006610.s"

#include "asm/code_080062e4/asm_08006648.s"

#include "asm/code_080062e4/asm_08006680.s"

#include "asm/code_080062e4/asm_08006700.s"

#include "asm/code_080062e4/asm_08006734.s"

#include "asm/code_080062e4/asm_0800673c.s"

#include "asm/code_080062e4/asm_08006790.s"

#include "asm/code_080062e4/asm_080067a4.s"

#include "asm/code_080062e4/asm_08006828.s"

#include "asm/code_080062e4/asm_0800687c.s"

#include "asm/code_080062e4/asm_08006918.s"

#include "asm/code_080062e4/asm_0800695c.s"

/* Blanks the display and the first backdrop colour. */
void func_080069F4(void) {
    REG_DISPCNT = 0;
    *(u16 *)PaletteRAMBase = 0;
}

/* Resets the graphics buffer: clears the display flags and wipes the register
   shadow from BG_CNT up to the palettes. */
void func_08006A04(void) {
    gGraphicsBuffer.unk854_1 = FALSE;
    gGraphicsBuffer.DISPCNT = 0;
    gGraphicsBuffer.unk854_2 = FALSE;
    gGraphicsBuffer.unk854_4 = FALSE;
    gGraphicsBuffer.unk854_5 = 0;
    gGraphicsBuffer.unk858 = 0;
    dma3_fill(0, gGraphicsBuffer.BG_CNT, 0x50, 0x20, 0x100);
}

#include "asm/code_080062e4/asm_08006a5c.s"

#include "asm/code_080062e4/asm_08006b00.s"

void func_08006B68(void) {
    dma3_fill(0x22222222, D_03004454, 0x400, 0x20, 0x100);
}

void func_08006B90(u16 value) {
    dma3_fill(value | (value << 16), D_03004054, 0x400, 0x20, 0x100);
}

#include "asm/code_080062e4/asm_08006bb8.s"

#include "asm/code_080062e4/asm_08006c40.s"

#include "asm/code_080062e4/asm_08006cc8.s"

#include "asm/code_080062e4/asm_08006ce8.s"

#include "asm/code_080062e4/asm_08006db8.s"

#include "asm/code_080062e4/asm_08006e94.s"

#include "asm/code_080062e4/asm_08006eb4.s"

#include "asm/code_080062e4/asm_08006ee0.s"

#include "asm/code_080062e4/asm_08006f04.s"

void func_08006F28(void) {
    sprite_handler_create(gSpriteHandler, 0x80, D_03004454, 0xC8, D_083A3D94 + 1);
    func_08001A70(0x20, D_03004454);
}

#include "asm/code_080062e4/asm_08006f68.s"

#include "asm/code_080062e4/asm_08006f84.s"

#include "asm/code_080062e4/asm_08006fc0.s"

#include "asm/code_080062e4/asm_08007000.s"

#include "asm/code_080062e4/asm_0800701c.s"

#include "asm/code_080062e4/asm_080070d0.s"

#include "asm/code_080062e4/asm_0800710c.s"

#include "asm/code_080062e4/asm_08007164.s"

#include "asm/code_080062e4/asm_0800719c.s"

#include "asm/code_080062e4/asm_08007204.s"

#include "asm/code_080062e4/asm_0800727c.s"

#include "asm/code_080062e4/asm_0800730c.s"

#include "asm/code_080062e4/asm_080073a8.s"

#include "asm/code_080062e4/asm_08007408.s"

#include "asm/code_080062e4/asm_08007474.s"

#include "asm/code_080062e4/asm_0800750c.s"

#include "asm/code_080062e4/asm_0800755c.s"

#include "asm/code_080062e4/asm_080075dc.s"

#include "asm/code_080062e4/asm_08007634.s"

#include "asm/code_080062e4/asm_080076c0.s"

#include "asm/code_080062e4/asm_08007724.s"

/* Copies the division helper from ROM into IWRAM, as func_08002620 does for
   the square root. */
void func_0800774C(void) {
    volatile u32 *dma = &REG_DMA3SAD;

    dma[0] = (u32)fast_udivsi3_rom;
    dma[1] = (u32)D_03000C18;
    dma[2] = ((u32)gfx_decompress_rom - (u32)fast_udivsi3_rom) / 4
             | ((DMACNT_SIZE | DMACNT_ENABLE) << 16);
    dma[2];

    D_03003FF0 = D_03000C18;
}

#include "asm/code_080062e4/asm_08007780.s"

#include "asm/code_080062e4/asm_080077b8.s"

#include "asm/code_080062e4/asm_08007830.s"

#include "asm/code_080062e4/asm_080078a8.s"

#include "asm/code_080062e4/asm_0800792c.s"

#include "asm/code_080062e4/asm_080079a4.s"

#include "asm/code_080062e4/asm_080079e0.s"

#include "asm/code_080062e4/asm_08007ad4.s"

#include "asm/code_080062e4/asm_08007af8.s"

#include "asm/code_080062e4/asm_08007b24.s"

#include "asm/code_080062e4/asm_08007b5c.s"

#include "asm/code_080062e4/asm_08007b98.s"

#include "asm/code_080062e4/asm_08007bf4.s"

#include "asm/code_080062e4/asm_08007ca4.s"

#include "asm/code_080062e4/asm_08007ce8.s"

#include "asm/code_080062e4/asm_08007db0.s"

#include "asm/code_080062e4/asm_08007dcc.s"

/* Runs a function once, `delay` beats from now. */
s32 schedule_function_call(u16 memID, void *function, s32 param, u32 delay) {
    struct ScheduledCall info;

    info.function = function;
    info.param = param;
    info.delay = delay;

    return start_new_task(memID, &D_083A4B38, &info, 0, 0);
}

#include "asm/code_080062e4/asm_08007e18.s"

#include "asm/code_080062e4/asm_08007e68.s"

#include "asm/code_080062e4/asm_08007e8c.s"

void func_08007EA0(void) {
    D_0300485C = NULL;
}

#include "asm/code_080062e4/asm_08007eac.s"

#include "asm/code_080062e4/asm_08007edc.s"

#include "asm/code_080062e4/asm_08007f20.s"

#include "asm/code_080062e4/asm_08007f48.s"

#include "asm/code_080062e4/asm_08007fa4.s"

#include "asm/code_080062e4/asm_08007fc0.s"

#include "asm/code_080062e4/asm_08007fdc.s"

#include "asm/code_080062e4/asm_08008038.s"

#include "asm/code_080062e4/asm_08008058.s"

#include "asm/code_080062e4/asm_0800806c.s"

#include "asm/code_080062e4/asm_08008130.s"


#include "bitmap_font.h"
#include "scenes.h"
#include "src/scenes/gameplay.h"

asm(".include \"include/gba.inc\"");

void func_0800B77C(u8* source, u8* destination) {
    u8* dst;
    u8* src;
    u32 i;

    src = source;
    dst = destination;
    
    for(i = 0; i < 2; i++){
        dma3_set(src, dst, 0x20, 0x10, 0x100);
        src += 0x40;
        dst += 0x400;
    }
}

void func_0800B7B0(u8* source, u8* destination) {
    u8* dst;
    u8* src;
    u32 i;

    src = source;
    dst = destination;

    for(i = 0; i < 2; i++){
        dma3_set(src, dst, 0x40, 0x10, 0x100);
        src += 0x40;
        dst += 0x400;
    }
}

u8 bmp_font_obj_parse_hex_digit(char c) {
    if ((c >= '0') && (c <= '9')) {
        return c - '0';
    }

    if ((c >= 'a') && (c <= 'f')) {
        return c - 'a' + 10;
    }

    if ((c >= 'A') && (c <= 'F')) {
        return c - 'A' + 10;
    }

    return 0;
}

#include "asm/bitmap_font/asm_0800b828.s"

#include "asm/bitmap_font/asm_0800ba78.s"

#include "asm/bitmap_font/asm_0800bb74.s"

/* Picks the entry for the active language out of a pointer table. */
void func_0800BBB4(void **byLanguage) {
    func_0800BB74(byLanguage[get_current_language()]);
}

#include "asm/bitmap_font/asm_0800bbcc.s"

void func_0800BC0C(void) {
}

#include "asm/bitmap_font/asm_0800bc10.s"

#include "asm/bitmap_font/asm_0800bc50.s"

void func_0800BC90(void) {
    if (gGameplayData.unk7_3) {
        func_0800BC50();
    }
}

void func_0800BCAC(u32 value) {
    gGameplayData.unk7_3 = value;
}

#include "asm/bitmap_font/asm_0800bcc8.s"

#include "asm/bitmap_font/asm_0800bd90.s"

/* Maps gGameplayData.unk195: 1..3 -> 1, 4 -> 2, else 0.  The ROM splits the
   range at 3 before testing the low bound; agbcc orders the two tests the
   other way round, so this stays as assembly for now. */
#include "asm/bitmap_font/asm_0800bec0.s"
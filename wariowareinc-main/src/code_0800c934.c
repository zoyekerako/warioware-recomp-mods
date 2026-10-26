#include "src/code_0800c934.h"

asm(".include \"include/gba.inc\"");

#include "asm/code_0800c934/asm_0800c934.s"

#include "asm/code_0800c934/asm_0800c974.s"

#include "asm/code_0800c934/asm_0800c9a4.s"

/* Part of the beatscript-scene reset: clears one flag and re-arms the counter
   at unk16A. */
void func_0800C9C0(void) {
    gBeatscriptScene.unk1_b4 = FALSE;
    gBeatscriptScene.unk168 = 0;
    gBeatscriptScene.unk16A = 0x18;
}

#include "asm/code_0800c934/asm_0800c9ec.s"

#include "asm/code_0800c934/asm_0800ca5c.s"

#include "asm/code_0800c934/asm_0800ca80.s"

void func_0800CAA4(u16 value) {
    gBeatscriptScene.unk16A = value;
}

void func_0800CAB8(s16 value) {
    gBeatscriptScene.unk168 = value;
}

#include "asm/code_0800c934/asm_0800cacc.s"

#include "asm/code_0800c934/asm_0800cb00.s"

#include "asm/code_0800c934/asm_0800cb34.s"

void func_0800CBA4(void) {
    gBeatscriptScene.interpolatingTempo = FALSE;
}

#include "asm/code_0800c934/asm_0800cbb4.s"

#include "asm/code_0800c934/asm_0800cc4c.s"

#include "asm/code_0800c934/asm_0800cc9c.s"

void func_0800CCB4(void) {
    gBeatscriptScene.interpolatingMusicPitch = FALSE;
}

#include "asm/code_0800c934/asm_0800ccc8.s"

#include "asm/code_0800c934/asm_0800cd4c.s"

#include "asm/code_0800c934/asm_0800cd94.s"

void func_0800CDB0(u32 value) {
    gBeatscriptScene.musicInterpolationEnabled = value;
}

#include "asm/code_0800c934/asm_0800cdcc.s"

#include "asm/code_0800c934/asm_0800ce1c.s"

#include "asm/code_0800c934/asm_0800ce6c.s"

#include "asm/code_0800c934/asm_0800ce90.s"

#include "asm/code_0800c934/asm_0800cea4.s"

void func_0800CEF8(void *ptr) {
    mem_heap_dealloc(ptr);
    func_08003FB8();
}

#include "asm/code_0800c934/asm_0800cf08.s"

#include "asm/code_0800c934/asm_0800cf3c.s"

#include "asm/code_0800c934/asm_0800cf5c.s"

#include "asm/code_0800c934/asm_0800cf7c.s"

#include "asm/code_0800c934/asm_0800cf9c.s"

#include "asm/code_0800c934/asm_0800cfbc.s"

#include "asm/code_0800c934/asm_0800cfdc.s"

#include "asm/code_0800c934/asm_0800cffc.s"

#include "asm/code_0800c934/asm_0800d224.s"

#include "asm/code_0800c934/asm_0800d23c.s"

#include "asm/code_0800c934/asm_0800d254.s"

#include "asm/code_0800c934/asm_0800d320.s"

#include "asm/code_0800c934/asm_0800d38c.s"

#include "asm/code_0800c934/asm_0800d3b8.s"

#include "asm/code_0800c934/asm_0800d3cc.s"

#include "asm/code_0800c934/asm_0800d68c.s"

#include "asm/code_0800c934/asm_0800d710.s"

#include "asm/code_0800c934/asm_0800d75c.s"

#include "asm/code_0800c934/asm_0800d764.s"

#include "asm/code_0800c934/asm_0800d76c.s"

#include "asm/code_0800c934/asm_0800d774.s"

#include "asm/code_0800c934/asm_0800d800.s"

#include "asm/code_0800c934/asm_0800d9c4.s"

#include "asm/code_0800c934/asm_0800da9c.s"

#include "asm/code_0800c934/asm_0800dad8.s"

#include "asm/code_0800c934/asm_0800daec.s"

#include "asm/code_0800c934/asm_0800daf8.s"


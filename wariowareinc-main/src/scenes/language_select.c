#include "src/scenes/language_select.h"
#include "src/main.h"

asm(".include \"include/gba.inc\"");

extern struct SubScene D_083AD90C;

void func_08016E6C(void) {
    if (D_030035E0 != 0) {
        func_08016CBC(&D_083AD90C);
    }

    if (func_08016D00() != 0) {
        gCurrentScene = GLOBAL_SCENE_INTRO;
    }
}

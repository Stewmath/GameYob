#include "inputhelper.h"
#include <nds/arm9/console.h>
#include <nds/arm9/input.h>
#include <nds/interrupts.h>

void versionInfoFunc(int value) {
    consoleClear();
    iprintf("GameYob %s\n", VERSION_STRING);
    while (true) {
        swiWaitForVBlank();
        readKeys();
        if (keyJustPressed(KEY_A) || keyJustPressed(KEY_B))
            break;
    }
}

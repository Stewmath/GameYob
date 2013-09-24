#include <nds/arm9/console.h>
#include <stdio.h>

void printVersionInfo() {
    consoleClear();
    iprintf("GameYob %s\n", VERSION_STRING);
}

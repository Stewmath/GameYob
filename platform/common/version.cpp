#ifdef DS
#include <nds/arm9/console.h>
#endif

#include <stdio.h>

void printVersionInfo() {
#ifdef DS
    consoleClear();
#endif
    printf("GameYob %s\n", VERSION_STRING);
}

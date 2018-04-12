#include <stdio.h>
#include "console.h"

void printVersionInfo() {
    clearConsole();
    printf("GameYob %s\n", VERSION_STRING);
}

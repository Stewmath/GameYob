#include <stdio.h>
#include "console.h"
int VERSION_STRING = 0;
void printVersionInfo() {
    clearConsole();
    printf("GameYob %s\n", VERSION_STRING);
}

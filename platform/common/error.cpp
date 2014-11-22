#include <stdio.h>
#include <stdarg.h>
#include "error.h"
#include "inputhelper.h"

void fatalerr(const char* format, ...) {
    va_list args;
    va_start(args, format);

    vprintf(format, args);
    va_end(args);

    printf("\n\nPlease restart GameYob.\n");

    while(1) {
        system_checkPolls();
        system_waitForVBlank();
    }
}

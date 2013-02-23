#include <nds.h>
#include <dirent.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
//#include <iostream>
//#include <string>
#include "gbgfx.h"
#include "gbcpu.h"
#include "inputhelper.h"
#include "timer.h"
#include "gbsnd.h"
#include "gameboy.h"
#include "mmu.h"
#include "main.h"
#include "console.h"

// defaultConsole is defined within libnds
extern PrintConsole defaultConsole;


void initializeGameboy() {
    initMMU();
	initCPU();
	initLCD();
	initGFX();
	initSND();
}

int main(int argc, char* argv[])
{
	videoSetModeSub(MODE_0_2D);
    vramSetBankH(VRAM_H_SUB_BG);
    PrintConsole console;
    consoleInit(NULL, defaultConsole.bgLayer, BgType_Text4bpp, BgSize_T_256x256, defaultConsole.mapBase, defaultConsole.gfxBase, false, true);
	consoleDebugInit(DebugDevice_CONSOLE);

    defaultExceptionHandler();

    initConsole();
	initInput();

    if (argc < 2)
        chdir("/lameboy");

	FILE* file;
	file = fopen("gbc_bios.bin", "rb");
    biosExists = file != NULL;
    if (biosExists) {
        fread(bios, 1, 0x900, file);
        biosEnabled = true;
    }
    else
        biosEnabled = false;

    if (argc >= 2) {
        loadProgram(argv[1]);
    }
    else {
        loadProgram(startFileChooser());
    }

    initializeGameboy();
	startTimer(0);

    runEmul();

    return 0;
}

void printLog(const char *format, ...) {
    va_list args;
    va_start(args, format);
    addToLog(format, args);
}

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
#include "nifi.h"

// defaultConsole is defined within libnds
extern PrintConsole defaultConsole;


void initializeGameboy() {
    initMMU();
    initCPU();
    initLCD();
    initGFX();
    initSND();

    if (suspendStateExists) {
        soundDisable();
        loadState(-1);
        if (!soundDisabled)
            soundEnable();
        // enter the console on resume
        advanceFrame = true;
    }
}

extern bool advanceFrame;
void fifoValue32Handler(u32 value, void* user_data) {
    static bool wasInConsole;
    static bool oldSoundDisabled;
    if (value == 1) {
        wasInConsole = isConsoleEnabled();
        oldSoundDisabled = soundDisabled;
        enterConsole();
        soundDisabled = true;
    }
    else {
        soundDisabled = false;
        if (!wasInConsole)
            exitConsole();
        soundDisabled = oldSoundDisabled;
    }
}

int main(int argc, char* argv[])
{
    videoSetModeSub(MODE_0_2D);
    vramSetBankH(VRAM_H_SUB_BG);
    REG_POWERCNT = POWER_ALL & ~(POWER_MATRIX | POWER_3D_CORE); // don't need 3D
    PrintConsole console;
    consoleInit(NULL, defaultConsole.bgLayer, BgType_Text4bpp, BgSize_T_256x256, defaultConsole.mapBase, defaultConsole.gfxBase, false, true);
    consoleDebugInit(DebugDevice_CONSOLE);

    defaultExceptionHandler();

    initConsole();
    initInput();
    readConfigFile();

    fifoSetValue32Handler(FIFO_USER_02, fifoValue32Handler, NULL);

    char *filename;
    bool filenameAllocated = false;
    if (argc >= 2) {
        filename = argv[1];
    }
    else {
        chdir("/lameboy");
        filename = startFileChooser();
        filenameAllocated = true;
    }

    FILE* file;
    file = fopen("gbc_bios.bin", "rb");
    biosExists = file != NULL;
    if (biosExists)
        fread(bios, 1, 0x900, file);
    else
        biosEnabled = false;

    loadProgram(filename);
    if (filenameAllocated)
        free(filename);

    initializeGameboy();
    startTimer();

    runEmul();

    return 0;
}

void printLog(const char *format, ...) {
    va_list args;
    va_start(args, format);
    addToLog(format, args);
}

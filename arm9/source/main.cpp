#include <nds.h>
#include <dirent.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
//#include <iostream>
//#include <string>
#include "gbgfx.h"
#include "gbcpu.h"
#include "inputhelper.h"
#include "filechooser.h"
#include "timer.h"
#include "gbsnd.h"
#include "gameboy.h"
#include "mmu.h"
#include "main.h"
#include "console.h"
#include "nifi.h"
#include "cheats.h"

extern bool __dsimode;
extern time_t rawTime;
extern time_t lastRawTime;

void initializeGameboy() {
    initMMU();
    initCPU();
    initLCD();
    initGFX();
    initSND();

    if (suspendStateExists) {
        loadState(-1);
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
    REG_POWERCNT = POWER_ALL & ~(POWER_MATRIX | POWER_3D_CORE); // don't need 3D
    consoleDemoInit();
    consoleDebugInit(DebugDevice_CONSOLE);

    defaultExceptionHandler();

    fifoSetValue32Handler(FIFO_USER_02, fifoValue32Handler, NULL);

    time(&rawTime);
    lastRawTime = rawTime;
    timerStart(0, ClockDivider_1024, TIMER_FREQ_1024(1), clockUpdater);

    /* Reset the EZ3in1 if present */
    if (!__dsimode) {
        sysSetCartOwner(BUS_OWNER_ARM9);

        GBA_BUS[0x0000] = 0xF0;
        GBA_BUS[0x1000] = 0xF0;
    }

    initConsole();
    initInput();
    readConfigFile();

    char *filename;
    bool filenameAllocated = false;
    if (argc >= 2) {
        filename = argv[1];
    }
    else {
        filename = startFileChooser();
        filenameAllocated = true;
    }

    if (!biosExists) {
        FILE* file;
        file = fopen("gbc_bios.bin", "rb");
        biosExists = file != NULL;
        if (biosExists)
            fread(bios, 1, 0x900, file);
        else
            biosEnabled = false;
    }

    loadProgram(filename);
    if (filenameAllocated)
        free(filename);


    initializeGameboy();

    updateConsoleScreen();

    runEmul();

    return 0;
}

void printLog(const char *format, ...) {
    va_list args;
    va_start(args, format);
    addToLog(format, args);
}

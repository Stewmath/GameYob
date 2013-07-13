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
#include "common.h"

extern bool __dsimode;
extern time_t rawTime;
extern time_t lastRawTime;
extern bool advanceFrame;

volatile SharedData* sharedData;

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
        time(&rawTime);
        lastRawTime = rawTime;
        soundDisabled = false;
        if (!wasInConsole)
            exitConsole();
        soundDisabled = oldSoundDisabled;
    }
}


void selectRom() {
    char* filename = startFileChooser();

    if (!biosExists) {
        FILE* file;
        file = fopen("gbc_bios.bin", "rb");
        biosExists = file != NULL;
        if (biosExists)
            fread(bios, 1, 0x900, file);
    }

    loadProgram(filename);
    free(filename);

    initializeGameboyFirstTime();
}

void initGBMode() {
    if (sgbModeOption != 0 && rom[0][0x14b] == 0x33 && rom[0][0x146] == 0x03)
        resultantGBMode = 2;
    else {
        resultantGBMode = 0;
    }
}
void initGBCMode() {
    if (sgbModeOption == 2 && rom[0][0x14b] == 0x33 && rom[0][0x146] == 0x03)
        resultantGBMode = 2;
    else {
        resultantGBMode = 1;
    }
}
void initializeGameboy() {
    switch(gbcModeOption) {
        case 0: // GB
            initGBMode();
            break;
        case 1: // GBC if needed
            if (rom[0][0x143] == 0xC0)
                initGBCMode();
            else
                initGBMode();
            break;
        case 2: // GBC
            if (rom[0][0x143] == 0x80 || rom[0][0x143] == 0xC0)
                initGBCMode();
            else
                initGBMode();
            break;
    }

    bool sgbEnhanced = rom[0][0x14b] == 0x33 && rom[0][0x146] == 0x03;
    if (sgbEnhanced && resultantGBMode != 2 && probingForBorder) {
        resultantGBMode = 2;
        nukeBorder = false;
    }
    else {
        probingForBorder = false;
    }

    initMMU();
    initCPU();
    initLCD();
    initGFX();
    initSND();

    if (!probingForBorder && suspendStateExists) {
        loadState(-1);
        // enter the console on resume
        advanceFrame = true;
    }
    updateScreens();
}

void initializeGameboyFirstTime() {
    if (sgbBordersEnabled)
        probingForBorder = true; // This will be ignored if starting in sgb mode, or if there is no sgb mode.
    nukeBorder = true;
    initializeGameboy();
}

int main(int argc, char* argv[])
{
    REG_POWERCNT = POWER_ALL & ~(POWER_MATRIX | POWER_3D_CORE); // don't need 3D
    consoleDebugInit(DebugDevice_CONSOLE);

    defaultExceptionHandler();

    time(&rawTime);
    lastRawTime = rawTime;
    timerStart(0, ClockDivider_1024, TIMER_FREQ_1024(1), clockUpdater);

    /* Reset the EZ3in1 if present */
    if (!__dsimode) {
        sysSetCartOwner(BUS_OWNER_ARM9);

        GBA_BUS[0x0000] = 0xF0;
        GBA_BUS[0x1000] = 0xF0;
    }

    fifoSetValue32Handler(FIFO_USER_02, fifoValue32Handler, NULL);

    sharedData = (SharedData*)memUncached(malloc(sizeof(SharedData)));
    sharedData->scalingOn = false;
    fifoSendValue32(FIFO_USER_03, ((u32)sharedData)&0x00ffffff);

    consoleOn = true;
    initConsole();
    initInput();
    readConfigFile();

    if (argc >= 2) {
        char* filename = argv[1];
        loadProgram(filename);
        initializeGameboyFirstTime();
    }
    else {
        selectRom();
    }
    consoleOn = false;
    updateScreens();

    runEmul();

    return 0;
}

void printLog(const char *format, ...) {
    va_list args;
    va_start(args, format);
    addToLog(format, args);
}

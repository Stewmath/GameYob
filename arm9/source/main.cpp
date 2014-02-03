#include <nds.h>
#include <dirent.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
//#include <iostream>
//#include <string>
#include "gbgfx.h"
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
#include "gbs.h"
#include "common.h"

time_t rawTime;
time_t lastRawTime;

volatile SharedData* sharedData;

Gameboy* gameboy;

// This is used to signal sleep mode starting or ending.
void fifoValue32Handler(u32 value, void* user_data) {
    static u8 scalingWasOn;
    static bool soundWasDisabled;
    static bool wasPaused;
    switch(value) {
        case FIFOMSG_LID_CLOSED:
            // Entering sleep mode
            scalingWasOn = sharedData->scalingOn;
            soundWasDisabled = soundDisabled;
            wasPaused = gameboy->isGameboyPaused();

            sharedData->scalingOn = 0;
            soundDisabled = true;
            gameboy->pause();
            break;
        case FIFOMSG_LID_OPENED:
            // Exiting sleep mode
            sharedData->scalingOn = scalingWasOn;
            soundDisabled = soundWasDisabled;
            if (!wasPaused)
                gameboy->unpause();
            // Time isn't incremented properly in sleep mode, compensate here.
            time(&rawTime);
            lastRawTime = rawTime;
            break;
    }
}


void selectRom() {
    gameboy->unloadRom();

    loadFileChooserState(&romChooserState);
    const char* extraExtensions[] = {"gbs"};
    char* filename = startFileChooser(extraExtensions, true);
    saveFileChooserState(&romChooserState);

    if (!biosExists) {
        FILE* file;
        file = fopen("gbc_bios.bin", "rb");
        biosExists = file != NULL;
        if (biosExists) {
            fread(gameboy->bios, 1, 0x900, file);
            fclose(file);
        }
    }

    gameboy->loadRom(filename);
    free(filename);

    updateScreens();

    initializeGameboyFirstTime();
}

void initGBMode() {
    if (sgbModeOption != 0 && gameboy->romSlot0[0x14b] == 0x33 && gameboy->romSlot0[0x146] == 0x03)
        gameboy->resultantGBMode = 2;
    else {
        gameboy->resultantGBMode = 0;
    }
}
void initGBCMode() {
    if (sgbModeOption == 2 && gameboy->romSlot0[0x14b] == 0x33 && gameboy->romSlot0[0x146] == 0x03)
        gameboy->resultantGBMode = 2;
    else {
        gameboy->resultantGBMode = 1;
    }
}
void initializeGameboy() {
    enableSleepMode();

    if (gbsMode) {
        gameboy->resultantGBMode = 1; // GBC
        probingForBorder = false;
    }
    else {
        switch(gbcModeOption) {
            case 0: // GB
                initGBMode();
                break;
            case 1: // GBC if needed
                if (gameboy->romSlot0[0x143] == 0xC0)
                    initGBCMode();
                else
                    initGBMode();
                break;
            case 2: // GBC
                if (gameboy->romSlot0[0x143] == 0x80 || gameboy->romSlot0[0x143] == 0xC0)
                    initGBCMode();
                else
                    initGBMode();
                break;
        }

        bool sgbEnhanced = gameboy->romSlot0[0x14b] == 0x33 && gameboy->romSlot0[0x146] == 0x03;
        if (sgbEnhanced && gameboy->resultantGBMode != 2 && probingForBorder) {
            gameboy->resultantGBMode = 2;
        }
        else {
            probingForBorder = false;
        }
    } // !gbsMode

    gameboy->init();

    if (!gbsMode && !probingForBorder && gameboy->checkStateExists(-1)) {
        gameboy->loadState(-1);
    }

    if (gbsMode)
        gbsInit();

    // We haven't calculated the # of cycles to the next hardware event.
    gameboy->cyclesToEvent = 1;
}

void initializeGameboyFirstTime() {
    if (sgbBordersEnabled)
        probingForBorder = true; // This will be ignored if starting in sgb mode, or if there is no sgb mode.
    sgbBorderLoaded = false; // Effectively unloads any sgb border

    initializeGameboy();

    if (gbsMode) {
        disableMenuOption("State Slot");
        disableMenuOption("Save State");
        disableMenuOption("Load State");
        disableMenuOption("Delete State");
        disableMenuOption("Suspend");
        disableMenuOption("Exit without saving");
    }
    else {
        enableMenuOption("State Slot");
        enableMenuOption("Save State");
        enableMenuOption("Suspend");
        if (gameboy->checkStateExists(stateNum)) {
            enableMenuOption("Load State");
            enableMenuOption("Delete State");
        }
        else {
            disableMenuOption("Load State");
            disableMenuOption("Delete State");
        }

        if (gameboy->getNumRamBanks() && !autoSavingEnabled)
            enableMenuOption("Exit without saving");
        else
            disableMenuOption("Exit without saving");
    }

    if (biosExists)
        enableMenuOption("GBC Bios");
    else
        disableMenuOption("GBC Bios");
}

int main(int argc, char* argv[])
{
    srand(time(NULL));

    REG_POWERCNT = POWER_ALL & ~(POWER_MATRIX | POWER_3D_CORE); // don't need 3D
    consoleDebugInit(DebugDevice_CONSOLE);
    lcdMainOnTop();

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
    sharedData->enableSleepMode = true;
    // It might make more sense to use "fifoSendAddress" here.
    // However there may have been something wrong with it in dsi mode.
    fifoSendValue32(FIFO_USER_03, ((u32)sharedData)&0x00ffffff);

    gameboy = new Gameboy();

    initInput();
    setMenuDefaults();
    readConfigFile();
    swiWaitForVBlank();
    swiWaitForVBlank();
    // initGFX is called in initializeGameboy, but I also call it from here to
    // set up the vblank handler asap.
    initGFX();

    consoleInitialized = false;

    if (argc >= 2) {
        char* filename = argv[1];
        gameboy->loadRom(filename);
        updateScreens();
        initializeGameboyFirstTime();
    }
    else {
        selectRom();
    }

    gameboy->runEmul();

    return 0;
}

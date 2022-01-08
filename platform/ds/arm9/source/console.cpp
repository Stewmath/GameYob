#include <nds.h>
#include "gbprinter.h"
#include "console.h"
#include "menu.h"
#include "inputhelper.h"
#include "filechooser.h"
#include "soundengine.h"
#include "main.h"
#include "gameboy.h"
#include "nifi.h"
#include "cheats.h"
#include "gbgfx.h"
#include "gbs.h"
#include "common.h"
#include "3in1.h"


const int screenTileWidth = 32;
const int backlights[] = {PM_BACKLIGHT_TOP, PM_BACKLIGHT_BOTTOM};

volatile int consoleSelectedRow = -1;



bool isConsoleOn() {
    return consoleInitialized;
}


void clearConsole() {
    consoleClear();
}

PrintConsole* getDefaultConsole() {
    return consoleGetDefault();
}

int consoleGetWidth() {
    return 32;
}
int consoleGetHeight() {
    return 24;
}

void consoleFlush() {
    fflush(stdout);
}

void printLog(const char *format, ...) {
    /*
    if (isMenuOn() || sharedData->scalingOn)
        return;
        */
    va_list args;
    va_start(args, format);

    if (consoleDebugOutput)
        viprintf(format, args);

    va_end(args);
}


// updateScreens helper functions

void enableConsoleBacklight2() {
    powerOn(backlights[!gameScreen]);
}
void enableConsoleBacklight() {
    // For some reason, waiting 2 frames helps eliminate a white flash.
    doAtVBlank(enableConsoleBacklight2);
}

// 2 frames delay
void setupScaledScreens2() {
    REG_DISPCNT &= ~(3<<16); // Disable main display
    REG_DISPCNT_SUB |= 1<<16; // Enable sub display
    if (gameScreen == 0)
        lcdMainOnBottom();
    else
        lcdMainOnTop();
    powerOn(backlights[gameScreen]);

    refreshScaleMode();
}

// 1 frame delay
void setupScaledScreens1() {
    powerOff(backlights[!gameScreen]);

    if (!singleScreenMode) {
        // Disable sub screen for 1 frame
        REG_DISPCNT_SUB &= ~(3<<16);
    }

    // By next vblank, the scaled image will be ready.
    doAtVBlank(setupScaledScreens2);
}

void setupUnscaledScreens() {
    if (!consoleInitialized) {
        consoleDemoInit(); // Or, consoleInit(menuConsole, ...)
        setPrintConsole(menuConsole);
        BG_PALETTE_SUB[8*16 - 1] = RGB15(17,17,17); // Grey (replaces a color established in consoleDemoInit)
        consoleInitialized = true;
    }

    if (gameScreen == 0) {
        if (singleScreenMode && (isMenuOn() || isFileChooserOn()))
            lcdMainOnBottom();
        else
            lcdMainOnTop();
    }
    else {
        if (singleScreenMode && (isMenuOn() || isFileChooserOn()))
            lcdMainOnTop();
        else
            lcdMainOnBottom();
    }

    if (singleScreenMode) {
        powerOn(backlights[gameScreen]);
        powerOff(backlights[!gameScreen]);
        if (isMenuOn() || isFileChooserOn()) {
            REG_DISPCNT &= ~(3<<16);
            REG_DISPCNT_SUB |= 1<<16;
        }
        else {
            REG_DISPCNT_SUB &= ~(3<<16);
            REG_DISPCNT |= 1<<16;
        }
    }
    else {
        REG_DISPCNT &= ~(3<<16);
        REG_DISPCNT |= 1<<16; // Enable main display

        int screensToSet[2];

        screensToSet[gameScreen] = true;

        if (!(fpsOutput || timeOutput || consoleDebugOutput || isMenuOn() || isFileChooserOn())) {
            screensToSet[!gameScreen] = false;
            REG_DISPCNT_SUB &= ~(3<<16); // Disable sub display
        }
        else {
            screensToSet[!gameScreen] = true;
            REG_DISPCNT_SUB &= ~(3<<16);
            REG_DISPCNT_SUB |= 1<<16; // Enable sub display
        }

        for (int i=0; i<2; i++) {
            if (screensToSet[i]) {
                if (i == !gameScreen)
                    doAtVBlank(enableConsoleBacklight);
                else
                    powerOn(backlights[i]);
            }
            else
                powerOff(backlights[i]);
        }
    }
}

void updateScreens(bool waitToFinish) {
    if (!gbsMode && !isMenuOn() && !isFileChooserOn() && scaleMode != 0) {
        // Manage screens with scaling enabled:
        sharedData->scalingOn = 1;

        // The VRAM used for the console will be overwritten, so...
        consoleInitialized = false;


        doAtVBlank(setupScaledScreens1);

        if (waitToFinish) {
            swiWaitForVBlank();
            swiWaitForVBlank();
        }
    }
    else {
        // Manage screens normally
        sharedData->scalingOn = 0;

        doAtVBlank(setupUnscaledScreens);
        if (waitToFinish)
            swiWaitForVBlank();
    }
}


// Console helper functions

PrintConsole* printConsole;

void setPrintConsole(PrintConsole* console) {
    printConsole = console;
    consoleSelect(console);
}

PrintConsole* getPrintConsole() {
    return printConsole;
}

void consoleSetPosColor(int x, int y, int color) {
    u16* map = BG_MAP_RAM_SUB(22);
    map[y*32+x] &= ~TILE_PALETTE(15);
    map[y*32+x] |= TILE_PALETTE(color);
}

void consoleSetLineColor(int line, int color) {
    u16* map = BG_MAP_RAM_SUB(22);
    for (int i=0; i<32; i++) {
        map[line*32+i] &= ~TILE_PALETTE(15);
        map[line*32+i] |= TILE_PALETTE(color);
    }
}

void iprintfColored(int palette, const char *format, ...) {
    va_list args;
    va_start(args, format);

    PrintConsole* console = getPrintConsole();
    int x = console->cursorX;
    int y = console->cursorY;

    char s[100];
    vsiprintf(s, format, args);
    va_end(args);

    u16* dest = BG_MAP_RAM_SUB(22)+y*32+x;
    for (uint i=0; i<strlen(s); i++) {
        if (s[i] == '\n') {
            x = 0;
            y++;
        }
        else {
            *(dest++) = s[i] | TILE_PALETTE(palette);
            x++;
            if (x == 32) {
                x = 0;
                y++;
            }
        }
    }
    console->cursorX = x;
    console->cursorY = y;
    //iprintf(s);
}


int checkRumble() {
    if (isDSiMode())
        return 0;

    sysSetCartOwner(BUS_OWNER_ARM9);

    OpenNorWrite();
    uint32 rumbleID = ReadNorFlashID();
    CloseNorWrite();


    if (isRumbleInserted())
        return 1; //Warioware / Official rumble found.
    else if (rumbleID != 0)
        return 2; //3in1 found
    else
        return 0; //No rumble found
}

// Misc stuff

void disableSleepMode() {
    if (sharedData->enableSleepMode) {
        sharedData->enableSleepMode = false;
        fifoSendValue32(FIFO_PM, PM_REQ_SLEEP_DISABLE);
    }
}

void enableSleepMode() {
    if (!sharedData->enableSleepMode) {
        sharedData->enableSleepMode = true;
        fifoSendValue32(FIFO_PM, PM_REQ_SLEEP_ENABLE);
    }
}

#include <stdlib.h>
#include "gbmanager.h"
#include "inputhelper.h"
#include "gameboy.h"
#include "nifi.h"
#include "gbs.h"
#include "menu.h"
#include "romfile.h"
#include "filechooser.h"
#include "soundengine.h"
#include "error.h"
#include "timer.h"

Gameboy* gameboy = NULL;
Gameboy* gb2 = NULL;

// Ordering for purposes of link emulation
Gameboy* gbUno = NULL;
Gameboy* gbDuo = NULL;

Gameboy* hostGb = NULL;

RomFile* romFile = NULL;

int mgr_frameCounter;

int autoFireCounterA=0, autoFireCounterB=0;


int fps = 0;

time_t rawTime;
time_t lastRawTime;

bool emulationPaused;

void mgr_init() {
    if (gameboy != NULL)
        delete gameboy;
    if (gb2 != NULL)
        delete gb2;
    if (romFile != NULL)
        delete romFile;

    gameboy = new Gameboy();
    hostGb = gameboy;
    gbUno = gameboy;
    gbDuo = NULL;

    mgr_frameCounter = 0;

    rawTime = 0;
    lastRawTime = rawTime;

    emulationPaused = false;

#ifdef CPU_DEBUG
    startDebugger();
#endif
}

void mgr_reset() {
    if (gameboy)
        gameboy->init();
    if (gb2)
        gb2->init();
    mgr_frameCounter = 0;
}

void mgr_runFrame() {
    if (!gbUno || emulationPaused)
        return;

    int ret1=0;

    if (gbDuo) {
//         printLog("Begin\n");
        while (!((ret1 & RET_VBLANK))) {
            bool swap = false;
            if ((gbUno->ioRam[0x02]&0x81) == 0x80)
                swap = true;
//             else if ((gbUno->ioRam[0x02]&0x81) == 0x80 && ((gbDuo->ioRam[0x02] & 0x81) == 0x80)) {
//                 if (gbDuo == hostGb)
//                     swap = true;
//             }

            if (swap) {
                Gameboy* tmp = gbUno;
                gbUno = gbDuo;
                gbDuo = tmp;
            }

            if (gbUno->cycleCount <= gbDuo->cycleCount) {
                if (gbUno == hostGb)
                    ret1 |= gbUno->runEmul();
                else
                    gbUno->runEmul();
            }
            else {
                if (gbDuo == hostGb)
                    ret1 |= gbDuo->runEmul();
                else
                    gbDuo->runEmul();
            }
//             printLog("%d, %d\n", gbUno->cycleCount, gbDuo->cycleCount);
        }
//         printLog("End\n");

        int subtractor;
        if (gbDuo->cycleCount <= gbUno->cycleCount)
            subtractor = gbDuo->cycleCount;
        else
            subtractor = gbUno->cycleCount;
        gbUno->cycleCount -= subtractor;
        gbDuo->cycleCount -= subtractor;
        if (gbUno->cycleToSerialTransfer != -1)
            gbUno->cycleToSerialTransfer -= subtractor;
        if (gbDuo->cycleToSerialTransfer != -1)
            gbDuo->cycleToSerialTransfer -= subtractor;
    }
    else {
        while (!(ret1 & RET_VBLANK)) {
            ret1 |= gbUno->runEmul();
        }

        gbUno->cycleCount = 0;
    }

    if (mgr_areBothUsingExternalClock())
        printLog("Both waiting\n");

    mgr_frameCounter++;
}

void mgr_startGb2(const char* filename) {
    if (gb2 == NULL)
        gb2 = new Gameboy();
    gb2->setRomFile(gameboy->getRomFile());
    if (filename == 0)
        gb2->loadSave(-1);
    else
        gb2->loadSave(2);
    gb2->init();
    gb2->cycleCount = gameboy->cycleCount;
    gb2->getSoundEngine()->mute();

    gameboy->linkedGameboy = gb2;
    gb2->linkedGameboy = gameboy;

    gbDuo = gb2;
}

void mgr_swapFocus() {
    if (gb2) {
        Gameboy* tmp = gameboy;
        gameboy = gb2;
        gb2 = tmp;

        gb2->getSoundEngine()->mute();
        gameboy->getSoundEngine()->refresh();

        refreshGFX();
    }
}

void mgr_setInternalClockGb(Gameboy* g) {
    gbUno = g;
    hostGb = g;
    if (g == gameboy)
        gbDuo = gb2;
    else
        gbDuo = gameboy;
}

bool mgr_isInternalClockGb(Gameboy* g) {
    return gbDuo && gbUno == g;
}
bool mgr_isExternalClockGb(Gameboy* g) {
    return gbUno && gbDuo == g;
}
bool mgr_areBothUsingExternalClock() {
    return (gbUno->ioRam[0x02] & 0x81) == 0x80 && gbDuo &&
        (gbDuo->ioRam[0x02] & 0x81) == 0x80;
}

void mgr_pause() {
    emulationPaused = true;
}
void mgr_unpause() {
    emulationPaused = false;
}
bool mgr_isPaused() {
    return emulationPaused;
}

void mgr_loadRom(const char* filename) {
    if (romFile != NULL)
        delete romFile;

    romFile = new RomFile(filename);
    if (romFile == 0)
        fatalerr("Not enough RAM to load rom");
    gameboy->setRomFile(romFile);
    gameboy->loadSave(1);

    hostGb = gameboy;

    // Border probing is broken
#if 0
    if (sgbBordersEnabled)
        probingForBorder = true; // This will be ignored if starting in sgb mode, or if there is no sgb mode.
#endif

    sgbBorderLoaded = false; // Effectively unloads any sgb border

    gameboy->init();

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

        if (gameboy->getNumSramBanks() && !autoSavingEnabled)
            enableMenuOption("Exit without saving");
        else
            disableMenuOption("Exit without saving");
    }

    if (biosExists)
        enableMenuOption("GBC Bios");
    else
        disableMenuOption("GBC Bios");
}

void mgr_unloadRom() {
#ifdef NIFI
    nifiStop();
#endif

    gameboy->unloadRom();
    gameboy->linkedGameboy = NULL;
    gbUno = gameboy;

    if (gb2) {
//         gb2->unloadRom();
        delete gb2;
        gb2 = NULL;
        gbDuo = NULL;
    }

    if (romFile != NULL) {
        delete romFile;
        romFile = NULL;
    }
}

void mgr_selectRom() {
    mgr_unloadRom();

    loadFileChooserState(&romChooserState);
    const char* extraExtensions[] = {"gbs"};
    char* filename = startFileChooser(extraExtensions, true);
    saveFileChooserState(&romChooserState);

    if (filename == NULL) {
        fatalerr("Filechooser error");
    }
    mgr_loadRom(filename);

    if (!biosExists) {
        gameboy->getRomFile()->loadBios("gbc_bios.bin");
    }

    free(filename);

    updateScreens();
}

void mgr_save() {
    if (gameboy)
        gameboy->saveGame();
    if (gb2)
        gb2->saveGame();
}

void mgr_updateVBlank() {
    drawScreen();

    system_checkPolls();

    inputUpdateVBlank();

    buttonsPressed = 0xff;
    if (isMenuOn())
        updateMenu();
    else {
        // Check some buttons
        buttonsPressed = 0xff;

        if (probingForBorder)
            return;

        if (keyPressed(mapFuncKey(FUNC_KEY_UP))) {
            buttonsPressed &= (0xFF ^ GB_UP);
        }
        if (keyPressed(mapFuncKey(FUNC_KEY_DOWN))) {
            buttonsPressed &= (0xFF ^ GB_DOWN);
        }
        if (keyPressed(mapFuncKey(FUNC_KEY_LEFT))) {
            buttonsPressed &= (0xFF ^ GB_LEFT);
        }
        if (keyPressed(mapFuncKey(FUNC_KEY_RIGHT))) {
            buttonsPressed &= (0xFF ^ GB_RIGHT);
        }
        if (keyPressed(mapFuncKey(FUNC_KEY_A))) {
            buttonsPressed &= (0xFF ^ GB_A);
        }
        if (keyPressed(mapFuncKey(FUNC_KEY_B))) {
            buttonsPressed &= (0xFF ^ GB_B);
        }
        if (keyPressed(mapFuncKey(FUNC_KEY_START))) {
            buttonsPressed &= (0xFF ^ GB_START);
        }
        if (keyPressed(mapFuncKey(FUNC_KEY_SELECT))) {
            buttonsPressed &= (0xFF ^ GB_SELECT);
        }

        if (keyPressed(mapFuncKey(FUNC_KEY_AUTO_A))) {
            if (autoFireCounterA <= 0) {
                buttonsPressed &= (0xFF ^ GB_A);
                autoFireCounterA = 2;
            }
            autoFireCounterA--;
        }
        if (keyPressed(mapFuncKey(FUNC_KEY_AUTO_B))) {
            if (autoFireCounterB <= 0) {
                buttonsPressed &= (0xFF ^ GB_B);
                autoFireCounterB = 2;
            }
            autoFireCounterB--;
        }

#ifndef NIFI
        gameboy->controllers[0] = buttonsPressed;
#endif

        if (keyJustPressed(mapFuncKey(FUNC_KEY_SAVE))) {
            if (!autoSavingEnabled) {
                gameboy->saveGame();
            }
        }

        fastForwardKey = keyPressed(mapFuncKey(FUNC_KEY_FAST_FORWARD));
        if (keyJustPressed(mapFuncKey(FUNC_KEY_FAST_FORWARD_TOGGLE)))
            fastForwardMode = !fastForwardMode;

        if (keyJustPressed(mapFuncKey(FUNC_KEY_MENU) | mapFuncKey(FUNC_KEY_MENU_PAUSE)
#if defined(DS) || defined(_3DS)
                    | KEY_TOUCH
#endif
                    )) {
            if (singleScreenMode || keyJustPressed(mapFuncKey(FUNC_KEY_MENU_PAUSE)))
                mgr_pause();

            forceReleaseKey(0xffffffff);
            fastForwardKey = false;
            fastForwardMode = false;
            displayMenu();
        }

        if (keyJustPressed(mapFuncKey(FUNC_KEY_SCALE))) {
            setMenuOption("Scaling", !getMenuOption("Scaling"));
        }

#ifdef DS
        if (fastForwardKey || fastForwardMode) {
            sharedData->hyperSound = false;
        }
        else {
            sharedData->hyperSound = hyperSound;
        }
#endif
        if (keyJustPressed(mapFuncKey(FUNC_KEY_RESET)))
            gameboy->resetGameboy();

        if (gb2 && keyJustPressed(mapFuncKey(FUNC_KEY_SWAPFOCUS)))
            mgr_swapFocus();

        gameboy->checkInput();
        if (gb2)
            gb2->checkInput();

        if (gbsMode)
            gbsCheckInput();
    } // !isMenuOn()

    if (gameboy) {
#ifdef NIFI
        nifiUpdateInput();
#endif
    }

#ifndef DS
    rawTime = getTime();
#endif

#ifndef SDL
    fps++;

    if (isConsoleOn() && !isMenuOn() && !consoleDebugOutput && (rawTime > lastRawTime))
    {
        setPrintConsole(menuConsole);
        int line=0;
        if (fpsOutput) {
            clearConsole();
            iprintf("FPS: %d\n", fps);
            line++;
        }
        fps = 0;
#ifdef DS
        if (timeOutput) {
            for (; line<23-1; line++)
                iprintf("\n");
            char *timeString = ctime(&rawTime);
            for (int i=0;; i++) {
                if (timeString[i] == ':') {
                    timeString += i-2;
                    break;
                }
            }
            char s[50];
            strncpy(s, timeString, 50);
            s[5] = '\0';
            int spaces = 31-strlen(s);
            for (int i=0; i<spaces; i++)
                iprintf(" ");

            iprintf("%s\n", s);
        }
#endif
        lastRawTime = rawTime;
    }
#endif
}

void mgr_exit() {
    if (gameboy)
        delete gameboy;
    if (gb2)
        delete gb2;
    if (romFile)
        delete romFile;

    gameboy = 0;
    gb2 = 0;
    romFile = 0;
}

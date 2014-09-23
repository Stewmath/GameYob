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

Gameboy* gameboy;
RomFile* romFile = NULL;

#ifdef DS
time_t rawTime;
time_t lastRawTime;
#endif


void updateVBlank();


void mgr_init() {
    gameboy = new Gameboy();
}

void mgr_run() {
	for (;;)
	{
        if (!gameboy->isGameboyPaused())
            gameboy->runEmul();
        updateVBlank();
	}
}

void mgr_loadRom(const char* filename) {
    if (romFile != NULL)
        delete romFile;

    romFile = new RomFile(filename);
    gameboy->setRomFile(romFile);
    gameboy->loadSave(1);


    if (sgbBordersEnabled)
        probingForBorder = true; // This will be ignored if starting in sgb mode, or if there is no sgb mode.
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

void mgr_unloadRom() {
    gameboy->unloadRom();

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

    mgr_loadRom(filename);

    if (!biosExists) {
        gameboy->getRomFile()->loadBios("gbc_bios.bin");
    }

    free(filename);

    updateScreens();
}


void updateVBlank() {
    inputUpdateVBlank();

    if (isMenuOn())
        updateMenu();
    else {
        gameboy->gameboyCheckInput();
        if (gbsMode)
            gbsCheckInput();
    }

    nifiCheckInput();

#ifdef DS
    int oldIME = REG_IME;
    REG_IME = 0;
    nifiUpdateInput();
    REG_IME = oldIME;
#endif

    drawScreen();

#ifdef DS
    if (isConsoleOn() && !isMenuOn() && !consoleDebugOutput && (rawTime > lastRawTime))
    {
        setPrintConsole(menuConsole);
        consoleClear();
        int line=0;
        if (fpsOutput) {
            consoleClear();
            iprintf("FPS: %d\n", gameboy->fps);
            line++;
        }
        gameboy->fps = 0;
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
        lastRawTime = rawTime;
    }
#endif
}

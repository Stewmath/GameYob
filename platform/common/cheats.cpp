#include <string.h>
#include <algorithm>
#include "gameboy.h"
#include "mmu.h"
#include "console.h"
#include "menu.h"
#include "main.h"
#include "cheats.h"
#include "inputhelper.h"
#include "gbgfx.h"
#include "romfile.h"
#include "io.h"

#define TO_INT(a) ( (a) >= 'a' ? (a) - 'a' + 10 : (a) >= 'A' ? (a) - 'A' + 10 : (a) - '0')

CheatEngine::CheatEngine(Gameboy* g) {
    gameboy = g;
    romFile = gameboy->getRomFile();
    cheatsEnabled = true;
    numCheats = 0;
    cheatsRomTitle[0] = '\0';
}

void CheatEngine::setRomFile(RomFile* r) {
    romFile = r;
}

void CheatEngine::enableCheats (bool enable)
{
    cheatsEnabled = enable;
}

bool CheatEngine::addCheat (const char *str)
{
    int len;
    int i = numCheats;

    if (i == MAX_CHEATS)
        return false;

    // Clear all flags
    cheats[i].flags = 0;
    cheats[i].patchedBanks = std::vector<int>();
    cheats[i].patchedValues = std::vector<int>();

    len = strlen(str);
    strncpy(cheats[i].cheatString, str, 12);

    // GameGenie AAA-BBB-CCC
    if (len == 11) {
        cheats[i].flags |= CHEAT_FLAG_GAMEGENIE;
        
        cheats[i].data = TO_INT(str[0]) << 4 | TO_INT(str[1]);
        cheats[i].address = TO_INT(str[6]) << 12 | TO_INT(str[2]) << 8 | 
            TO_INT(str[4]) << 4 | TO_INT(str[5]);
        cheats[i].compare = TO_INT(str[8]) << 4 | TO_INT(str[10]);

        cheats[i].address ^= 0xf000;
        cheats[i].compare = (cheats[i].compare >> 2) | (cheats[i].compare&0x3) << 6;
        cheats[i].compare ^= 0xba;

        printLog("GG %04x / %02x -> %02x\n", cheats[i].address, cheats[i].data, cheats[i].compare);
    }
    // GameGenie (6digit version) AAA-BBB
    else if (len == 7) {
        cheats[i].flags |= CHEAT_FLAG_GAMEGENIE1;

        cheats[i].data = TO_INT(str[0]) << 4 | TO_INT(str[1]);
        cheats[i].address = TO_INT(str[6]) << 12 | TO_INT(str[2]) << 8 | 
            TO_INT(str[4]) << 4 | TO_INT(str[5]);

        printLog("GG1 %04x / %02x\n", cheats[i].address, cheats[i].data);
    }
    // Gameshark AAAAAAAA
    else if (len == 8) {
        cheats[i].flags |= CHEAT_FLAG_GAMESHARK;
        
        cheats[i].data = TO_INT(str[2]) << 4 | TO_INT(str[3]);
        cheats[i].bank = TO_INT(str[0]) << 4 | TO_INT(str[1]);
        cheats[i].address = TO_INT(str[6]) << 12 | TO_INT(str[7]) << 8 | 
            TO_INT(str[4]) << 4 | TO_INT(str[5]);

        printLog("GS (%02x)%04x/ %02x\n", cheats[i].bank, cheats[i].address, cheats[i].data);
    }
    else { // dafuq did i just read ?
        return false;
    }

    numCheats++;
    return true;
}

void CheatEngine::toggleCheat (int i, bool enabled) 
{
    if (enabled) {
        cheats[i].flags |= CHEAT_FLAG_ENABLED;
        if ((cheats[i].flags & CHEAT_FLAG_TYPE_MASK) != CHEAT_FLAG_GAMESHARK) {
            for (int j=0; j<romFile->getNumRomBanks(); j++) {
                if (romFile->isRomBankLoaded(j))
                    applyGGCheatsToBank(j);
            }
        }
    }
    else {
        unapplyGGCheat(i);
        cheats[i].flags &= ~CHEAT_FLAG_ENABLED;
    }
}

void CheatEngine::unapplyGGCheat(int cheat) {
    if ((cheats[cheat].flags & CHEAT_FLAG_TYPE_MASK) != CHEAT_FLAG_GAMESHARK) {
        for (unsigned int i=0; i<cheats[cheat].patchedBanks.size(); i++) {
            int bank = cheats[cheat].patchedBanks[i];
            if (romFile->isRomBankLoaded(bank)) {
                romFile->getRomBank(bank)[cheats[cheat].address&0x3fff] = cheats[cheat].patchedValues[i];
            }
        }
        cheats[cheat].patchedBanks = std::vector<int>();
        cheats[cheat].patchedValues = std::vector<int>();
    }
}

void CheatEngine::applyGGCheatsToBank(int bank) {
	u8* bankPtr = romFile->getRomBank(bank);
    for (int i=0; i<numCheats; i++) {
        if (cheats[i].flags & CHEAT_FLAG_ENABLED && ((cheats[i].flags & CHEAT_FLAG_TYPE_MASK) != CHEAT_FLAG_GAMESHARK)) {

            int bankSlot = cheats[i].address/0x4000;
            if ((bankSlot == 0 && bank == 0) || (bankSlot == 1 && bank != 0)) {
                int address = cheats[i].address&0x3fff;
                if (((cheats[i].flags & CHEAT_FLAG_TYPE_MASK) == CHEAT_FLAG_GAMEGENIE1 || bankPtr[address] == cheats[i].compare) && 
                        find(cheats[i].patchedBanks.begin(), cheats[i].patchedBanks.end(), bank) == cheats[i].patchedBanks.end()) {

                    cheats[i].patchedBanks.push_back(bank);
                    cheats[i].patchedValues.push_back(bankPtr[address]);
                    bankPtr[address] = cheats[i].data;
                }
            }
        }
    }
}

void CheatEngine::applyGSCheats() {
    int i;
    int compareBank;

    for (i = 0; i < numCheats; i++) {
        if (cheats[i].flags & CHEAT_FLAG_ENABLED && ((cheats[i].flags & CHEAT_FLAG_TYPE_MASK) == CHEAT_FLAG_GAMESHARK)) {
            switch (cheats[i].bank & 0xf0) {
                case 0x90:
                    compareBank = gameboy->getWramBank();
                    gameboy->setWramBank(cheats[i].bank & 0x7);
                    gameboy->writeMemory(cheats[i].address, cheats[i].data);
                    gameboy->setWramBank(compareBank);
                    break;
                case 0x80: /* TODO : Find info and stuff */
                    break;
                case 0x00:
                    gameboy->writeMemory(cheats[i].address, cheats[i].data);
                    break;
            }
        }
    }
}

void CheatEngine::loadCheats(const char* filename) {
    // TODO: get rid of the "cheatsRomTitle" stuff
    if (strcmp(cheatsRomTitle, romFile->getRomTitle()) == 0) {
        // Rom hasn't been changed
        for (int i=0; i<numCheats; i++)
            unapplyGGCheat(i);
    }
    else
        // Rom has been changed
        strncpy(cheatsRomTitle, romFile->getRomTitle(), 20);
    numCheats = 0;

    // Begin loading new cheat file
    FileHandle* file = file_open(filename, "r");
    if (file == NULL) {
        disableMenuOption("Manage Cheats");
        return;
    }

    while (file_tell(file) < file_getSize(file)) {
        int i = numCheats;

        char line[100];
        file_gets(line, 100, file);

        if (*line != '\0') {
            char* spacePos = strchr(line, ' ');
            if (spacePos != NULL) {
                *spacePos = '\0';
                if (strlen(spacePos+1) >= 1 && addCheat(line)) {
                    strncpy(cheats[i].name, spacePos+2, MAX_CHEAT_NAME_LEN);
                    cheats[i].name[MAX_CHEAT_NAME_LEN] = '\0';
                    char c;
                    while ((c = cheats[i].name[strlen(cheats[i].name)-1]) == '\n' || c == '\r')
                        cheats[i].name[strlen(cheats[i].name)-1] = '\0';
                    toggleCheat(i, *(spacePos+1) == '1');
                }
            }
        }
    }

    file_close(file);

    enableMenuOption("Manage Cheats");
}

void CheatEngine::saveCheats(const char* filename) {
    if (numCheats == 0)
        return;
    FileHandle* file = file_open(filename, "w");
    for (int i=0; i<numCheats; i++) {
        file_printf(file, "%s %d%s\n", cheats[i].cheatString, !!(cheats[i].flags & CHEAT_FLAG_ENABLED), cheats[i].name);
    }
    file_close(file);
}

// Menu code

const int cheatsPerPage=18;
int cheatMenuSelection=0;
bool cheatMenu_gameboyWasPaused;
CheatEngine* ch; // cheat engine to display the menu for

void redrawCheatMenu() {
    int numCheats = ch->getNumCheats();

    int numPages = (numCheats-1)/cheatsPerPage+1;

    int page = cheatMenuSelection/cheatsPerPage;

    clearConsole();

    printf("          Cheat Menu      ");
    printf("%d/%d\n\n", page+1, numPages);
    for (int i=page*cheatsPerPage; i<numCheats && i < (page+1)*cheatsPerPage; i++) {
        int nameColor = (cheatMenuSelection == i ? CONSOLE_COLOR_LIGHT_YELLOW : CONSOLE_COLOR_WHITE);
        iprintfColored(nameColor, ch->cheats[i].name);
        for (unsigned int j=0; j<25-strlen(ch->cheats[i].name); j++)
            printf(" ");
        if (ch->isCheatEnabled(i)) {
            if (cheatMenuSelection == i) {
                iprintfColored(CONSOLE_COLOR_LIGHT_YELLOW, "* ");
                iprintfColored(CONSOLE_COLOR_LIGHT_GREEN, "On");
                iprintfColored(CONSOLE_COLOR_LIGHT_YELLOW, " * ");
            }
            else
                iprintfColored(CONSOLE_COLOR_WHITE, "  On   ");
        }
        else {
            if (cheatMenuSelection == i) {
                iprintfColored(CONSOLE_COLOR_LIGHT_YELLOW, "* ");
                iprintfColored(CONSOLE_COLOR_LIGHT_GREEN, "Off");
                iprintfColored(CONSOLE_COLOR_LIGHT_YELLOW, " *");
            }
            else
                iprintfColored(CONSOLE_COLOR_WHITE, "  Off  ");
        }
    }

}

void updateCheatMenu() {
    bool redraw=false;
    int numCheats = ch->getNumCheats();

    if (cheatMenuSelection >= numCheats) {
        cheatMenuSelection = 0;
    }

    if (keyPressedAutoRepeat(mapMenuKey(MENU_KEY_UP))) {
        if (cheatMenuSelection > 0) {
            cheatMenuSelection--;
            redraw = true;
        }
    }
    else if (keyPressedAutoRepeat(mapMenuKey(MENU_KEY_DOWN))) {
        if (cheatMenuSelection < numCheats-1) {
            cheatMenuSelection++;
            redraw = true;
        }
    }
    else if (keyJustPressed(mapMenuKey(MENU_KEY_RIGHT)) |
            keyJustPressed(mapMenuKey(MENU_KEY_LEFT))) {
        ch->toggleCheat(cheatMenuSelection, !ch->isCheatEnabled(cheatMenuSelection));
        redraw = true;
    }
    else if (keyJustPressed(mapMenuKey(MENU_KEY_R))) {
        cheatMenuSelection += cheatsPerPage;
        if (cheatMenuSelection >= numCheats)
            cheatMenuSelection = 0;
        redraw = true;
    }
    else if (keyJustPressed(mapMenuKey(MENU_KEY_L))) {
        cheatMenuSelection -= cheatsPerPage;
        if (cheatMenuSelection < 0)
            cheatMenuSelection = numCheats-1;
        redraw = true;
    }
    if (keyJustPressed(mapMenuKey(MENU_KEY_B))) {
        closeSubMenu();
        if (!cheatMenu_gameboyWasPaused)
            gameboy->unpause();
    }

    if (redraw)
        doAtVBlank(redrawCheatMenu);
}

bool startCheatMenu() {
    ch = gameboy->getCheatEngine();

    if (ch == NULL || ch->getNumCheats() == 0)
        return false;

    cheatMenu_gameboyWasPaused = gameboy->isGameboyPaused();
    gameboy->pause();
    displaySubMenu(updateCheatMenu);
    redrawCheatMenu();

    return true;
}

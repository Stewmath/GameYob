#include <string.h>
#include <algorithm>
#include <nds.h>
#include "mmu.h"
#include "main.h"
#include "cheats.h"
#include "inputhelper.h"

#define TO_INT(a) ( (a) >= 'A' ? (a) - 'A' + 10 : (a) - '0')

bool     cheatsEnabled = true;
cheat_t  cheats[MAX_CHEATS];
int numCheats=0;

// Use this to check whether another rom has been loaded
char cheatsRomTitle[20] = "\0";

void enableCheats (bool enable)
{
    cheatsEnabled = enable;
}

bool addCheat (const char *str)
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
        cheats[i].flags |= FLAG_GAMEGENIE;
        
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
        cheats[i].flags |= FLAG_GAMEGENIE1;

        cheats[i].data = TO_INT(str[0]) << 4 | TO_INT(str[1]);
        cheats[i].address = TO_INT(str[6]) << 12 | TO_INT(str[2]) << 8 | 
            TO_INT(str[4]) << 4 | TO_INT(str[5]);

        printLog("GG1 %04x / %02x\n", cheats[i].address, cheats[i].data);
    }
    // Gameshark AAAAAAAA
    else if (len == 8) {
        cheats[i].flags |= FLAG_GAMESHARK;
        
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

void toggleCheat (int i, bool enabled) 
{
    if (enabled) {
        cheats[i].flags |= FLAG_ENABLED;
        if ((cheats[i].flags & FLAG_TYPE_MASK) != FLAG_GAMESHARK) {
            for (int j=0; j<numRomBanks; j++) {
                if (isRomBankLoaded(j))
                    applyGGCheatsToBank(j);
            }
        }
    }
    else {
        unapplyGGCheat(i);
        cheats[i].flags &= ~FLAG_ENABLED;
    }
}

void unapplyGGCheat(int cheat) {
    if ((cheats[cheat].flags & FLAG_TYPE_MASK) != FLAG_GAMESHARK) {
        for (unsigned int i=0; i<cheats[cheat].patchedBanks.size(); i++) {
            int bank = cheats[cheat].patchedBanks[i];
            if (isRomBankLoaded(bank)) {
                getRomBank(bank)[cheats[cheat].address&0x3fff] = cheats[cheat].patchedValues[i];
            }
        }
        cheats[cheat].patchedBanks = std::vector<int>();
        cheats[cheat].patchedValues = std::vector<int>();
    }
}

void applyGGCheatsToBank(int bank) {
	u8* bankPtr = getRomBank(bank);
    for (int i=0; i<numCheats; i++) {
        if (cheats[i].flags & FLAG_ENABLED && ((cheats[i].flags & FLAG_TYPE_MASK) != FLAG_GAMESHARK)) {

            int bankSlot = cheats[i].address/0x4000;
            if ((bankSlot == 0 && bank == 0) || (bankSlot == 1 && bank != 0)) {
                int address = cheats[i].address&0x3fff;
                if (((cheats[i].flags & FLAG_TYPE_MASK) == FLAG_GAMEGENIE1 || bankPtr[address] == cheats[i].compare) && 
                        find(cheats[i].patchedBanks.begin(), cheats[i].patchedBanks.end(), bank) == cheats[i].patchedBanks.end()) {

                    cheats[i].patchedBanks.push_back(bank);
                    cheats[i].patchedValues.push_back(bankPtr[address]);
                    bankPtr[address] = cheats[i].data;
                }
            }
        }
    }
}

void applyGSCheats (void) ITCM_CODE;

void applyGSCheats (void) 
{
    int i;
    int compareBank;

    for (i = 0; i < numCheats; i++) {
        if (cheats[i].flags & FLAG_ENABLED && ((cheats[i].flags & FLAG_TYPE_MASK) == FLAG_GAMESHARK)) {
            switch (cheats[i].bank & 0xf0) {
                case 0x90:
                    compareBank = wramBank;
                    wramBank = cheats[i].bank & 0x7;
                    writeMemory(cheats[i].address, cheats[i].data);
                    wramBank = compareBank;
                    break;
                case 0x80: /* TODO : Find info and stuff */
                    break;
                case 0x00:
                    writeMemory(cheats[i].address, cheats[i].data);
                    break;
            }
        }
    }
}

void loadCheats(const char* filename) {
    if (strcmp(cheatsRomTitle, getRomTitle()) == 0) {
        // Rom hasn't been changed
        for (int i=0; i<numCheats; i++)
            unapplyGGCheat(i);
    }
    else
        // Rom has been changed
        strncpy(cheatsRomTitle, getRomTitle(), 20);
    numCheats = 0;

    // Begin loading new cheat file
    FILE* file = fopen(filename, "r");
    if (file == NULL)
        return;

    while (!feof(file)) {
        int i = numCheats;

        char line[100];
        fgets(line, 100, file);

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

    fclose(file);
}

bool startCheatMenu() {
    const int cheatsPerPage=18;

    int numPages = (numCheats-1)/cheatsPerPage+1;
    bool quit=false;
    static int selection=0;

    if (numCheats == 0)
        return false;
    if (selection >= numCheats) {
        selection = 0;
    }

    while (!quit) {
        int page = selection/cheatsPerPage;
        consoleClear();
        iprintf("          Cheat Menu      ");
        iprintf("%d/%d\n\n", page+1, numPages);
        for (int i=page*cheatsPerPage; i<numCheats && i < (page+1)*cheatsPerPage; i++) {
            iprintf("%s", cheats[i].name);
            for (unsigned int j=0; j<25-strlen(cheats[i].name); j++)
                iprintf(" ");
            if (cheats[i].flags & FLAG_ENABLED) {
                if (selection == i)
                    iprintf("* On * ");
                else
                    iprintf("  On   ");
            }
            else {
                if (selection == i)
                    iprintf("* Off *");
                else
                    iprintf("  Off  ");
            }
        }

        while (true) {
            swiWaitForVBlank();
            readKeys();
            if (keyPressedAutoRepeat(KEY_UP)) {
                if (selection > 0) {
                    selection--;
                    break;
                }
            }
            else if (keyPressedAutoRepeat(KEY_DOWN)) {
                if (selection < numCheats-1) {
                    selection++;
                    break;
                }
            }
            else if (keyJustPressed(KEY_RIGHT | KEY_LEFT)) {
                toggleCheat(selection, !(cheats[selection].flags & FLAG_ENABLED));
                break;
            }
            else if (keyJustPressed(KEY_R)) {
                selection += cheatsPerPage;
                if (selection >= numCheats)
                    selection = 0;
                break;
            }
            else if (keyJustPressed(KEY_L)) {
                selection -= cheatsPerPage;
                if (selection < 0)
                    selection = numCheats-1;
                break;
            }
            if (keyJustPressed(KEY_B)) {
                return true;
            }
        }
    }

    return true;
}

void saveCheats(const char* filename) {
    if (numCheats == 0)
        return;
    FILE* file = fopen(filename, "w");
    for (int i=0; i<numCheats; i++) {
        fiprintf(file, "%s %d%s\n", cheats[i].cheatString, !!(cheats[i].flags & FLAG_ENABLED), cheats[i].name);
    }
    fclose(file);
}

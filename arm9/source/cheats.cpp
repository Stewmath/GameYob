#include <string.h>
#include <algorithm>
#include <nds.h>
#include "mmu.h"
#include "main.h"
#include "cheats.h"
#include "inputhelper.h"

bool     cheatsEnabled = true;
cheat_t  cheats[MAX_CHEATS];
u8       slots[MAX_CHEATS] = {0};
int numCheats=0;

#define IS_HEX(a) (((a) >= '0' && (a) <= '9') || ((a) >= 'A' && (a) <= 'F'))
#define TO_INT(a) ( (a) >= 'A' ? (a) - 'A' + 10 : (a) - '0')

void enableCheats (bool enable)
{
    cheatsEnabled = enable;
}

bool addCheat (const char *str)
{
    int len;
    int i;

    for (i = 0; i < MAX_CHEATS; i++)
        if (!(slots[i] & SLOT_USED))
            break;

    /* ENOFREESLOT */
    if (i == MAX_CHEATS)
        return false;
    /* Mark as used and clear it */
    slots[i] &= ~(SLOT_ENABLED | SLOT_TYPE_MASK);
    slots[i] |= SLOT_USED;

    len = strlen(str);
    if (len > 11)
        return false;

    strcpy(cheats[i].cheatString, str);

    /* GameGenie AAA-BBB-CCC */
    if (len == 11) {
        slots[i] |= SLOT_GAMEGENIE;
        
        cheats[i].data = TO_INT(str[0]) << 4 | TO_INT(str[1]);
        cheats[i].address = TO_INT(str[6]) << 12 | TO_INT(str[2]) << 8 | 
            TO_INT(str[4]) << 4 | TO_INT(str[5]);
        cheats[i].compare = TO_INT(str[8]) << 4 | TO_INT(str[10]);

        cheats[i].address ^= 0xf000;
        cheats[i].compare = (cheats[i].compare >> 2) | (cheats[i].compare&0x3) << 6;
        cheats[i].compare ^= 0xba;

        printLog("GG %04x / %02x -> %02x\n", cheats[i].address, cheats[i].data, cheats[i].compare);
    }
    /* GameGenie (6digit version) AAA-BBB */
    else if (len == 7) {
        slots[i] |= SLOT_GAMEGENIE1;

        cheats[i].data = TO_INT(str[0]) << 4 | TO_INT(str[1]);
        cheats[i].address = TO_INT(str[6]) << 12 | TO_INT(str[2]) << 8 | 
            TO_INT(str[4]) << 4 | TO_INT(str[5]);

        printLog("GG1 %04x / %02x\n", cheats[i].address, cheats[i].data);
    }
    /* Gameshark AAAAAAAA */
    else if (len == 8) {
        slots[i] |= SLOT_GAMESHARK;
        
        cheats[i].data = TO_INT(str[2]) << 4 | TO_INT(str[3]);
        cheats[i].bank = TO_INT(str[0]) << 4 | TO_INT(str[1]);
        cheats[i].address = TO_INT(str[6]) << 12 | TO_INT(str[7]) << 8 | 
            TO_INT(str[4]) << 4 | TO_INT(str[5]);

        printLog("GS (%02x)%04x/ %02x\n", cheats[i].bank, cheats[i].address, cheats[i].data);
    }
    else /* dafuq did i just read ? */
        return false;

    return true;
}

void removeCheat (int i)
{
    slots[i] &= ~(SLOT_ENABLED | SLOT_USED);
    unapplyGGCheat(i);
}

void toggleCheat (int i, bool enabled) 
{
    if (enabled) {
        slots[i] |= SLOT_ENABLED;
        if (slots[i] & (SLOT_GAMEGENIE | SLOT_GAMEGENIE1)) {
            for (int j=0; j<numRomBanks; j++) {
                if (bankLoaded(j))
                    applyGGCheats(j);
            }
        }
    }
    else {
        unapplyGGCheat(i);
        slots[i] &= ~SLOT_ENABLED;
    }
}

void unapplyGGCheat(int cheat) {
    if (slots[cheat] & (SLOT_GAMEGENIE | SLOT_GAMEGENIE1)) {
        for (int i=0; i<cheats[cheat].patchedBanks.size(); i++) {
            int bank = cheats[cheat].patchedBanks[i];
            if (bankLoaded(bank)) {
                rom[bank][cheats[cheat].address&0x3fff] = cheats[cheat].patchedValues[i];
            }
        }
        cheats[cheat].patchedBanks = std::vector<int>();
        cheats[cheat].patchedValues = std::vector<int>();
    }
}

void applyGGCheats(int romBank) {
    for (int i=0; i<MAX_CHEATS; i++) {
        if (slots[i] & SLOT_ENABLED && (slots[i] & SLOT_GAMEGENIE || slots[i] & SLOT_GAMEGENIE1)) {

            int bankSlot = cheats[i].address/0x4000;
            if ((bankSlot == 0 && romBank == 0) || (bankSlot == 1 && romBank != 0)) {
                int address = cheats[i].address&0x3fff;
                if ((slots[i] & SLOT_GAMEGENIE1 || rom[romBank][address] == cheats[i].compare) && 
                        find(cheats[i].patchedBanks.begin(), cheats[i].patchedBanks.end(), romBank) == cheats[i].patchedBanks.end()) {

                    cheats[i].patchedBanks.push_back(romBank);
                    cheats[i].patchedValues.push_back(rom[romBank][address]);
                    rom[romBank][address] = cheats[i].data;
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

    for (i = 0; i < MAX_CHEATS; i++) {
        if (slots[i] & SLOT_ENABLED && ((slots[i] & SLOT_TYPE_MASK) == SLOT_GAMESHARK)) {
            switch (cheats[i].bank & 0xf0) {
                case 0x90:
                    compareBank = wramBank;
                    wramBank = cheats[i].bank & 0xf;
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
    FILE* file = fopen(filename, "r");
    if (file == NULL)
        return;

    // Delete previously loaded cheats
    for (int i=0; i<numCheats; i++)
        removeCheat(i);

    numCheats=0;
    while (!feof(file)) {
        char line[100];
        fgets(line, 100, file);

        if (*line != '\0') {
            char* spacePos = strchr(line, ' ');
            if (spacePos != NULL) {
                *spacePos = '\0';
                if (strlen(spacePos+1) >= 1 && addCheat(line)) {
                    toggleCheat(numCheats, *(spacePos+1) == '1');
                    strncpy(cheats[numCheats].name, spacePos+2, MAX_CHEAT_NAME_LEN);
                    cheats[numCheats].name[MAX_CHEAT_NAME_LEN] = '\0';
                    char c;
                    while ((c = cheats[numCheats].name[strlen(cheats[numCheats].name)-1]) == '\n' || c == '\r')
                        cheats[numCheats].name[strlen(cheats[numCheats].name)-1] = '\0';
                    numCheats++;
                }
            }
        }
    }

    printLog("Loaded cheat file\n");
    fclose(file);
}

void startCheatMenu() {
    const int cheatsPerPage=14;

    bool quit=false;
    static int selection=0;
    static int firstCheat=0;

    if (numCheats == 0)
        return;
    if (selection >= numCheats) {
        selection = 0;
        firstCheat = selection;
    }

    while (!quit) {
        consoleClear();
        printf("          Cheat Menu\n\n");
        for (int i=firstCheat; i<numCheats && i < firstCheat+cheatsPerPage; i++) {
            if (slots[i] & SLOT_USED) {
                printf("%s", cheats[i].name);
                for (int j=0; j<25-strlen(cheats[i].name); j++)
                    printf(" ");
                if (slots[i] & SLOT_ENABLED) {
                    if (selection == i)
                        printf("* On * ");
                    else
                        printf("  On   ");
                }
                else {
                    if (selection == i)
                        printf("* Off *");
                    else
                        printf("  Off  ");
                }
            }
        }

        while (true) {
            swiWaitForVBlank();
            readKeys();
            if (keyPressedAutoRepeat(KEY_UP)) {
                if (selection > 0) {
                    selection--;
                    if (selection < firstCheat)
                        firstCheat = selection;
                    break;
                }
            }
            else if (keyPressedAutoRepeat(KEY_DOWN)) {
                if (selection < numCheats-1) {
                    selection++;
                    if (selection-firstCheat+1 > cheatsPerPage) {
                        firstCheat = selection-cheatsPerPage+1;
                        if (firstCheat < 0)
                            firstCheat = 0;
                    }

                    break;
                }
            }
            else if (keyJustPressed(KEY_RIGHT | KEY_LEFT)) {
                toggleCheat(selection, !(slots[selection] & SLOT_ENABLED));
                break;
            }
            else if (keyJustPressed(KEY_B)) {
                return;
            }
        }
    }
}

void saveCheats(const char* filename) {
    FILE* file = fopen(filename, "w");
    for (int i=0; i<numCheats; i++) {
        fprintf(file, "%s %d%s\n", cheats[i].cheatString, !!(slots[i] & SLOT_ENABLED), cheats[i].name);
    }
    fclose(file);
}

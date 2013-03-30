#include <string.h>
#include <nds.h>
#include "mmu.h"
#include "main.h"
#include "cheats.h"

bool     cheatsEnabled = true;
patch_t  patches[MAX_CHEATS];
u8       slots[MAX_CHEATS] = {0};

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

    /* GameGenie AAA-BBB-CCC */
    if (len == 11) {
        slots[i] |= SLOT_GAMEGENIE;
        
        patches[i].data = TO_INT(str[0]) << 4 | TO_INT(str[1]);
        patches[i].address = TO_INT(str[6]) << 12 | TO_INT(str[2]) << 8 | 
            TO_INT(str[4]) << 4 | TO_INT(str[5]);
        patches[i].compare = TO_INT(str[8]) << 4 | TO_INT(str[10]);

        patches[i].address ^= 0xf000;
        patches[i].compare = (patches[i].compare >> 2) | (patches[i].compare&0x3) << 6;
        patches[i].compare ^= 0xba;

        printLog("GG %04x / %02x -> %02x\n", patches[i].address, patches[i].data, patches[i].compare);
    }
    /* GameGenie (6digit version) AAA-BBB */
    else if (len == 7) {
        slots[i] |= SLOT_GAMEGENIE1;

        patches[i].data = TO_INT(str[0]) << 4 | TO_INT(str[1]);
        patches[i].address = TO_INT(str[6]) << 12 | TO_INT(str[2]) << 8 | 
            TO_INT(str[4]) << 4 | TO_INT(str[5]);

        printLog("GG1 %04x / %02x\n", patches[i].address, patches[i].data);
    }
    /* Gameshark AAAAAAAA */
    else if (len == 8) {
        slots[i] |= SLOT_GAMESHARK;
        
        patches[i].data = TO_INT(str[2]) << 4 | TO_INT(str[3]);
        patches[i].bank = TO_INT(str[0]) << 4 | TO_INT(str[1]);
        patches[i].address = TO_INT(str[6]) << 12 | TO_INT(str[7]) << 8 | 
            TO_INT(str[4]) << 4 | TO_INT(str[5]);

        printLog("GS (%02x)%04x/ %02x\n", patches[i].bank, patches[i].address, patches[i].data);
    }
    else /* dafuq did i just read ? */
        return false;

    return true;
}

void removeCheat (int i)
{
    slots[i] &= ~(SLOT_ENABLED | SLOT_USED);
}

void toggleCheat (int i, bool enabled) 
{
    if (enabled)
        slots[i] |= SLOT_ENABLED;
    else
        slots[i] &= ~SLOT_ENABLED;
}

u8 hookGGRead (u16 addr) 
{
    u8 mem;
    int i;

    mem = memory[addr>>12][addr&0xfff];

    if (!cheatsEnabled || addr > 0x7fff)
        return mem;

    for (i = 0; i < MAX_CHEATS; i++) {
        if (slots[i] & SLOT_ENABLED && patches[i].address == addr) {
            switch (slots[i] & SLOT_TYPE_MASK) {
                case SLOT_GAMEGENIE:
                    if (mem == patches[i].compare)
                        printLog("Applied cheat\n");
                    return (mem == patches[i].compare) ? patches[i].data : mem;
                case SLOT_GAMEGENIE1:
                    return patches[i].data;
            }
        }
    }

    return mem;
}

void applyGSCheats (void) ITCM_CODE;

void applyGSCheats (void) 
{
    int i;
    int compareBank;

    for (i = 0; i < MAX_CHEATS; i++) {
        if (slots[i] & SLOT_ENABLED && ((slots[i] & SLOT_TYPE_MASK) == SLOT_GAMESHARK)) {
            switch (patches[i].bank & 0xf0) {
                case 0x90:
                    compareBank = wramBank;
                    wramBank = patches[i].bank & 0xf;
                    writeMemory(patches[i].address, patches[i].data);
                    wramBank = compareBank;
                    break;
                case 0x80: /* TODO : Find info and stuff */
                    break;
                case 0x00:
                    writeMemory(patches[i].address, patches[i].data);
                    break;
            }
        }
    }
}

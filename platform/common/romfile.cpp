#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "main.h"
#include "romfile.h"
#include "gbs.h"
#include "console.h"
#include "inputhelper.h"
#include "gameboy.h"
#include "cheats.h"
#include "error.h"
#include "io.h"

#ifdef EMBEDDED_ROM
#include "rom_gb.h"
#endif


#ifdef DS
extern bool __dsimode;
#endif

RomFile::RomFile(const char* f) {

    romFile=NULL;
#ifdef DS
        if (__dsimode)
            maxLoadedRomBanks = 512; // 8 megabytes
        else
            maxLoadedRomBanks = 128; // 2 megabytes
#else
        maxLoadedRomBanks = 512;
#endif

    strcpy(filename, f);

#ifdef EMBEDDED_ROM
    gbsMode = false;

    numRomBanks = rom_gb_size/0x4000;
    numLoadedRomBanks = numRomBanks;
    romBankSlots = (u8*)rom_gb;

    for (int i=0; i<numRomBanks; i++) {
        bankSlotIDs[i] = i;
        lastBanksUsed.push_back(i);
    }
#else
    if (romBankSlots == NULL) {
        romBankSlots = (u8*)malloc(maxLoadedRomBanks*0x4000);
    }

    // Check if this is a GBS file
    gbsMode = (strcasecmp(strrchr(filename, '.'), ".gbs") == 0);

    romFile = file_open(filename, "rb");
    if (romFile == NULL)
    {
        fatalerr("Error opening %s.", filename);
        return;
    }

    if (gbsMode) {
        file_read(gbsHeader, 1, 0x70, romFile);
        gbsReadHeader();
        file_seek(romFile, 0, SEEK_END);
        numRomBanks = (file_tell(romFile)-0x70+0x3fff)/0x4000; // Get number of banks, rounded up
        printf("%.2x\n", numRomBanks);
    }
    else {
        file_seek(romFile, 0, SEEK_END);
        numRomBanks = (file_tell(romFile)+0x3fff)/0x4000; // Get number of banks, rounded up
    }

    // Round numRomBanks to a power of 2
    int n=1;
    while (n < numRomBanks) n*=2;
    numRomBanks = n;

    //int rawRomSize = file_tell(romFile);
    file_seek(romFile, 0, SEEK_SET);

    if (numRomBanks <= maxLoadedRomBanks)
        numLoadedRomBanks = numRomBanks;
    else
        numLoadedRomBanks = maxLoadedRomBanks;

    for (int i=0; i<numRomBanks; i++) {
        bankSlotIDs[i] = -1;
    }

    // Load rom banks and initialize all those "bank" arrays
    lastBanksUsed = std::vector<int>();
    // Read bank 0
    if (gbsMode) {
        bankSlotIDs[0] = 0;
        file_seek(romFile, 0x70, SEEK_SET);
        file_read(romBankSlots+gbsLoadAddress, 1, 0x4000-gbsLoadAddress, romFile);
    }
    else {
        bankSlotIDs[0] = 0;
        file_seek(romFile, 0, SEEK_SET);
        file_read(romBankSlots, 1, 0x4000, romFile);
    }
    // Read the rest of the banks
    for (int i=1; i<numLoadedRomBanks; i++) {
        bankSlotIDs[i] = i;
        file_read(romBankSlots+0x4000*i, 1, 0x4000, romFile);
        lastBanksUsed.push_back(i);
    }

#endif

    romSlot0 = romBankSlots;
    romSlot1 = romBankSlots + 0x4000;


    strcpy(basename, filename);
    *(strrchr(basename, '.')) = '\0';

    u8 cgbFlag = getCgbFlag();

    int nameLength = 16;
    if (cgbFlag == 0x80 || cgbFlag == 0xc0)
        nameLength = 15;
    for (int i=0; i<nameLength; i++) 
        romTitle[i] = (char)romSlot0[i+0x134];
    romTitle[nameLength] = '\0';

    if (gbsMode) {
        MBC = MBC5;
    }
    else {
        switch (getMapper()) {
            case 0: case 8: case 9:
                MBC = MBC0; 
                break;
            case 1: case 2: case 3:
                MBC = MBC1;
                break;
            case 5: case 6:
                MBC = MBC2;
                break;
                //case 0xb: case 0xc: case 0xd:
                //MBC = MMM01;
                //break;
            case 0xf: case 0x10: case 0x11: case 0x12: case 0x13:
                MBC = MBC3;
                break;
                //case 0x15: case 0x16: case 0x17:
                //MBC = MBC4;
                //break;
            case 0x19: case 0x1a: case 0x1b: 
                MBC = MBC5;
                break;
            case 0x1c: case 0x1d: case 0x1e: // MBC5 with rumble
                MBC = MBC5;
                break;
            case 0x22:
                MBC = MBC7;
                break;
            case 0xea: /* Hack for SONIC5 */
                MBC = MBC1;
                break;
            case 0xfe:
                MBC = HUC3;
                break;
            case 0xff:
                MBC = HUC1;
                break;
            default:
                printLog("Unsupported MBC %02x\n", getMapper());
                MBC = MBC5;
                break;
        }

    } // !gbsMode

#ifndef EMBEDDED_ROM
    // If we've loaded everything, close the rom file
    if (numRomBanks <= numLoadedRomBanks) {
        file_close(romFile);
        romFile = NULL;
    }
#endif
}

RomFile::~RomFile() {
    if (romFile != NULL)
        file_close(romFile);
#ifndef EMBEDDED_ROM
    free(romBankSlots);
#endif
}


void RomFile::loadRomBank(int romBank) {
    if (bankSlotIDs[romBank] != -1 || numRomBanks <= numLoadedRomBanks || romBank == 0) {
        romSlot1 = romBankSlots+bankSlotIDs[romBank]*0x4000;
        return;
    }
    int bankToUnload = lastBanksUsed.back();
    lastBanksUsed.pop_back();
    int slot = bankSlotIDs[bankToUnload];
    bankSlotIDs[bankToUnload] = -1;
    bankSlotIDs[romBank] = slot;

    file_seek(romFile, 0x4000*romBank, SEEK_SET);
    file_read(romBankSlots+slot*0x4000, 1, 0x4000, romFile);

    lastBanksUsed.insert(lastBanksUsed.begin(), romBank);

    gameboy->getCheatEngine()->applyGGCheatsToBank(romBank);

    romSlot1 = romBankSlots+slot*0x4000;

    loadBios("\0");
}

bool RomFile::isRomBankLoaded(int bank) {
    return bankSlotIDs[bank] != -1;
}
u8* RomFile::getRomBank(int bank) {
    if (!isRomBankLoaded(bank))
        return 0;
    return romBankSlots+bankSlotIDs[bank]*0x4000;
}

const char* RomFile::getBasename() {
    return basename;
}


void RomFile::loadBios(const char* filename) {
    FileHandle* file;

    if (biosExists)
        goto load;

    file = file_open(filename, "rb");
    biosExists = file != NULL;
    if (biosExists) {
        file_read(bios, 1, 0x900, file);
        file_close(file);
    }
    else
        return;
load:
    // Little hack to preserve "quickread" from gbcpu.cpp.
    for (int i=0x100; i<0x150; i++)
        bios[i] = romSlot0[i];

}

char* RomFile::getRomTitle() {
    return romTitle;
}

#include "gameboy.h"
#include "cheats.h"
#include "inputhelper.h"
#include "libfat_fake.h"

const char *mbcNames[] = {"ROM","MBC1","MBC2","MBC3","MBC4","MBC5","MBC7","HUC3","HUC1"};

extern bool __dsimode;

void Gameboy::gbioInit() {
}


// This bypasses libfat's cache to directly write a single sector of the save 
// file. This reduces lag.
void Gameboy::writeSaveFileSectors(int startSector, int numSectors) {
    if (saveFileSectors[startSector] == -1) {
        flushFatCache();
        return;
    }
    devoptab_t* devops = (devoptab_t*)GetDeviceOpTab ("sd");
    PARTITION* partition = (PARTITION*)devops->deviceData;
    CACHE* cache = partition->cache;

	_FAT_disc_writeSectors(cache->disc, saveFileSectors[startSector], numSectors, gameboy->externRam+startSector*512);
}

void Gameboy::loadRomBank() {
    if (bankSlotIDs[romBank] != -1 || numRomBanks <= numLoadedRomBanks || romBank == 0) {
        romSlot1 = romBankSlots+bankSlotIDs[romBank]*0x4000;
        return;
    }
    int bankToUnload = lastBanksUsed.back();
    lastBanksUsed.pop_back();
    int slot = bankSlotIDs[bankToUnload];
    bankSlotIDs[bankToUnload] = -1;
    bankSlotIDs[romBank] = slot;

    fseek(romFile, 0x4000*romBank, SEEK_SET);
    fread(romBankSlots+slot*0x4000, 1, 0x4000, romFile);

    lastBanksUsed.insert(lastBanksUsed.begin(), romBank);

    cheatEngine->applyGGCheatsToBank(romBank);

    romSlot1 = romBankSlots+slot*0x4000;
}

bool Gameboy::isRomBankLoaded(int bank) {
    return bankSlotIDs[bank] != -1;
}
u8* Gameboy::getRomBank(int bank) {
    if (!isRomBankLoaded(bank))
        return 0;
    return romBankSlots+bankSlotIDs[bank]*0x4000;
}

const char* Gameboy::getRomBasename() {
    return basename;
}

void Gameboy::printRomInfo() {
    consoleClear();
    iprintf("ROM Title: \"%s\"\n", romTitle);
    iprintf("Cartridge type: %.2x (%s)\n", mapper, mbcNames[MBC]);
    iprintf("ROM Size: %.2x (%d banks)\n", romSize, numRomBanks);
    iprintf("RAM Size: %.2x (%d banks)\n", ramSize, numRamBanks);
}


void Gameboy::loadBios(const char* filename) {
    FILE* file = fopen(filename, "rb");
    biosExists = file != NULL;
    if (biosExists)
        fread(bios, 1, 0x900, file);
}

char* Gameboy::getRomTitle() {
    return romTitle;
}

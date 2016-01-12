#pragma once
#ifdef DS
#include <nds.h>
#endif

#include <stdio.h>
#include <vector>
#include "io.h"

#define MAX_ROM_BANKS   0x200

/* All the possible MBC */
enum {
    MBC0 = 0,
    MBC1,
    MBC2,
    MBC3,
    MBC4, // Unsupported
    MBC5,
    MBC7, // Unsupported
    HUC1,
    HUC3,
    MBC_MAX,
};

class RomFile {
    public:
        RomFile(const char* filename);
        ~RomFile();

        void loadRomBank(int romBank);
        bool isRomBankLoaded(int bank);
        u8* getRomBank(int bank);
        const char* getBasename();

        char* getRomTitle();
        void loadBios(const char* filename);

        void saveState(int num);
        int loadState(int num);
        void deleteState(int num);
        bool checkStateExists(int num);

        inline int getNumRomBanks() { return numRomBanks; }
        inline int getNumSramBanks() { return numRamBanks; }
        inline int getCgbFlag() { return romSlot0[0x143]; }
        inline int getRamSize() { return romSlot0[0x149]; }
        inline int getMapper() { return romSlot0[0x147]; }
        inline int getMBC() { return MBC; }
        inline bool hasRumble() { return MBC == 0x1c || MBC == 0x1d || MBC == 0x1e; }

        u8* romSlot0;
        u8* romSlot1;

        u8 bios[0x900];

    private:
        u8* romBankSlots = NULL; // Each 0x4000 bytes = one slot

        int numRomBanks;
        int maxLoadedRomBanks;
        int numLoadedRomBanks;
        int numRamBanks;
        int bankSlotIDs[MAX_ROM_BANKS]; // Keeps track of which bank occupies which slot
        std::vector<int> lastBanksUsed;

        FileHandle* romFile;
        char filename[256];
        char basename[256];
        char romTitle[20];

        int MBC;
};

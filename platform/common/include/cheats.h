#pragma once
#include <vector>

#ifdef DS
#include <nds.h>
#endif

class Gameboy;

#define MAX_CHEAT_NAME_LEN  24

#define MAX_CHEATS      900
#define CHEAT_FLAG_ENABLED    (1<<0)
#define CHEAT_FLAG_TYPE_MASK  (3<<2)
#define CHEAT_FLAG_GAMEGENIE  (1<<2)
#define CHEAT_FLAG_GAMEGENIE1 (2<<2)
#define CHEAT_FLAG_GAMESHARK  (3<<2)


typedef struct cheat_t {
    char name[MAX_CHEAT_NAME_LEN+1];
    char cheatString[12];
    u8 flags;
    u8  data;
    u16 address;
    union {
        u8 compare; /* For GameGenie codes */
        u8 bank;/* For Gameshark codes */
    };
    std::vector<int> patchedBanks; /* For GameGenie codes */
    std::vector<int> patchedValues; /* For GameGenie codes */
} cheat_t;

class CheatEngine {
    public:
        CheatEngine(Gameboy* g);
        void setRomFile(RomFile* r);

        void enableCheats(bool enable);
        bool addCheat(const char *str);
        void toggleCheat(int i, bool enabled);

        void unapplyGGCheat(int cheat);
        void applyGGCheatsToBank(int romBank);

        void applyGSCheats();

        void loadCheats(const char* filename);
        void saveCheats(const char* filename);

        inline int getNumCheats() {return numCheats;}
        inline bool isCheatEnabled(int c) {return cheats[c].flags & CHEAT_FLAG_ENABLED;}
        inline bool areCheatsEnabled() { return cheatsEnabled; }

        cheat_t cheats[MAX_CHEATS];
    private:
        // variables
        bool cheatsEnabled;
        int numCheats;
        // Use this to check whether another rom has been loaded
        char cheatsRomTitle[20];

        Gameboy* gameboy;
        RomFile* romFile;
};

bool startCheatMenu();
void redrawCheatMenu();

#pragma once
#include <vector>

#define MAX_CHEAT_NAME_LEN  24

typedef struct cheat_t {
    char name[MAX_CHEAT_NAME_LEN+1];
    char cheatString[12];
    u8  data;
    u16 address;
    union {
        u8 compare; /* For GameGenie codes */
        u8 bank;/* For Gameshark codes */
    };
    std::vector<int> patchedBanks; /* For GameGenie codes */
    std::vector<int> patchedValues; /* For GameGenie codes */
} cheat_t;


#define MAX_CHEATS      14 
#define SLOT_ENABLED    (1<<0)
#define SLOT_USED       (1<<1)
#define SLOT_TYPE_MASK  (3<<2)
#define SLOT_GAMEGENIE  (1<<2)
#define SLOT_GAMEGENIE1 (2<<2)
#define SLOT_GAMESHARK  (3<<2)

extern bool cheatsEnabled;
extern cheat_t cheats[MAX_CHEATS];
extern u8   slots[MAX_CHEATS];
extern int numCheats;

void enableCheats(bool enable);
bool addCheat(const char *str);
void removeCheat(int i);
void toggleCheat(int i, bool enabled);

void unapplyGGCheat(int cheat);
void applyGGCheats(int romBank);

void applyGSCheats(void);

void startCheatMenu();
void loadCheats(const char* filename);
void saveCheats(const char* filename);

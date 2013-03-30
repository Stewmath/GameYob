#pragma once
#include <vector>

typedef struct patch_t {
    u8  data;
    u16 address;
    union {
        u8 compare; /* For GameGenie codes */
        u8 bank;/* For Gameshark codes */
    };
    std::vector<int> patchedBanks; /* For GameGenie codes */
    std::vector<int> patchedValues; /* For GameGenie codes */
} patch_t;

#define MAX_CHEATS      14 
#define SLOT_ENABLED    (1<<0)
#define SLOT_USED       (1<<1)
#define SLOT_TYPE_MASK  (3<<2)
#define SLOT_GAMEGENIE  (1<<2)
#define SLOT_GAMEGENIE1 (2<<2)
#define SLOT_GAMESHARK  (3<<2)

extern bool cheatsEnabled;
extern patch_t patches[MAX_CHEATS];
extern u8   slots[MAX_CHEATS];

void enableCheats(bool enable);
bool addCheat(const char *str);
void removeCheat(int i);
void toggleCheat(int i, bool enabled);

void unapplyGGCheat(int cheat);
void applyGGCheats(int romBank);

void applyGSCheats(void);

#pragma once
#include "global.h"
#include <stdio.h>

/* All the possible MBC */
enum {
    MBC0 = 0,
    MBC1,
    MBC2,
    MBC3,
    MBC4, // Unsupported
    MBC5,
    MBC7, // Unsupported
    HUC3,
    HUC1,
    MBC_MAX,
};

#define BUTTONA		0x1
#define BUTTONB		0x2
#define SELECT		0x4
#define START		0x8
#define RIGHT		0x10
#define LEFT		0x20
#define UP			0x40
#define DOWN		0x80

const int MAX_ROM_BANKS = 0x200;

extern int maxLoadedRomBanks;
extern int numLoadedRomBanks;

extern u8* romSlot0;
extern u8* romSlot1;

extern u8 buttonsPressed;

extern bool suspendStateExists;
extern bool advanceFrame;

extern FILE* saveFile;

void initInput();
void flushFatCache();

void startKeyConfigChooser();
void readConfigFile();
void writeConfigFile();

void readKeys();
bool keyPressed(int key);
bool keyPressedAutoRepeat(int key);
bool keyJustPressed(int key);
// Consider this key unpressed until released and pressed again
void forceReleaseKey(int key);

void loadBios(const char* filename);
int loadProgram(char* filename);
void loadRomBank();
bool isRomBankLoaded(int bank);
u8* getRomBank(int bank);
int loadSave();
int saveGame();
char* getRomTitle();
void printRomInfo();

void updateInput();

void saveState(int num);
int loadState(int num);

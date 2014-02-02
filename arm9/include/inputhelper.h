#pragma once
#include <stdio.h>
#include "global.h"

#define FAT_CACHE_SIZE 16

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

enum {
    KEY_NONE,
    KEY_GB_A, KEY_GB_B, KEY_GB_LEFT, KEY_GB_RIGHT, KEY_GB_UP, KEY_GB_DOWN, KEY_GB_START, KEY_GB_SELECT,
    KEY_MENU, KEY_MENU_PAUSE, KEY_SAVE, KEY_GB_AUTO_A, KEY_GB_AUTO_B, KEY_FAST_FORWARD, KEY_FAST_FORWARD_TOGGLE,
    KEY_SCALE, KEY_RESET
};

/*
extern char filename[256];
extern char savename[256];
extern char basename[256];
*/

extern bool fastForwardMode; // controlled by the toggle hotkey
extern bool fastForwardKey;  // only while its hotkey is pressed


extern int maxLoadedRomBanks;

extern bool suspendStateExists;


extern FILE* saveFile;

extern char* borderPath;

void initInput();
void flushFatCache();

void startKeyConfigChooser();
bool readConfigFile();
void writeConfigFile();

void readKeys();
bool keyPressed(int key);
bool keyPressedAutoRepeat(int key);
bool keyJustPressed(int key);
// Consider this key unpressed until released and pressed again
void forceReleaseKey(int key);
int mapGbKey(int gbKey); // Maps a "functional" key to a physical key.

//void loadBios(const char* filename);

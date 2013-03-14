#pragma once
#include "global.h"
#include <stdio.h>

#define BUTTONA		0x1
#define BUTTONB		0x2
#define SELECT		0x4
#define START		0x8
#define RIGHT		0x10
#define LEFT		0x20
#define UP			0x40
#define DOWN		0x80

#define MAX_LOADED_ROM_BANKS	128 /* 64 = 1 MiB */

extern u8 buttonsPressed;
extern char filename[100];

extern int GB_KEY_A;
extern int GB_KEY_B;

void initInput();
void readKeys();
bool keyPressed(int key);
bool keyPressedAutoRepeat(int key);
bool keyJustPressed(int key);
// Consider this key unpressed until released and pressed again
void forceReleaseKey(int key);

char* startFileChooser();
int loadProgram(char* filename);
void loadRomBank();
int loadSave();
int saveGame();
char* getRomTitle();
void printRomInfo();

int handleEvents();

void enableTurbo();
void disableTurbo();

void saveState(int num);
int loadState(int num);

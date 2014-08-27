#pragma once
#include "console.h"

extern bool consoleInitialized;
extern bool consoleDebugOutput;
extern int gbcModeOption;
extern bool gbaModeOption;
extern int sgbModeOption;
extern int biosEnabled;
extern bool soundDisabled;
extern bool hyperSound;
extern bool customBordersEnabled;
extern bool sgbBordersEnabled;
extern bool autoSavingEnabled;
extern bool printerEnabled;
extern int stateNum;
extern int consoleScreen;

extern bool hblankDisabled;
extern bool windowDisabled;

extern PrintConsole* menuConsole;

extern bool fpsOutput;
extern bool timeOutput;

extern int rumbleStrength;



void setMenuDefaults();

void displayMenu();
void closeMenu(); // updateScreens may need to be called after this
bool isMenuOn();

void redrawMenu();
void updateMenu();
void printMenuMessage(const char* s);

void displaySubMenu(void (*updateFunc)());
void closeSubMenu();

int getMenuOption(const char* name);
void setMenuOption(const char* name, int value);
void enableMenuOption(const char* name);
void disableMenuOption(const char* name);

void menuParseConfig(const char* line);
void menuPrintConfig(FILE* file);


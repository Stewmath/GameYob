#pragma once
#include <stdarg.h>
#include <stdio.h>
#include <nds.h>

extern bool nukeBorder;

extern bool consoleInitialized;
extern bool consoleDebugOutput;
extern int gbcModeOption;
extern bool gbaModeOption;
extern int sgbModeOption;
extern bool customBordersEnabled;
extern bool sgbBordersEnabled;
extern bool autoSavingEnabled;
extern bool printerEnabled;
extern int stateNum;
extern PrintConsole* menuConsole;

extern volatile int consoleSelectedRow; // This line is given a different backdrop


void setMenuDefaults();

void displayMenu();
void closeMenu(); // updateScreens may need to be called after this
bool isMenuOn();
bool isConsoleOn(); // Returns true if the sub-screen's console is set up.

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

void updateScreens(bool waitToFinish=false);


void setPrintConsole(PrintConsole* console);
PrintConsole* getPrintConsole();

void consoleSetPosColor(int x, int y, int color);
void consoleSetLineColor(int line, int color);
void iprintfColored(int palette, const char* format, ...);
void printLog(const char* format, ...);

int checkRumble();

void disableSleepMode();
void enableSleepMode();


enum {
    CONSOLE_COLOR_BLACK=0,
    CONSOLE_COLOR_RED,
    CONSOLE_COLOR_GREEN,
    CONSOLE_COLOR_YELLOW,
    CONSOLE_COLOR_BLUE,
    CONSOLE_COLOR_MAGENTA,
    CONSOLE_COLOR_CYAN,
    CONSOLE_COLOR_GREY,
    CONSOLE_COLOR_LIGHT_BLACK,
    CONSOLE_COLOR_LIGHT_RED,
    CONSOLE_COLOR_LIGHT_GREEN,
    CONSOLE_COLOR_LIGHT_YELLOW,
    CONSOLE_COLOR_LIGHT_BLUE,
    CONSOLE_COLOR_LIGHT_MAGENTA,
    CONSOLE_COLOR_LIGHT_CYAN,
    CONSOLE_COLOR_WHITE
};

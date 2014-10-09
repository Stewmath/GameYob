#pragma once
#include <stdarg.h>
#include <stdio.h>

#ifdef DS
#include <nds/arm9/console.h>
#endif

#ifdef SDL
struct PrintConsole {

};
#endif

#ifdef _3DS
#include "printconsole.h"
#endif

extern PrintConsole* menuConsole;

extern volatile int consoleSelectedRow; // This line is given a different backdrop


bool isConsoleOn(); // Returns true if the sub-screen's console is set up.

void clearConsole();
void consoleFlush();
PrintConsole* getDefaultConsole();

void updateScreens(bool waitToFinish=false);


void consoleSetPosColor(int x, int y, int color);
void consoleSetLineColor(int line, int color);
void iprintfColored(int palette, const char* format, ...);
void printLog(const char* format, ...);

int checkRumble();


void disableSleepMode();
void enableSleepMode();

void setPrintConsole(PrintConsole* console);
PrintConsole* getPrintConsole();


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

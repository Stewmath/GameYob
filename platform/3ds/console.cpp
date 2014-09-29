#include <stdio.h>
#include <stdlib.h>
#include "console.h"

volatile int consoleSelectedRow;


bool isConsoleOn() {
    return true;
}

void clearConsole() {
	iprintf("\x1b[2J");
}

PrintConsole* getDefaultConsole() {
    return NULL;
}

void updateScreens(bool waitToFinish) {

}


void consoleSetPosColor(int x, int y, int color) {

}

void consoleSetLineColor(int line, int color) {

}

void iprintfColored(int palette, const char* format, ...) {
    va_list args;
    va_start(args, format);

    vprintf(format, args);
}
void printLog(const char* format, ...) {
    va_list args;
    va_start(args, format);

    vprintf(format, args);
}

int checkRumble() {
    return 0;
}


void disableSleepMode() {

}
void enableSleepMode() {

}

void setPrintConsole(PrintConsole* console) {

}
PrintConsole* getPrintConsole() {
    return NULL;
}

#include <stdio.h>
#include <stdlib.h>
#include "console.h"

volatile int consoleSelectedRow;


bool isConsoleOn() {
    return true;
}

void clearConsole() {
    system("clear");
}

void consoleFlush() {
    fflush(stdout);
}

PrintConsole* getDefaultConsole() {
    return NULL;
}

int consoleGetWidth() {
    return 32;
}
int consoleGetHeight() {
    return 24;
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

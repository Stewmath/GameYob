#include <stdarg.h>
#include <stdio.h>

extern bool consoleDebugOutput;
extern int selectedGameboyMode;
extern bool gbaMode;

void initConsole();
void printConsoleMessage(char* s);

void enterConsole(); // May be called from an interrupt
void exitConsole();

bool isConsoleEnabled();
int displayConsole();
void updateConsoleScreen(); // Disables console screen if necessary

void consoleParseConfig(const char* line);
void consolePrintConfig(FILE* file);
void addToLog(const char* format, va_list args);

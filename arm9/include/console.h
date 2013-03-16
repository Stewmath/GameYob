#include <stdarg.h>
#include <stdio.h>

extern bool consoleDebugOutput;

void initConsole();
void printConsoleMessage(char* s);
void enterConsole();
void exitConsole();
bool isConsoleEnabled();
int displayConsole();
void consoleParseConfig(const char* option, const char* value);
void consolePrintConfig(FILE* file);
void addToLog(const char* format, va_list args);

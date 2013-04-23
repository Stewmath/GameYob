#include <stdarg.h>
#include <stdio.h>

extern bool nukeBorder;

extern bool consoleDebugOutput;
extern int selectedGameboyMode;
extern bool gbaModeOption;
extern int sgbModeOption;
extern bool customBordersEnabled;
extern bool sgbBordersEnabled;

void initConsole();

void printConsoleMessage(const char* s);

void enterConsole(); // May be called from an interrupt
void exitConsole();

bool isConsoleEnabled();
void displayConsole();
void updateConsoleScreen(); // Disables console screen if necessary

void consoleParseConfig(const char* line);
void consolePrintConfig(FILE* file);
void addToLog(const char* format, va_list args);

void OpenNorWrite();
void CloseNorWrite();
uint32 ReadNorFlashID();

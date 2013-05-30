#include <stdarg.h>
#include <stdio.h>

extern bool nukeBorder;

extern bool consoleOn;

extern bool consoleDebugOutput;
extern int gbcModeOption;
extern bool gbaModeOption;
extern int sgbModeOption;
extern bool customBordersEnabled;
extern bool sgbBordersEnabled;
extern bool autoSavingEnabled;

void initConsole();

void printConsoleMessage(const char* s);

void enterConsole(); // May be called from an interrupt
void exitConsole();

bool isConsoleEnabled();
void displayConsole();
void updateScreens();

void consoleParseConfig(const char* line);
void consolePrintConfig(FILE* file);
void addToLog(const char* format, va_list args);

void OpenNorWrite();
void CloseNorWrite();
uint32 ReadNorFlashID();

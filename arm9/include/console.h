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

void setConsoleDefaults();

void printConsoleMessage(const char* s);

void enterConsole(); // May be called from an interrupt
void exitConsole();

bool isConsoleEnabled();
void displayConsole();

int getConsoleOption(const char* name);
void setConsoleOption(const char* name, int value);

void updateScreens();

void disableSleepMode();
void enableSleepMode();

void consoleParseConfig(const char* line);
void consolePrintConfig(FILE* file);
void addToLog(const char* format, va_list args);

void OpenNorWrite();
void CloseNorWrite();
uint32 ReadNorFlashID();

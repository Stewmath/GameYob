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

extern volatile int consoleSelectedRow; // This line is given a different backdrop

void setConsoleDefaults();

void consoleSetPosColor(int x, int y, int color);
void consoleSetLineColor(int line, int color);

void iprintfColored(int palette, const char* format, ...);

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

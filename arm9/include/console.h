#include <stdarg.h>
#include <stdio.h>

extern bool nukeBorder;

extern bool consoleOn;

extern bool consoleInitialized;
extern bool consoleDebugOutput;
extern int gbcModeOption;
extern bool gbaModeOption;
extern int sgbModeOption;
extern bool customBordersEnabled;
extern bool sgbBordersEnabled;
extern bool autoSavingEnabled;
extern int stateNum;

extern volatile int consoleSelectedRow; // This line is given a different backdrop

void setConsoleDefaults();

void consoleSetPosColor(int x, int y, int color);
void consoleSetLineColor(int line, int color);

void iprintfColored(int palette, const char* format, ...);

void redrawMenu();

void printConsoleMessage(const char* s);

void displayMenu();
void closeMenu(); // updateScreens may need to be called after this

void updateMenu();

void displaySubMenu(void (*updateFunc)());
void closeSubMenu();

bool isMenuOn();

int getMenuOption(const char* name);
void setMenuOption(const char* name, int value);
void enableMenuOption(const char* name);
void disableMenuOption(const char* name);

void updateScreens(bool waitToFinish=false);

void disableSleepMode();
void enableSleepMode();

void consoleParseConfig(const char* line);
void consolePrintConfig(FILE* file);
void addToLog(const char* format, va_list args);

int checkRumble();
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

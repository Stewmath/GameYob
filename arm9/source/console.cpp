#include <nds.h>
#include "console.h"
#include "inputhelper.h"
#include "gbsnd.h"
#include "main.h"
#include "gameboy.h"
#include "mmu.h"
#include "nifi.h"

const int screenTileWidth = 32;
bool consoleDebugOutput = false;
bool quitConsole = false;
bool consoleOn = false;
int displayConsoleRetval=0;
int menu=0;
int option = -1;
char printMessage[33];


int stateNum=0;

extern int interruptWaitMode;
extern bool advanceFrame;
extern bool windowDisabled;
extern bool hblankDisabled;
extern int frameskip;
extern int halt;

extern bool __dsimode;

void selectRomFunc(int value) {
    printConsoleMessage("Saving SRAM...");
    saveGame();
    printMessage[0] = '\0';
    char* filename = startFileChooser();
    loadProgram(filename);
    free(filename);
    initializeGameboy();
    quitConsole = true;
    displayConsoleRetval = 1;
}
void setScreenFunc(int value) {
    if (value)
        lcdMainOnBottom();
    else
        lcdMainOnTop();
}
void buttonModeFunc(int value) {
    if (value == 0) {
        GB_KEY_A = KEY_A;
        GB_KEY_B = KEY_B;
    }
    else {
        GB_KEY_A = KEY_B;
        GB_KEY_B = KEY_Y;
    }
}
void consoleOutputFunc(int value) {
    if (value == 0) {
        fpsOutput = false;
        timeOutput = false;
        consoleDebugOutput = false;
    }
    else if (value == 1) {
        fpsOutput = false;
        timeOutput = true;
        consoleDebugOutput = false;
    }
    else if (value == 2) {
        fpsOutput = true;
        timeOutput = true;
        consoleDebugOutput = false;
    }
    else if (value == 3) {
        fpsOutput = false;
        timeOutput = false;
        consoleDebugOutput = true;
    }
}

void biosEnableFunc(int value) {
    biosEnabled = value;
}

void nifiEnableFunc(int value) {
    if (value)
        enableNifi();
    else
        disableNifi();
}

void saveSettingsFunc(int value) {
    writeConfigFile();
    printConsoleMessage("Settings saved.");
}

void stateSelectFunc(int value) {
    stateNum = value;
}
void stateLoadFunc(int value) {
    printConsoleMessage("Loading state...");
    if (loadState(stateNum) == 0) {
        printMessage[0] = '\0';
        quitConsole = true;
    }
}
void stateSaveFunc(int value) {
    printConsoleMessage("Saving state...");
    saveState(stateNum);
    printConsoleMessage("State saved.");
}
void resetFunc(int value) {
    initializeGameboy();
    quitConsole = true;
    displayConsoleRetval = 1;
}
void returnFunc(int value) {
    quitConsole = true;
}


void vblankWaitFunc(int value) {
    interruptWaitMode = value;
}
void hblankEnableFunc(int value) {
    hblankDisabled = !value;
    if (value) {
        irqEnable(IRQ_HBLANK);
    }
    else
        irqDisable(IRQ_HBLANK);
}
void windowEnableFunc(int value) {
    windowDisabled = !value;
    if (windowDisabled)
        REG_DISPCNT &= ~DISPLAY_WIN0_ON;
    else
        REG_DISPCNT |= DISPLAY_WIN0_ON;
}
void soundEnableFunc(int value) {
    soundDisabled = !value;
    soundDisable();
}
void advanceFrameFunc(int value) {
    advanceFrame = true;
    quitConsole = true;
}
void romInfoFunc(int value) {
    printRomInfo();
}

void setChanEnabled(int chan, int value) {
    if (value == 0)
        disableChannel(chan);
    else
        enableChannel(chan);
}
void chan1Func(int value) {
    setChanEnabled(0, value);
}
void chan2Func(int value) {
    setChanEnabled(1, value);
}
void chan3Func(int value) {
    setChanEnabled(2, value);
}
void chan4Func(int value) {
    setChanEnabled(3, value);
}

void serialFunc(int value) {
    ioRam[0x01] = packetData;
    requestInterrupt(SERIAL);
    ioRam[0x02] &= ~0x80;
}


struct ConsoleSubMenu {
    char *name;
    int numOptions;
    int numSelections[10];
    char *options[10];
    char *optionValues[10][10];
    void (*optionFunctions[10])(int);
    int defaultOptionSelections[10];
    int optionSelections[10];
};

ConsoleSubMenu menuList[] = { 
    {
        "Options",
        5,
        {0,         10,                                         0,              0,              0},
        {"Load ROM","State Slot",                               "Save State",   "Load State",   "Reset"},
        {{},        {"0","1","2","3","4","5","6","7","8","9"},  {},             {},             {}},
        {selectRomFunc,stateSelectFunc,                         stateSaveFunc,  stateLoadFunc,   resetFunc},
        {0,             0,                                      0,              0,              0}
    },
    {
        "Settings",
        6,
        {2,              2,              4,                                 2,              2,              0},
        {"Game Screen", "A & B Buttons", "Console Output",                  "GBC Bios",     "NiFi",         "Save Settings"},
        {{"Top","Bottom"},{"A/B", "B/Y"},{"Off","Time","FPS+Time","Debug"},{"Off","On"},    {"Off","On"},   {}},
        {setScreenFunc, buttonModeFunc, consoleOutputFunc,                  biosEnableFunc, nifiEnableFunc, saveSettingsFunc},
        {0,             0,               2,                                 1,              0,              0}
    },
    {
        "Debug",
        6,
        {2,2,2,2,0,0},
        {"Wait for Vblank", "Hblank", "Window", "Sound", "Advance Frame", "ROM Info" },
        {{"Off", "On"}, {"Off", "On"}, {"Off", "On"}, {"Off", "On"}, {}, {}},
        {vblankWaitFunc, hblankEnableFunc, windowEnableFunc, soundEnableFunc, advanceFrameFunc, romInfoFunc},
        {0,1,1,1,0,0}
    },
    {
        "Sound Channels",
        4,
        {2,2,2,2},
        {"Channel 1", "Channel 2", "Channel 3", "Channel 4"},
        {{"Off", "On"}, {"Off", "On"}, {"Off", "On"}, {"Off", "On"}},
        {chan1Func, chan2Func, chan3Func, chan4Func},
        {1,1,1,1}
    }
};
const int numMenus = sizeof(menuList)/sizeof(ConsoleSubMenu);

void initConsole() {
    for (int i=0; i<numMenus; i++) {
        for (int j=0; j<menuList[i].numOptions; j++) {
            menuList[i].optionSelections[j] = menuList[i].defaultOptionSelections[j];
            // check "Wait for VBlank" which for some reason causes issues on dsi's.
            if (__dsimode && i == 1 && j == 0)
                menuList[i].optionSelections[j] = 1;
            if (menuList[i].numSelections[j] != 0) {
                menuList[i].optionFunctions[j](menuList[i].defaultOptionSelections[j]);
            }
        }
    }
}

// Message will be printed immediately, but also stored in case it's overwritten 
// right away.
void printConsoleMessage(char* s) {
    strncpy(printMessage, s, 33);

    int newlines = 23-(menuList[menu].numOptions*2+2)-1;
    for (int i=0; i<newlines; i++)
        printf("\n");
    int spaces = 31-strlen(printMessage);
    for (int i=0; i<spaces; i++)
        printf(" ");
    printf("%s\n", printMessage);
}

void enterConsole() {
    if (!consoleOn)
        advanceFrame = true;
}
void exitConsole() {
    quitConsole = true;
}
bool isConsoleEnabled() {
    return consoleOn;
}

int displayConsole() {
    advanceFrame = 0;
    displayConsoleRetval=0;
    consoleOn = true;

    quitConsole = false;
    soundDisable();

    while (!quitConsole) {
        consoleClear();
        int nameStart = (32-strlen(menuList[menu].name)-2)/2;
        if (option == -1)
            nameStart-=2;
        for (int i=0; i<nameStart; i++)
            printf(" ");
        if (option == -1)
            printf("* ");
        printf("[");
        printf(menuList[menu].name);
        printf("]");
        if (option == -1)
            printf(" *");
        printf("\n\n");

        for (int i=0; i<menuList[menu].numOptions; i++) {
            if (menuList[menu].numSelections[i] == 0) {
                for (int j=0; j<(32-strlen(menuList[menu].options[i]))/2-2; j++)
                    printf(" ");
                if (i == option)
                    printf("* %s *\n\n", menuList[menu].options[i]);
                else
                    printf("  %s  \n\n", menuList[menu].options[i]);
            }
            else {
                for (int j=0; j<18-strlen(menuList[menu].options[i]); j++)
                    printf(" ");
                printf("%s ", menuList[menu].options[i]);
                if (i == option)
                    printf("* ");
                else
                    printf("  ");
                printf("%s", menuList[menu].optionValues[i][menuList[menu].optionSelections[i]]);
                if (i == option)
                    printf(" *");
                printf("\n\n");
            }
        }

        if (printMessage[0] != '\0') {
            int newlines = 23-(menuList[menu].numOptions*2+2)-1;
            for (int i=0; i<newlines; i++)
                printf("\n");
            int spaces = 31-strlen(printMessage);
            for (int i=0; i<spaces; i++)
                printf(" ");
            printf("%s\n", printMessage);

            printMessage[0] = '\0';
        }

        // get input
        while (!quitConsole) {
            swiWaitForVBlank();
            readKeys();

            if (keyPressedAutoRepeat(KEY_UP)) {
                option--;
                if (option < -1)
                    option = menuList[menu].numOptions-1;
                break;
            }
            else if (keyPressedAutoRepeat(KEY_DOWN)) {
                option++;
                if (option >= menuList[menu].numOptions)
                    option = -1;
                break;
            }
            else if (keyPressedAutoRepeat(KEY_LEFT)) {
                if (option == -1) {
                    menu--;
                    if (menu < 0)
                        menu = numMenus-1;
                    break;
                }
                else if (menuList[menu].optionValues[option][0] != 0) {
                    int selection = menuList[menu].optionSelections[option]-1;
                    if (selection < 0)
                        selection = menuList[menu].numSelections[option]-1;
                    menuList[menu].optionSelections[option] = selection;
                    menuList[menu].optionFunctions[option](selection);
                    break;
                }
            }
            else if (keyPressedAutoRepeat(KEY_RIGHT)) {
                if (option == -1) {
                    menu++;
                    if (menu >= numMenus)
                        menu = 0;
                    break;
                }
                else if (menuList[menu].optionValues[option][0] != 0) {
                    int selection = menuList[menu].optionSelections[option]+1;
                    if (selection >= menuList[menu].numSelections[option])
                        selection = 0;
                    menuList[menu].optionSelections[option] = selection;
                    menuList[menu].optionFunctions[option](selection);
                    break;
                }
            }
            else if (keyJustPressed(KEY_A)) {
                if (option >= 0 && menuList[menu].numSelections[option] == 0) {
                    menuList[menu].optionFunctions[option](menuList[menu].optionSelections[option]);
                    forceReleaseKey(KEY_A);
                    break;
                }
            }
            else if (keyJustPressed(KEY_B)) {
                forceReleaseKey(KEY_B);
                goto end;
            }
            else if (keyJustPressed(KEY_L)) {
                menu--;
                if (menu < 0)
                    menu = numMenus-1;
                if (option >= menuList[menu].numOptions)
                    option = menuList[menu].numOptions-1;
                break;
            }
            else if (keyJustPressed(KEY_R)) {
                menu++;
                if (menu >= numMenus)
                    menu = 0;
                if (option >= menuList[menu].numOptions)
                    option = menuList[menu].numOptions-1;
                break;
            }
        }
    }
end:
    if (!soundDisabled)
        soundEnable();
    consoleClear();
    consoleOn = false;
    return displayConsoleRetval;
}

void consoleParseConfig(const char* option, const char* value) {
    int val = atoi(value);
    for (int i=0; i<numMenus; i++) {
        for (int j=0; j<menuList[i].numOptions; j++) {
            if (strcmpi(menuList[i].options[j], option) == 0) {
                menuList[i].optionSelections[j] = val;
                menuList[i].optionFunctions[j](val);
            }
        }
    }
}

void consolePrintConfig(FILE* file) {
    for (int i=0; i<numMenus; i++) {
        for (int j=0; j<menuList[i].numOptions; j++) {
            if (menuList[i].numSelections[j] != 0)
                fprintf(file, "%s=%d\n", menuList[i].options[j], menuList[i].optionSelections[j]);
        }
    }
}

void addToLog(const char* format, va_list args) {
    if (consoleDebugOutput)
        vprintf(format, args);
}

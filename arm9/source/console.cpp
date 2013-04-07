#include <nds.h>
#include "console.h"
#include "inputhelper.h"
#include "gbsnd.h"
#include "main.h"
#include "gameboy.h"
#include "mmu.h"
#include "nifi.h"
#include "cheats.h"

const int screenTileWidth = 32;
bool consoleDebugOutput = false;
bool quitConsole = false;
bool consoleOn = false;
int displayConsoleRetval=0;
int menu=0;
int option = -1;
char printMessage[33];
int consoleScreenBacklight;
int stateNum=0;

int selectedGameboyMode=0;

extern int interruptWaitMode;
extern bool windowDisabled;
extern bool hblankDisabled;
extern int frameskip;
extern int halt;

extern bool __dsimode;

void selectRomFunc() {
    char* filename = startFileChooser();
    loadProgram(filename);
    free(filename);
    initializeGameboy();
    quitConsole = true;
    displayConsoleRetval = 1;
}

void suspendFunc(int value) {
    printConsoleMessage("Suspending...");
    saveGame();
    saveState(-1);
    printMessage[0] = '\0';
    selectRomFunc();
}
void exitFunc(int value) {
    printConsoleMessage("Saving...");
    saveGame();
    printMessage[0] = '\0';
    selectRomFunc();
}
void fastForwardFunc(int value) {
    fastForwardMode = (value == 1);
}
void setScreenFunc(int value) {
    if (value) {
        lcdMainOnBottom();
        consoleScreenBacklight = PM_BACKLIGHT_TOP;
    }
    else {
        lcdMainOnTop();
        consoleScreenBacklight = PM_BACKLIGHT_BOTTOM;
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
void returnToLauncherFunc(int value) {
    exit(0);
}

void nifiEnableFunc(int value) {
    if (value)
        enableNifi();
    else
        disableNifi();
}

void cheatFunc(int value) {
    startCheatMenu();
}
void keyConfigFunc(int value) {
    startKeyConfigChooser();
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
        displayConsoleRetval = 1;
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

void gameboyModeFunc(int value) {
    selectedGameboyMode = value;
}

void biosEnableFunc(int value) {
    biosEnabled = value;
}


void vblankWaitFunc(int value) {
    // For SOME REASON it crashes in dsi mode when it's zero.
    if (__dsimode)
        interruptWaitMode = 1;
    else
        interruptWaitMode = value;
}
void hblankEnableFunc(int value) {
    hblankDisabled = !value;
    if (value)
        irqEnable(IRQ_HBLANK);
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

void setRumbleFunc(int value) {
    /* The strenght settings are "low","mid","hi","off" */
    rumbleEnabled = (bool)(value < 3);

    /* This code is used to put the  EZ3in1 in rumble mode,
     * writing 0x8 disables it, while the rumble strength
     * is set by writing (0xF0 + setting).
     * It should work as a regular rumble pack. */
    sysSetCartOwner(BUS_OWNER_ARM9);

    GBA_BUS[0x1FE0000/2] = 0xd200;
    GBA_BUS[0x0000000/2] = 0x1500;
    GBA_BUS[0x0020000/2] = 0xd200;
    GBA_BUS[0x0040000/2] = 0x1500;
    GBA_BUS[0x1E20000/2] = (rumbleEnabled) ? (0xF0 + value) : 0x08;
    GBA_BUS[0x1FC0000/2] = 0x1500;

    RUMBLE_PAK = 0x02;
}

struct MenuOption {
    const char* name;
    void (*function)(int);
    int numValues;
    const char* values[10];
    int defaultSelection;
    int selection;
};
struct ConsoleSubMenu {
    char *name;
    int numOptions;
    MenuOption options[10];
};

ConsoleSubMenu menuList[] = {
    {
        "Options",
        7,
        {
            {"Suspend", suspendFunc, 0, {}, 0},
            {"State Slot", stateSelectFunc, 10, {"0","1","2","3","4","5","6","7","8","9"}, 0},
            {"Save State", stateSaveFunc, 0, {}, 0},
            {"Load State", stateLoadFunc, 0, {}, 0},
            {"Reset", resetFunc, 0, {}, 0},
            {"Save & Exit", exitFunc, 0, {}, 0},
            {"Quit to Launcher/Reboot", returnToLauncherFunc, 0, {}, 0}
        }
    },
    {
        "Settings",
        8,
        {
            {"Key Config", keyConfigFunc, 0, {}, 0},
            {"Manage Cheats", cheatFunc, 0, {}, 0},
            {"Fast Forward", fastForwardFunc, 2, {"Off","On"}, 0},
            {"Game Screen", setScreenFunc, 2, {"Top","Bottom"}, 0},
            {"Console Output", consoleOutputFunc, 4, {"Off","Time","FPS+Time","Debug"}, 0},
            {"NiFi", nifiEnableFunc, 2, {"Off","On"}, 0},
            {"Rumble Pak", setRumbleFunc, 4, {"Low","Mid","High","Off"}, 1},
            {"Save Settings", saveSettingsFunc, 0, {}, 0}
        }
    },
    {
        "GB Mode",
        2,
        {
            {"GBC Mode", gameboyModeFunc, 4, {"Off","If Needed","On GBC","On GBA"}, 2},
            {"GBC Bios", biosEnableFunc, 2, {"Off","On"}, 1}
        }
    },
    {
        "Debug",
        6,
        {
            {"Wait for Vblank", vblankWaitFunc, 2, {"Off","On"}, 0},
            {"Hblank", hblankEnableFunc, 2, {"Off","On"}, 1},
            {"Window", windowEnableFunc, 2, {"Off","On"}, 1},
            {"Sound", soundEnableFunc, 2, {"Off","On"}, 1},
            {"Advance Frame", advanceFrameFunc, 0, {}, 0},
            {"ROM Info", romInfoFunc, 0, {}, 0}
        }
    },
    {
        "Sound Channels",
        4,
        {
            {"Channel 1", chan1Func, 2, {"Off","On"}, 1},
            {"Channel 2", chan2Func, 2, {"Off","On"}, 1},
            {"Channel 3", chan3Func, 2, {"Off","On"}, 1},
            {"Channel 4", chan4Func, 2, {"Off","On"}, 1}
        }
    }
};
const int numMenus = sizeof(menuList)/sizeof(ConsoleSubMenu);

void initConsole() {
    for (int i=0; i<numMenus; i++) {
        for (int j=0; j<menuList[i].numOptions; j++) {
            menuList[i].options[j].selection = menuList[i].options[j].defaultSelection;
            if (menuList[i].options[j].numValues != 0) {
                menuList[i].options[j].function(menuList[i].options[j].defaultSelection);
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
    powerOn(consoleScreenBacklight);

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
            if (menuList[menu].options[i].numValues == 0) {
                for (int j=0; j<(32-strlen(menuList[menu].options[i].name))/2-2; j++)
                    printf(" ");
                if (i == option)
                    printf("* %s *\n\n", menuList[menu].options[i].name);
                else
                    printf("  %s  \n\n", menuList[menu].options[i].name);
            }
            else {
                for (int j=0; j<17-strlen(menuList[menu].options[i].name); j++)
                    printf(" ");
                printf("%s ", menuList[menu].options[i].name);
                if (i == option)
                    printf("* ");
                else
                    printf("  ");
                printf("%s", menuList[menu].options[i].values[menuList[menu].options[i].selection]);
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
                else if (menuList[menu].options[option].numValues != 0) {
                    int selection = menuList[menu].options[option].selection-1;
                    if (selection < 0)
                        selection = menuList[menu].options[option].numValues-1;
                    menuList[menu].options[option].selection = selection;
                    menuList[menu].options[option].function(selection);
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
                else if (menuList[menu].options[option].numValues != 0) {
                    int selection = menuList[menu].options[option].selection+1;
                    if (selection >= menuList[menu].options[option].numValues)
                        selection = 0;
                    menuList[menu].options[option].selection = selection;
                    menuList[menu].options[option].function(selection);
                    break;
                }
            }
            else if (keyJustPressed(KEY_A)) {
                if (option >= 0 && menuList[menu].options[option].numValues == 0) {
                    menuList[menu].options[option].function(menuList[menu].options[option].selection);
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

    updateConsoleScreen();

    return displayConsoleRetval;
}

void updateConsoleScreen() {
    if (!(fpsOutput || timeOutput || consoleDebugOutput))
        powerOff(consoleScreenBacklight);
}

void consoleParseConfig(const char* line) {
    char* equalsPos = strchr(line, '=');
    if (equalsPos == 0)
        return;
    *equalsPos = '\0';
    const char* option = line;
    const char* value = equalsPos+1;
    int val = atoi(value);
    for (int i=0; i<numMenus; i++) {
        for (int j=0; j<menuList[i].numOptions; j++) {
            if (strcmpi(menuList[i].options[j].name, option) == 0 && menuList[i].options[j].numValues != 0) {
                menuList[i].options[j].selection = val;
                menuList[i].options[j].function(val);
            }
        }
    }
}

void consolePrintConfig(FILE* file) {
    for (int i=0; i<numMenus; i++) {
        for (int j=0; j<menuList[i].numOptions; j++) {
            if (menuList[i].options[j].numValues != 0)
                fprintf(file, "%s=%d\n", menuList[i].options[j].name, menuList[i].options[j].selection);
        }
    }
}

void addToLog(const char* format, va_list args) {
    if (consoleDebugOutput)
        vprintf(format, args);
}

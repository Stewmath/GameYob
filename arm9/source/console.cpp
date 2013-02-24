#include <nds.h>
#include "console.h"
#include "inputhelper.h"
#include "gbsnd.h"
#include "main.h"
#include "gameboy.h"
#include "mmu.h"

const int screenTileWidth = 32;
bool consoleDebugOutput = false;
bool quitConsole = false;
int displayConsoleRetval=0;

extern int interruptWaitMode;
extern bool advanceFrame;
extern bool windowDisabled;
extern bool hblankDisabled;
extern bool soundDisabled;
extern int frameskip;
extern int halt;

void selectRomFunc(int value) {
    saveGame();
    loadProgram(startFileChooser());
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
        consoleDebugOutput = false;
    }
    else if (value == 1) {
        fpsOutput = true;
        consoleDebugOutput = false;
    }
    else if (value == 2) {
        fpsOutput = false;
        consoleDebugOutput = true;
    }
}

void biosEnableFunc(int value) {
    if (biosExists)
        biosEnabled = value;
    else
        biosEnabled = 0;
}

void frameskipFunc(int value) {
    frameskip = value;
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
void logViewFunc(int value) {

}
void resetFunc(int value) {
    initializeGameboy();
    quitConsole = true;
    displayConsoleRetval = 1;
}
void returnFunc(int value) {
    quitConsole = true;
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



struct ConsoleSubMenu {
    char *name;
    int numOptions;
    int numSelections[10];
    char *options[10];
    char *optionValues[10][5];
    void (*optionFunctions[10])(int);
    int defaultOptionSelections[10];
    int optionSelections[10];
};

ConsoleSubMenu menuList[] = { 
    {
        "Options",
        7,
        {0,2,2,3,2,0,0},
        {"Load ROM", "Game Screen", "A & B Buttons", "Console Output", "GBC Bios", "Reset", "Return to game"},
        {{},{"Top","Bottom"},{"A/B", "B/Y"},{"Off","FPS","Debug"},{"Off","On"},{},{}},
        {selectRomFunc, setScreenFunc, buttonModeFunc, consoleOutputFunc, biosEnableFunc, resetFunc, returnFunc},
        {0,0,0,1,1,0,0}
    },
    {
        "Debug",
        5,
        {2,2,2,2,0},
        {"Wait for Vblank", "Hblank", "Window", "Sound", "Advance Frame" },
        {{"Off", "On"}, {"Off", "On"}, {"Off", "On"}, {"Off", "On"}, {}},
        {vblankWaitFunc, hblankEnableFunc, windowEnableFunc, soundEnableFunc, advanceFrameFunc},
        {0,1,1,1,0}
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
int numMenus = sizeof(menuList)/sizeof(ConsoleSubMenu);

void initConsole() {
    for (int i=0; i<numMenus; i++) {
        for (int j=0; j<menuList[i].numOptions; j++) {
            menuList[i].optionSelections[j] = menuList[i].defaultOptionSelections[j];
            if (menuList[i].numSelections[j] != 0) {
                menuList[i].optionFunctions[j](menuList[i].defaultOptionSelections[j]);
            }
        }
    }
}

int displayConsole() {
    static int menu=0;
    static int option = -1;

    displayConsoleRetval=0;

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
                for (int j=0; j<19-strlen(menuList[menu].options[i]); j++)
                    printf(" ");
                printf("%s  ", menuList[menu].options[i]);
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

        // get input
        while (true) {
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
                else if (menuList[menu].optionValues[option][0] != 0 && menuList[menu].optionSelections[option] > 0) {
                    menuList[menu].optionSelections[option]--;
                    menuList[menu].optionFunctions[option](menuList[menu].optionSelections[option]);
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
                else if (menuList[menu].optionValues[option][0] != 0 && menuList[menu].optionSelections[option] < menuList[menu].numSelections[option]-1) {
                    menuList[menu].optionSelections[option]++;
                    menuList[menu].optionFunctions[option](menuList[menu].optionSelections[option]);
                    break;
                }
            }
            else if (keyPressedAutoRepeat(KEY_A)) {
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
    return displayConsoleRetval;
}

void addToLog(const char *format, va_list args) {
    if (consoleDebugOutput)
        vprintf(format, args);
}

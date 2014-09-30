#ifdef DS
#include "common.h"
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "menu.h"
#include "gbprinter.h"
#include "console.h"
#include "inputhelper.h"
#include "filechooser.h"
#include "soundengine.h"
#include "main.h"
#include "gameboy.h"
#include "nifi.h"
#include "cheats.h"
#include "gbgfx.h"
#include "gbs.h"
#include "gbmanager.h"


void printVersionInfo(); // Defined in version.cpp

void subMenuGenericUpdateFunc(); // Private function here


bool consoleDebugOutput = false;
bool menuOn = false;
bool consoleInitialized = false;
int menu=0;
int option = -1;
char printMessage[33];
int gameScreen;
int singleScreenMode;
int stateNum=0;

bool windowDisabled;
bool hblankDisabled;

PrintConsole* menuConsole;


int gbcModeOption;
bool gbaModeOption;
int sgbModeOption;

bool soundDisabled;
bool hyperSound;

bool customBordersEnabled;
bool sgbBordersEnabled;
bool autoSavingEnabled;

bool printerEnabled;

// how/when the bios should be used
int biosEnabled;

void (*subMenuUpdateFunc)();

bool fpsOutput;
bool timeOutput;

int rumbleStrength;

extern int interruptWaitMode;
extern bool windowDisabled;
extern bool hblankDisabled;
extern int halt;

extern int rumbleInserted;


// Private function used for simple submenus
void subMenuGenericUpdateFunc() {
    if (keyJustPressed(mapMenuKey(MENU_KEY_A)) || keyJustPressed(mapMenuKey(MENU_KEY_B)))
        closeSubMenu();
}

// Functions corresponding to menu options

void suspendFunc(int value) {
    muteSND();
    if (!autoSavingEnabled) {
        printMenuMessage("Saving SRAM...");
        mgr_save();
    }
    printMenuMessage("Saving state...");
    gameboy->saveState(-1);
    printMessage[0] = '\0';
    closeMenu();
    mgr_selectRom();
}
void exitFunc(int value) {
    muteSND();
    if (!autoSavingEnabled && !gbsMode) {
        printMenuMessage("Saving SRAM...");
        mgr_save();
    }
    printMessage[0] = '\0';
    closeMenu();
    mgr_selectRom();
}
void exitNoSaveFunc(int value) {
    muteSND();
    closeMenu();
    mgr_selectRom();
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

void printerEnableFunc(int value) {
    if (value) {
        initGbPrinter();
    }
    printerEnabled = value;
}

void cheatFunc(int value) {
    if (!startCheatMenu())
        printMenuMessage("No cheats found!");
}
void keyConfigFunc(int value) {
    startKeyConfigChooser();
}

void saveSettingsFunc(int value) {
    printMenuMessage("Saving settings...");
    muteSND();
    writeConfigFile();
    if (!gameboy->isGameboyPaused())
        unmuteSND();
    printMenuMessage("Settings saved.");
}

void stateSelectFunc(int value) {
    stateNum = value;
    if (gameboy->checkStateExists(stateNum)) {
        enableMenuOption("Load State");
        enableMenuOption("Delete State");
    }
    else {
        disableMenuOption("Load State");
        disableMenuOption("Delete State");
    }
}
void stateSaveFunc(int value) {
    printMenuMessage("Saving state...");
    muteSND();
    gameboy->saveState(stateNum);
    if (!gameboy->isGameboyPaused())
        unmuteSND();
    printMenuMessage("State saved.");
    // Will activate the other state options
    stateSelectFunc(stateNum);
}
void stateLoadFunc(int value) {
    printMenuMessage("Loading state...");
    muteSND();
    if (gameboy->loadState(stateNum) == 0) {
        closeMenu();
        updateScreens();
        printMessage[0] = '\0';
    }
}
void stateDeleteFunc(int value) {
    muteSND();
    gameboy->deleteState(stateNum);
    // Will grey out the other state options
    stateSelectFunc(stateNum);
    if (!gameboy->isGameboyPaused())
        unmuteSND();
}
void resetFunc(int value) {
    closeMenu();
    updateScreens();
    gameboy->init();
}
void returnFunc(int value) {
    closeMenu();
    updateScreens();
}

void gameboyModeFunc(int value) {
    gbcModeOption = value;
}

void gbaModeFunc(int value) {
    gbaModeOption = value;
}

void sgbModeFunc(int value) {
    sgbModeOption = value;
}

void biosEnableFunc(int value) {
    biosEnabled = value;
}

void setScreenFunc(int value) {
    gameScreen = value;
    updateScreens();
}

void setSingleScreenFunc(int value) {
    if (value != singleScreenMode) {
        singleScreenMode = value;
        if (singleScreenMode)
            gameboy->pause();

        if (isMenuOn()) {
            // Swap game screen
            // This will invoke updateScreens, incidentally.
            setMenuOption("Game Screen", !gameScreen);
        }
    }
}

void setScaleModeFunc(int value) {
    scaleMode = value;
    if (!isMenuOn()) {
        updateScreens();
    }
    if (value == 0) {
        doAtVBlank(checkBorder);
        enableMenuOption("Console Output");
    }
    else {
        disableMenuOption("Console Output");
    }
}
void setScaleFilterFunc(int value) {
    scaleFilter = value;
}

void customBorderEnableFunc(int value) {
    customBordersEnabled = value;
    checkBorder();
}

void sgbBorderEnableFunc(int value) {
    sgbBordersEnabled = value;
    checkBorder();
}

void vblankWaitFunc(int value) {
    interruptWaitMode = value;
}
void hblankEnableFunc(int value) {
    hblankDisabled = !value;
}
void windowEnableFunc(int value) {
    windowDisabled = !value;
#ifdef DS
    if (windowDisabled)
        REG_DISPCNT &= ~DISPLAY_WIN0_ON;
    else
        REG_DISPCNT |= DISPLAY_WIN0_ON;
#endif
}
void soundEnableFunc(int value) {
    soundDisabled = !value;
#ifdef DS
    sharedData->fifosSent++;
    fifoSendValue32(FIFO_USER_01, GBSND_MUTE_COMMAND<<20);
#endif
}
void romInfoFunc(int value) {
    displaySubMenu(subMenuGenericUpdateFunc);
    gameboy->printRomInfo();
}
void versionInfoFunc(int value) {
    displaySubMenu(subMenuGenericUpdateFunc);
    printVersionInfo();
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

void setRumbleFunc(int value) {
    rumbleStrength = value;

    rumbleInserted = checkRumble();
}

void hyperSoundFunc(int value) {
    hyperSound = value;
#ifdef DS
    sharedData->hyperSound = value;
    sharedData->fifosSent++;
    fifoSendValue32(FIFO_USER_01, GBSND_HYPERSOUND_ENABLE_COMMAND<<20 | hyperSound);
#endif
}

void setAutoSaveFunc(int value) {
    muteSND();
    if (autoSavingEnabled)
        gameboy->gameboySyncAutosave();
    else
        gameboy->saveGame(); // Synchronizes save file with filesystem
    autoSavingEnabled = value;
    if (gameboy->isRomLoaded() && gameboy->getNumRamBanks() && !gbsMode && !autoSavingEnabled)
        enableMenuOption("Exit without saving");
    else
        disableMenuOption("Exit without saving");
    if (!gameboy->isGameboyPaused())
        unmuteSND();
}

struct MenuOption {
    const char* name;
    void (*function)(int);
    int numValues;
    const char* values[10];
    int defaultSelection;
    bool enabled;
    int selection;
};
struct ConsoleSubMenu {
    const char *name;
    int numOptions;
    MenuOption options[10];
};


ConsoleSubMenu menuList[] = {
    {
        "ROM",
        9,
        {
            {"Exit", exitFunc, 0, {}, 0},
            {"Reset", resetFunc, 0, {}, 0},
            {"State Slot", stateSelectFunc, 10, {"0","1","2","3","4","5","6","7","8","9"}, 0},
            {"Save State", stateSaveFunc, 0, {}, 0},
            {"Load State", stateLoadFunc, 0, {}, 0},
            {"Delete State", stateDeleteFunc, 0, {}, 0},
            {"Quit to Launcher", returnToLauncherFunc, 0, {}, 0},
            {"Exit without saving", exitNoSaveFunc, 0, {}, 0},
            {"Suspend", suspendFunc, 0, {}, 0}
        }
    },
    {
        "Settings",
        7,
        {
            {"Key Config", keyConfigFunc, 0, {}, 0},
            {"Manage Cheats", cheatFunc, 0, {}, 0},
            {"Rumble Pak", setRumbleFunc, 4, {"Off","Low","Mid","High"}, 2},
            {"Console Output", consoleOutputFunc, 4, {"Off","Time","FPS+Time","Debug"}, 0},
            {"GB Printer", printerEnableFunc, 2, {"Off","On"}, 1},
            {"Autosaving", setAutoSaveFunc, 1, {"Off","On"}, 1}, // Autosaving disabled for now
            {"Save Settings", saveSettingsFunc, 0, {}, 0}
        }
    },
    {
        "Display",
        7,
        {
            {"Game Screen", setScreenFunc, 2, {"Top","Bottom"}, 0},
            {"Single Screen", setSingleScreenFunc, 2, {"Off","On"}, 0},
            {"Scaling", setScaleModeFunc, 3, {"Off","Aspect","Full"}, 0},
            {"Scale Filter", setScaleFilterFunc, 2, {"Off","On"}, 1},
            {"SGB Borders", sgbBorderEnableFunc, 2, {"Off","On"}, 1},
            {"Custom Border", customBorderEnableFunc, 2, {"Off","On"}, 1},
            {"Select Border", (void (*)(int))selectBorder, 0, {}, 0},
        }
    },
    {
        "GB Modes",
        4,
        {
            {"GBC Bios", biosEnableFunc, 3, {"Off","GB Only","On"}, 1},
            {"Detect GBA", gbaModeFunc, 2, {"Off","On"}, 0},
            {"GBC Mode", gameboyModeFunc, 3, {"Off","If Needed","On"}, 2},
            {"SGB Mode", sgbModeFunc, 3, {"Off","Prefer GBC","Prefer SGB"}, 1}
        }
    },
    {
        "Debug",
        7,
        {
            {"Wait for Vblank", vblankWaitFunc, 2, {"Off","On"}, 0},
            {"Hblank", hblankEnableFunc, 2, {"Off","On"}, 1},
            {"Window", windowEnableFunc, 2, {"Off","On"}, 1},
            {"Sound", soundEnableFunc, 2, {"Off","On"}, 1},
            {"Sound Timing Fix", hyperSoundFunc, 2, {"Off","On"}, 1},
            {"ROM Info", romInfoFunc, 0, {}, 0},
            {"Version Info", versionInfoFunc, 0, {}, 0}
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
    },
#ifdef NIFI
    {
        "Linking",
        2,
        {
            {"Link to DS", (void (*)(int))nifiInterLinkMenu, 0, {}, 0},
            {"Swap Focus", (void (*)(int))mgr_swapFocus, 0, {}, 0}
        }
    }
#endif
};
const int numMenus = sizeof(menuList)/sizeof(ConsoleSubMenu);

void setMenuDefaults() {
    for (int i=0; i<numMenus; i++) {
        for (int j=0; j<menuList[i].numOptions; j++) {
            menuList[i].options[j].selection = menuList[i].options[j].defaultSelection;
            menuList[i].options[j].enabled = true;
            if (menuList[i].options[j].numValues != 0) {
                menuList[i].options[j].function(menuList[i].options[j].defaultSelection);
            }
        }
    }

#ifdef DS
    menuConsole = (PrintConsole*)malloc(sizeof(PrintConsole));
    memcpy(menuConsole, getDefaultConsole(), sizeof(PrintConsole));
#endif
}

void displayMenu() {
    menuOn = true;
    if (checkRumble())
        enableMenuOption("Rumble Pak");
    else
        disableMenuOption("Rumble Pak");

    updateScreens();
    doAtVBlank(redrawMenu);
}
void closeMenu() {
    menuOn = false;
    setPrintConsole(menuConsole);
    clearConsole();
    gameboy->unpause();
}

bool isMenuOn() {
    return menuOn;
}

void redrawMenu() {
    PrintConsole* oldConsole = getPrintConsole();
    setPrintConsole(menuConsole);
    clearConsole();

    // Top line: submenu
    int pos=0;
    int nameStart = (32-strlen(menuList[menu].name)-2)/2;
    if (option == -1) {
        nameStart-=2;
        iprintfColored(CONSOLE_COLOR_LIGHT_GREEN, "<");
    }
    else
        printf("<");
    pos++;
    for (; pos<nameStart; pos++)
        printf(" ");
    if (option == -1) {
        iprintfColored(CONSOLE_COLOR_LIGHT_YELLOW, "* ");
        pos += 2;
    }
    {
        int color = (option == -1 ? CONSOLE_COLOR_LIGHT_YELLOW : CONSOLE_COLOR_WHITE);
        iprintfColored(color, "[%s]", menuList[menu].name);
    }
    pos += 2 + strlen(menuList[menu].name);
    if (option == -1) {
        iprintfColored(CONSOLE_COLOR_LIGHT_YELLOW, " *");
        pos += 2;
    }
    for (; pos < 31; pos++)
        printf(" ");
    if (option == -1)
        iprintfColored(CONSOLE_COLOR_LIGHT_GREEN, ">");
    else
        printf(">");
    printf("\n");

    // Rest of the lines: options
    for (int i=0; i<menuList[menu].numOptions; i++) {
        int option_color;
        if (!menuList[menu].options[i].enabled)
            option_color = CONSOLE_COLOR_GREY;
        else if (option == i)
            option_color = CONSOLE_COLOR_LIGHT_YELLOW;
        else
            option_color = CONSOLE_COLOR_WHITE;

        if (menuList[menu].options[i].numValues == 0) {
            for (unsigned int j=0; j<(32-strlen(menuList[menu].options[i].name))/2-2; j++)
                printf(" ");
            if (i == option) {
                iprintfColored(option_color, "* %s *\n\n", menuList[menu].options[i].name);
            }
            else
                iprintfColored(option_color, "  %s  \n\n", menuList[menu].options[i].name);
        }
        else {
            for (unsigned int j=0; j<16-strlen(menuList[menu].options[i].name); j++)
                printf(" ");
            if (i == option) {
                iprintfColored(option_color, "* ");
                iprintfColored(option_color, "%s  ", menuList[menu].options[i].name);
                iprintfColored(menuList[menu].options[i].enabled ? CONSOLE_COLOR_LIGHT_GREEN : option_color,
                        "%s", menuList[menu].options[i].values[menuList[menu].options[i].selection]);
                iprintfColored(option_color, " *");
            }
            else {
                printf("  ");
                iprintfColored(option_color, "%s  ", menuList[menu].options[i].name);
                iprintfColored(option_color, "%s", menuList[menu].options[i].values[menuList[menu].options[i].selection]);
            }
            printf("\n\n");
        }
    }

    // Message at the bottom
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

    setPrintConsole(oldConsole);
}

// Called each vblank while the menu is on
void updateMenu() {
    if (!isMenuOn())
        return;

    if (subMenuUpdateFunc != 0) {
        subMenuUpdateFunc();
        return;
    }

    bool redraw = false;
    // Get input
    if (keyPressedAutoRepeat(mapMenuKey(MENU_KEY_UP))) {
        option--;
        if (option < -1)
            option = menuList[menu].numOptions-1;
        redraw = true;
    }
    else if (keyPressedAutoRepeat(mapMenuKey(MENU_KEY_DOWN))) {
        option++;
        if (option >= menuList[menu].numOptions)
            option = -1;
        redraw = true;
    }
    else if (keyPressedAutoRepeat(mapMenuKey(MENU_KEY_LEFT))) {
        if (option == -1) {
            menu--;
            if (menu < 0)
                menu = numMenus-1;
        }
        else if (menuList[menu].options[option].numValues != 0 && menuList[menu].options[option].enabled) {
            int selection = menuList[menu].options[option].selection-1;
            if (selection < 0)
                selection = menuList[menu].options[option].numValues-1;
            menuList[menu].options[option].selection = selection;
            menuList[menu].options[option].function(selection);
        }
        redraw = true;
    }
    else if (keyPressedAutoRepeat(mapMenuKey(MENU_KEY_RIGHT))) {
        if (option == -1) {
            menu++;
            if (menu >= numMenus)
                menu = 0;
        }
        else if (menuList[menu].options[option].numValues != 0 && menuList[menu].options[option].enabled) {
            int selection = menuList[menu].options[option].selection+1;
            if (selection >= menuList[menu].options[option].numValues)
                selection = 0;
            menuList[menu].options[option].selection = selection;
            menuList[menu].options[option].function(selection);
        }
        redraw = true;
    }
    else if (keyJustPressed(mapMenuKey(MENU_KEY_A))) {
        forceReleaseKey(mapMenuKey(MENU_KEY_A));
        if (option >= 0 && menuList[menu].options[option].numValues == 0 && menuList[menu].options[option].enabled) {
            menuList[menu].options[option].function(menuList[menu].options[option].selection);
        }
        redraw = true;
    }
    else if (keyJustPressed(mapMenuKey(MENU_KEY_B))) {
        forceReleaseKey(mapMenuKey(MENU_KEY_B));
        closeMenu();
        updateScreens();
    }
    else if (keyJustPressed(mapMenuKey(MENU_KEY_L))) {
        menu--;
        if (menu < 0)
            menu = numMenus-1;
        if (option >= menuList[menu].numOptions)
            option = menuList[menu].numOptions-1;
        redraw = true;
    }
    else if (keyJustPressed(mapMenuKey(MENU_KEY_R))) {
        menu++;
        if (menu >= numMenus)
            menu = 0;
        if (option >= menuList[menu].numOptions)
            option = menuList[menu].numOptions-1;
        redraw = true;
    }
    if (redraw && subMenuUpdateFunc == 0 &&
            isMenuOn()) // The menu may have been closed by an option
        doAtVBlank(redrawMenu);
}

// Message will be printed immediately, but also stored in case it's overwritten 
// right away.
void printMenuMessage(const char* s) {
    bool hadPreviousMessage = printMessage[0] != '\0';
    strncpy(printMessage, s, 33);

    if (hadPreviousMessage) {
        printf("\r");
    }
    else {
        int newlines = 23-(menuList[menu].numOptions*2+2)-1;
        for (int i=0; i<newlines; i++)
            printf("\n");
    }
    int spaces = 31-strlen(printMessage);
    for (int i=0; i<spaces; i++)
        printf(" ");
    printf("%s", printMessage);
}

void displaySubMenu(void (*updateFunc)()) {
    subMenuUpdateFunc = updateFunc;
}
void closeSubMenu() {
    subMenuUpdateFunc = NULL;
    doAtVBlank(redrawMenu);
}


int getMenuOption(const char* optionName) {
    for (int i=0; i<numMenus; i++) {
        for (int j=0; j<menuList[i].numOptions; j++) {
            if (strcasecmp(optionName, menuList[i].options[j].name) == 0) {
                return menuList[i].options[j].selection;
            }
        }
    }
    return 0;
}
void setMenuOption(const char* optionName, int value) {
    for (int i=0; i<numMenus; i++) {
        for (int j=0; j<menuList[i].numOptions; j++) {
            if (strcasecmp(optionName, menuList[i].options[j].name) == 0) {
                menuList[i].options[j].selection = value;
                menuList[i].options[j].function(value);
                return;
            }
        }
    }
}
void enableMenuOption(const char* optionName) {
    for (int i=0; i<numMenus; i++) {
        for (int j=0; j<menuList[i].numOptions; j++) {
            if (strcasecmp(optionName, menuList[i].options[j].name) == 0) {
                menuList[i].options[j].enabled = true;
                return;
            }
        }
    }
}
void disableMenuOption(const char* optionName) {
    for (int i=0; i<numMenus; i++) {
        for (int j=0; j<menuList[i].numOptions; j++) {
            if (strcasecmp(optionName, menuList[i].options[j].name) == 0) {
                menuList[i].options[j].enabled = false;
                return;
            }
        }
    }
}

void menuParseConfig(const char* line) {
    char* equalsPos = (char*)strchr(line, '=');
    if (equalsPos == 0)
        return;
    *equalsPos = '\0';
    const char* option = line;
    const char* value = equalsPos+1;
    int val = atoi(value);
    for (int i=0; i<numMenus; i++) {
        for (int j=0; j<menuList[i].numOptions; j++) {
            if (strcasecmp(menuList[i].options[j].name, option) == 0 && menuList[i].options[j].numValues != 0) {
                menuList[i].options[j].selection = val;
                menuList[i].options[j].function(val);
                return;
            }
        }
    }
}

void menuPrintConfig(FileHandle* file) {
    for (int i=0; i<numMenus; i++) {
        for (int j=0; j<menuList[i].numOptions; j++) {
            if (menuList[i].options[j].numValues != 0)
                file_printf(file, "%s=%d\n", menuList[i].options[j].name, menuList[i].options[j].selection);
        }
    }
}


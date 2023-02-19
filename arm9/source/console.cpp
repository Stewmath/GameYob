#include <nds.h>
#include "gbprinter.h"
#include "console.h"
#include "inputhelper.h"
#include "filechooser.h"
#include "gbsnd.h"
#include "main.h"
#include "gameboy.h"
#include "mmu.h"
#include "nifi.h"
#include "cheats.h"
#include "gbgfx.h"
#include "gbs.h"
#include "common.h"
#include "3in1.h"


void printVersionInfo(); // Defined in version.cpp

void subMenuGenericUpdateFunc(); // Private function here


const int screenTileWidth = 32;
const int backlights[] = {PM_BACKLIGHT_TOP, PM_BACKLIGHT_BOTTOM};

bool consoleDebugOutput = false;
bool menuOn = false;
bool consoleInitialized = false;
int menu=0;
int option = -1;
char printMessage[33];
int gameScreen;
int singleScreenMode;
int stateNum=0;
PrintConsole* menuConsole;

int gbcModeOption=0;
bool gbaModeOption=false;
int sgbModeOption=false;

bool customBordersEnabled;
bool sgbBordersEnabled;
bool autoSavingEnabled;

bool printerEnabled;

volatile int consoleSelectedRow = -1;

void (*subMenuUpdateFunc)();

extern int interruptWaitMode;
extern bool windowDisabled;
extern bool hblankDisabled;
extern int halt;

extern int rumbleInserted;
extern int rumbleStrength;


// Private function used for simple submenus
void subMenuGenericUpdateFunc() {
    if (keyJustPressed(KEY_A) || keyJustPressed(KEY_B))
        closeSubMenu();
}

// Functions corresponding to menu options

void suspendFunc(int value) {
    muteSND();
    if (!autoSavingEnabled) {
        printMenuMessage("Saving SRAM...");
        saveGame();
    }
    printMenuMessage("Saving state...");
    saveState(-1);
    printMessage[0] = '\0';
    closeMenu();
    selectRom();
}
void exitFunc(int value) {
    muteSND();
    if (!autoSavingEnabled && numRamBanks && !gbsMode) {
        printMenuMessage("Saving SRAM...");
        saveGame();
    }
    printMessage[0] = '\0';
    closeMenu();
    selectRom();
}
void exitNoSaveFunc(int value) {
    muteSND();
    closeMenu();
    selectRom();
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
    if (value) {
		printMenuMessage("Warning: link emulation sucks.");
        setMenuOption("GB Printer", 0);
        enableNifi();
	}
    else
        disableNifi();
}

void printerEnableFunc(int value) {
    if (value) {
        setMenuOption("Wireless Link", 0);
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
    if (!isGameboyPaused())
        unmuteSND();
    printMenuMessage("Settings saved.");
}

void stateSelectFunc(int value) {
    stateNum = value;
    if (checkStateExists(stateNum)) {
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
    saveState(stateNum);
    if (!isGameboyPaused())
        unmuteSND();
    printMenuMessage("State saved.");
    // Will activate the other state options
    stateSelectFunc(stateNum);
}
void stateLoadFunc(int value) {
    printMenuMessage("Loading state...");
    muteSND();
    if (loadState(stateNum) == 0) {
        closeMenu();
        updateScreens();
        printMessage[0] = '\0';
    }
}
void stateDeleteFunc(int value) {
    muteSND();
    deleteState(stateNum);
    // Will grey out the other state options
    stateSelectFunc(stateNum);
    if (!isGameboyPaused())
        unmuteSND();
}
void resetFunc(int value) {
    closeMenu();
    updateScreens();
    initializeGameboy();
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
            pauseGameboy();

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
    if (windowDisabled)
        REG_DISPCNT &= ~DISPLAY_WIN0_ON;
    else
        REG_DISPCNT |= DISPLAY_WIN0_ON;
}
void soundEnableFunc(int value) {
    soundDisabled = !value;
    sharedData->fifosSent++;
    fifoSendValue32(FIFO_USER_01, GBSND_MUTE_COMMAND<<20);
}
void advanceFrameFunc(int value) {
    advanceFrame = true;
    closeMenu();
    updateScreens();
}
void romInfoFunc(int value) {
    displaySubMenu(subMenuGenericUpdateFunc);
    printRomInfo();
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

void setCamera(int value) {
    if (value > 0) {
        system_enableCamera(value);
    } else {
        system_disableCamera();
    }
}

void hyperSoundFunc(int value) {
    hyperSound = value;
    sharedData->hyperSound = value;
    sharedData->fifosSent++;
    fifoSendValue32(FIFO_USER_01, GBSND_HYPERSOUND_ENABLE_COMMAND<<20 | hyperSound);
}

void setAutoSaveFunc(int value) {
    muteSND();
    if (autoSavingEnabled)
        gameboySyncAutosave();
    else
        saveGame(); // Synchronizes save file with filesystem
    autoSavingEnabled = value;
    if (numRamBanks && !gbsMode && !autoSavingEnabled)
        enableMenuOption("Exit without saving");
    else
        disableMenuOption("Exit without saving");
    if (!isGameboyPaused())
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
        8,
        {
            {"Key Config", keyConfigFunc, 0, {}, 0},
            {"Manage Cheats", cheatFunc, 0, {}, 0},
            {"Rumble Pak", setRumbleFunc, 4, {"Off","Low","Mid","High"}, 2},
            {"GB Camera", setCamera, 3, {"Off", "Inner","Outer"}, 0},
            {"Console Output", consoleOutputFunc, 4, {"Off","Time","FPS+Time","Debug"}, 0},
            {"Wireless Link", nifiEnableFunc, 2, {"Off","On"}, 0},
            {"GB Printer", printerEnableFunc, 2, {"Off","On"}, 1},
            {"Autosaving", setAutoSaveFunc, 2, {"Off","On"}, 1},
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
            //{"Advance Frame", advanceFrameFunc, 0, {}, 0},
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
    }
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

    menuConsole = (PrintConsole*)malloc(sizeof(PrintConsole));
    memcpy(menuConsole, consoleGetDefault(), sizeof(PrintConsole));
}

void displayMenu() {
    menuOn = true;
    if (checkRumble())
        enableMenuOption("Rumble Pak");
    else
        disableMenuOption("Rumble Pak");

    if (checkCamera())
        enableMenuOption("GB Camera");
    else
        disableMenuOption("GB Camera");

    updateScreens();
    doAtVBlank(redrawMenu);
}
void closeMenu() {
    menuOn = false;
    setPrintConsole(menuConsole);
    consoleClear();
    unpauseGameboy();
}

bool isMenuOn() {
    return menuOn;
}
bool isConsoleOn() {
    return consoleInitialized;
}


void redrawMenu() {
    PrintConsole* oldConsole = getPrintConsole();
    setPrintConsole(menuConsole);
    consoleClear();

    // Top line: submenu
    int pos=0;
    int nameStart = (32-strlen(menuList[menu].name)-2)/2;
    if (option == -1) {
        nameStart-=2;
        iprintfColored(CONSOLE_COLOR_LIGHT_GREEN, "<");
    }
    else
        iprintf("<");
    pos++;
    for (; pos<nameStart; pos++)
        iprintf(" ");
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
        iprintf(" ");
    if (option == -1)
        iprintfColored(CONSOLE_COLOR_LIGHT_GREEN, ">");
    else
        iprintf(">");
    iprintf("\n");

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
                iprintf(" ");
            if (i == option) {
                iprintfColored(option_color, "* %s *\n\n", menuList[menu].options[i].name);
            }
            else
                iprintfColored(option_color, "  %s  \n\n", menuList[menu].options[i].name);
        }
        else {
            for (unsigned int j=0; j<16-strlen(menuList[menu].options[i].name); j++)
                iprintf(" ");
            if (i == option) {
                iprintfColored(option_color, "* ");
                iprintfColored(option_color, "%s  ", menuList[menu].options[i].name);
                iprintfColored(menuList[menu].options[i].enabled ? CONSOLE_COLOR_LIGHT_GREEN : option_color,
                        "%s", menuList[menu].options[i].values[menuList[menu].options[i].selection]);
                iprintfColored(option_color, " *");
            }
            else {
                iprintf("  ");
                iprintfColored(option_color, "%s  ", menuList[menu].options[i].name);
                iprintfColored(option_color, "%s", menuList[menu].options[i].values[menuList[menu].options[i].selection]);
            }
            iprintf("\n\n");
        }
    }

    // Message at the bottom
    if (printMessage[0] != '\0') {
        int newlines = 23-(menuList[menu].numOptions*2+2)-1;
        for (int i=0; i<newlines; i++)
            iprintf("\n");
        int spaces = 31-strlen(printMessage);
        for (int i=0; i<spaces; i++)
            iprintf(" ");
        iprintf("%s\n", printMessage);

        printMessage[0] = '\0';
    }

    setPrintConsole(oldConsole);
}

// Called each vblank while the menu is on
void updateMenu() {
    if (subMenuUpdateFunc != 0) {
        subMenuUpdateFunc();
        return;
    }

    bool redraw = false;
    // Get input
    if (keyPressedAutoRepeat(KEY_UP)) {
        option--;
        if (option < -1)
            option = menuList[menu].numOptions-1;
        redraw = true;
    }
    else if (keyPressedAutoRepeat(KEY_DOWN)) {
        option++;
        if (option >= menuList[menu].numOptions)
            option = -1;
        redraw = true;
    }
    else if (keyPressedAutoRepeat(KEY_LEFT)) {
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
    else if (keyPressedAutoRepeat(KEY_RIGHT)) {
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
    else if (keyJustPressed(KEY_A)) {
        forceReleaseKey(KEY_A);
        if (option >= 0 && menuList[menu].options[option].numValues == 0 && menuList[menu].options[option].enabled) {
            menuList[menu].options[option].function(menuList[menu].options[option].selection);
        }
        redraw = true;
    }
    else if (keyJustPressed(KEY_B)) {
        forceReleaseKey(KEY_B);
        closeMenu();
        updateScreens();
    }
    else if (keyJustPressed(KEY_L)) {
        menu--;
        if (menu < 0)
            menu = numMenus-1;
        if (option >= menuList[menu].numOptions)
            option = menuList[menu].numOptions-1;
        redraw = true;
    }
    else if (keyJustPressed(KEY_R)) {
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
        iprintf("\r");
    }
    else {
        int newlines = 23-(menuList[menu].numOptions*2+2)-1;
        for (int i=0; i<newlines; i++)
            iprintf("\n");
    }
    int spaces = 31-strlen(printMessage);
    for (int i=0; i<spaces; i++)
        iprintf(" ");
    iprintf("%s", printMessage);
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
                /*
                if (isMenuOn())
                    redrawMenu();
                    */
            }
        }
    }
}
void disableMenuOption(const char* optionName) {
    for (int i=0; i<numMenus; i++) {
        for (int j=0; j<menuList[i].numOptions; j++) {
            if (strcasecmp(optionName, menuList[i].options[j].name) == 0) {
                menuList[i].options[j].enabled = false;
                /*
                if (isMenuOn())
                    redrawMenu();
                    */
            }
        }
    }
}

void menuParseConfig(const char* line) {
    char* equalsPos = strchr(line, '=');
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

void menuPrintConfig(FILE* file) {
    for (int i=0; i<numMenus; i++) {
        for (int j=0; j<menuList[i].numOptions; j++) {
            if (menuList[i].options[j].numValues != 0)
                fiprintf(file, "%s=%d\n", menuList[i].options[j].name, menuList[i].options[j].selection);
        }
    }
}

void printLog(const char *format, ...) {
    if (isMenuOn() || sharedData->scalingOn)
        return;
    va_list args;
    va_start(args, format);

    if (consoleDebugOutput)
        viprintf(format, args);
}


// updateScreens helper functions

void enableConsoleBacklight2() {
    powerOn(backlights[!gameScreen]);
}
void enableConsoleBacklight() {
    // For some reason, waiting 2 frames helps eliminate a white flash.
    doAtVBlank(enableConsoleBacklight2);
}

// 2 frames delay
void setupScaledScreens2() {
    REG_DISPCNT &= ~(3<<16); // Disable main display
    REG_DISPCNT_SUB |= 1<<16; // Enable sub display
    if (gameScreen == 0)
        lcdMainOnBottom();
    else
        lcdMainOnTop();
    powerOn(backlights[gameScreen]);

    refreshScaleMode();
}

// 1 frame delay
void setupScaledScreens1() {
    powerOff(backlights[!gameScreen]);

    if (!singleScreenMode) {
        // Disable sub screen for 1 frame
        REG_DISPCNT_SUB &= ~(3<<16);
    }

    // By next vblank, the scaled image will be ready.
    doAtVBlank(setupScaledScreens2);
}

void setupUnscaledScreens() {
    if (!consoleInitialized) {
        consoleDemoInit(); // Or, consoleInit(menuConsole, ...)
        setPrintConsole(menuConsole);
        BG_PALETTE_SUB[8*16 - 1] = RGB15(17,17,17); // Grey (replaces a color established in consoleDemoInit)
        consoleInitialized = true;
    }

    if (gameScreen == 0) {
        if (singleScreenMode && (isMenuOn() || isFileChooserOn()))
            lcdMainOnBottom();
        else
            lcdMainOnTop();
    }
    else {
        if (singleScreenMode && (isMenuOn() || isFileChooserOn()))
            lcdMainOnTop();
        else
            lcdMainOnBottom();
    }

    if (singleScreenMode) {
        powerOn(backlights[gameScreen]);
        powerOff(backlights[!gameScreen]);
        if (isMenuOn() || isFileChooserOn()) {
            REG_DISPCNT &= ~(3<<16);
            REG_DISPCNT_SUB |= 1<<16;
        }
        else {
            REG_DISPCNT_SUB &= ~(3<<16);
            REG_DISPCNT |= 1<<16;
        }
    }
    else {
        REG_DISPCNT &= ~(3<<16);
        REG_DISPCNT |= 1<<16; // Enable main display

        int screensToSet[2];

        screensToSet[gameScreen] = true;

        if (!(fpsOutput || timeOutput || consoleDebugOutput || isMenuOn() || isFileChooserOn())) {
            screensToSet[!gameScreen] = false;
            REG_DISPCNT_SUB &= ~(3<<16); // Disable sub display
        }
        else {
            screensToSet[!gameScreen] = true;
            REG_DISPCNT_SUB &= ~(3<<16);
            REG_DISPCNT_SUB |= 1<<16; // Enable sub display
        }

        for (int i=0; i<2; i++) {
            if (screensToSet[i]) {
                if (i == !gameScreen)
                    doAtVBlank(enableConsoleBacklight);
                else
                    powerOn(backlights[i]);
            }
            else
                powerOff(backlights[i]);
        }
    }
}

void updateScreens(bool waitToFinish) {
    if (!gbsMode && !isMenuOn() && !isFileChooserOn() && scaleMode != 0) {
        // Manage screens with scaling enabled:
        sharedData->scalingOn = 1;

        // The VRAM used for the console will be overwritten, so...
        consoleInitialized = false;


        doAtVBlank(setupScaledScreens1);

        if (waitToFinish) {
            swiWaitForVBlank();
            swiWaitForVBlank();
        }
    }
    else {
        // Manage screens normally
        sharedData->scalingOn = 0;

        doAtVBlank(setupUnscaledScreens);
        if (waitToFinish)
            swiWaitForVBlank();
    }
}


// Console helper functions

PrintConsole* printConsole;

void setPrintConsole(PrintConsole* console) {
    printConsole = console;
    consoleSelect(console);
}

PrintConsole* getPrintConsole() {
    return printConsole;
}

void consoleSetPosColor(int x, int y, int color) {
    u16* map = BG_MAP_RAM_SUB(22);
    map[y*32+x] &= ~TILE_PALETTE(15);
    map[y*32+x] |= TILE_PALETTE(color);
}

void consoleSetLineColor(int line, int color) {
    u16* map = BG_MAP_RAM_SUB(22);
    for (int i=0; i<32; i++) {
        map[line*32+i] &= ~TILE_PALETTE(15);
        map[line*32+i] |= TILE_PALETTE(color);
    }
}

void iprintfColored(int palette, const char *format, ...) {
    va_list args;
    va_start(args, format);

    PrintConsole* console = getPrintConsole();
    int x = console->cursorX;
    int y = console->cursorY;

    char s[100];
    vsiprintf(s, format, args);

    u16* dest = BG_MAP_RAM_SUB(22)+y*32+x;
    for (uint i=0; i<strlen(s); i++) {
        if (s[i] == '\n') {
            x = 0;
            y++;
        }
        else {
            *(dest++) = s[i] | TILE_PALETTE(palette);
            x++;
            if (x == 32) {
                x = 0;
                y++;
            }
        }
    }
    console->cursorX = x;
    console->cursorY = y;
    //iprintf(s);
}


int checkRumble() {
    if (__dsimode)
        return 0;

    sysSetCartOwner(BUS_OWNER_ARM9);

    OpenNorWrite();
    uint32 rumbleID = ReadNorFlashID();
    CloseNorWrite();


    if (isRumbleInserted())
        return 1; //Warioware / Official rumble found.
    else if (rumbleID != 0)
        return 2; //3in1 found
    else
        return 0; //No rumble found
}

int checkCamera() {
    if (__dsimode) {
        return 2;
    }
    return 0;
}

// Misc stuff

void disableSleepMode() {
    if (sharedData->enableSleepMode) {
        sharedData->enableSleepMode = false;
        fifoSendValue32(FIFO_PM, PM_REQ_SLEEP_DISABLE);
    }
}

void enableSleepMode() {
    if (!sharedData->enableSleepMode) {
        sharedData->enableSleepMode = true;
        fifoSendValue32(FIFO_PM, PM_REQ_SLEEP_ENABLE);
    }
}

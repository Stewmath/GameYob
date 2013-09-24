#include <nds.h>
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


void printVersionInfo(); // Defined in version.cpp

void subMenuGenericUpdateFunc(); // Private function here


const int screenTileWidth = 32;
const int backlights[] = {PM_BACKLIGHT_TOP, PM_BACKLIGHT_BOTTOM};

PrintConsole* printConsole;

bool consoleDebugOutput = false;
bool consoleOn = false;
bool consoleInitialized = false;
int menu=0;
int option = -1;
char printMessage[33];
int consoleScreen;
int stateNum=0;

bool nukeBorder=false;
int gbcModeOption=0;
bool gbaModeOption=false;
int sgbModeOption=false;

bool customBordersEnabled;
bool sgbBordersEnabled;
bool autoSavingEnabled;

volatile int consoleSelectedRow = -1;

void (*subMenuUpdateFunc)();

extern int interruptWaitMode;
extern bool windowDisabled;
extern bool hblankDisabled;
extern int halt;

extern int rumbleInserted;
extern int rumbleStrength;

void suspendFunc(int value) {
    muteSND();
    printConsoleMessage("Suspending...");
    if (!autoSavingEnabled)
        saveGame();
    saveState(-1);
    printMessage[0] = '\0';
    closeMenu();
    selectRom();
}
void exitFunc(int value) {
    muteSND();
    if (!autoSavingEnabled && numRamBanks && !gbsMode) {
        printConsoleMessage("Saving...");
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
		printConsoleMessage("Warning: link emulation sucks.");
        enableNifi();
	}
    else
        disableNifi();
}

void cheatFunc(int value) {
    if (!startCheatMenu())
        printConsoleMessage("No cheats found!");
}
void keyConfigFunc(int value) {
    startKeyConfigChooser();
}

void saveSettingsFunc(int value) {
    printConsoleMessage("Saving settings...");
    writeConfigFile();
    printConsoleMessage("Settings saved.");
}

void stateSelectFunc(int value) {
    stateNum = value;
    if (checkStateExists(stateNum))
        enableMenuOption("Load State");
    else
        disableMenuOption("Load State");
}
void stateLoadFunc(int value) {
    printConsoleMessage("Loading state...");
    if (loadState(stateNum) == 0) {
        closeMenu();
        updateScreens();
        printMessage[0] = '\0';
    }
}
void stateSaveFunc(int value) {
    printConsoleMessage("Saving state...");
    saveState(stateNum);
    printConsoleMessage("State saved.");
}
void resetFunc(int value) {
    nukeBorder = false;
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
    consoleScreen = !value;
    updateScreens();
}

void setScaleModeFunc(int value) {
    scaleMode = value;
    if (!isMenuOn()) {
        updateScreens();
    }
    if (value == 0) {
        doAtVBlank(checkBorder);
        if (customBorderExists)
            enableMenuOption("Custom Border");
        enableMenuOption("Console Output");
    }
    else {
        disableMenuOption("Custom Border");
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

void serialFunc(int value) {
    ioRam[0x01] = packetData;
    requestInterrupt(SERIAL);
    ioRam[0x02] &= ~0x80;
}

void setRumbleFunc(int value) {
    rumbleStrength = value;

    rumbleInserted = checkRumble();
}

void hyperSoundFunc(int value) {
    hyperSound = value;
    sharedData->hyperSound = value;
    sharedData->fifosSent++;
    fifoSendValue32(FIFO_USER_01, GBSND_HYPERSOUND_ENABLE_COMMAND<<20 | hyperSound);
}

void setAutoSaveFunc(int value) {
    autoSavingEnabled = value;
    saveGame(); // Synchronizes save file with filesystem
    if (numRamBanks && !gbsMode && !autoSavingEnabled)
        enableMenuOption("Exit without saving");
    else
        disableMenuOption("Exit without saving");
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
        8,
        {
            {"Exit", exitFunc, 0, {}, 0},
            {"Reset", resetFunc, 0, {}, 0},
            {"State Slot", stateSelectFunc, 10, {"0","1","2","3","4","5","6","7","8","9"}, 0},
            {"Save State", stateSaveFunc, 0, {}, 0},
            {"Load State", stateLoadFunc, 0, {}, 0},
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
            {"Wireless Link", nifiEnableFunc, 2, {"Off","On"}, 0},
            {"Autosaving", setAutoSaveFunc, 2, {"Off","On"}, 0},
            {"Save Settings", saveSettingsFunc, 0, {}, 0}
        }
    },
    {
        "Display",
        5,
        {
            {"Game Screen", setScreenFunc, 2, {"Top","Bottom"}, 0},
            {"Scaling", setScaleModeFunc, 3, {"Off","Aspect","Full"}, 0},
            {"Scale Filter", setScaleFilterFunc, 2, {"Off","On"}, 1},
            {"Custom Border", customBorderEnableFunc, 2, {"Off","On"}, 1},
            {"SGB Borders", sgbBorderEnableFunc, 2, {"Off","On"}, 1}
        }
    },
    {
        "GB Modes",
        4,
        {
            {"GBC Bios", biosEnableFunc, 3, {"Off","GB Only","On"}, 1},
            {"GBC Mode", gameboyModeFunc, 3, {"Off","If Needed","On"}, 2},
            {"Detect GBA", gbaModeFunc, 2, {"Off","On"}, 0},
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

void setConsoleDefaults() {
    for (int i=0; i<numMenus; i++) {
        for (int j=0; j<menuList[i].numOptions; j++) {
            menuList[i].options[j].selection = menuList[i].options[j].defaultSelection;
            menuList[i].options[j].enabled = true;
            if (menuList[i].options[j].numValues != 0) {
                menuList[i].options[j].function(menuList[i].options[j].defaultSelection);
            }
        }
    }
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

    PrintConsole* console = consoleGetDefault();
    int x = printConsole->cursorX;
    int y = printConsole->cursorY;

    char s[100];
    vsiprintf(s, format, args);
    iprintf(s);

    for (uint i=0; i<strlen(s); i++) {
        consoleSetPosColor(x, y, palette);
        x++;
        if (x == 32) {
            x = 0;
            y++;
        }
    }
}

void redrawMenu() {
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
}

// Message will be printed immediately, but also stored in case it's overwritten 
// right away.
void printConsoleMessage(const char* s) {
    strncpy(printMessage, s, 33);

    int newlines = 23-(menuList[menu].numOptions*2+2)-1;
    for (int i=0; i<newlines; i++)
        iprintf("\n");
    int spaces = 31-strlen(printMessage);
    for (int i=0; i<spaces; i++)
        iprintf(" ");
    iprintf("%s\n", printMessage);
}

void displayMenu() {
    consoleOn = true;
    fastForwardMode = false;
    if (checkRumble())
        enableMenuOption("Rumble Pak");
    else
        disableMenuOption("Rumble Pak");
    // Enable backlight if necessary, and delay as necessary if the console
    // needs to be initialized.
    updateScreens(!consoleInitialized);
    redrawMenu();
}
void closeMenu() {
    consoleOn = false;
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

void subMenuGenericUpdateFunc() {
    if (keyJustPressed(KEY_A) || keyJustPressed(KEY_B))
        closeSubMenu();
}

void displaySubMenu(void (*updateFunc)()) {
    subMenuUpdateFunc = updateFunc;
}
void closeSubMenu() {
    subMenuUpdateFunc = NULL;
    doAtVBlank(redrawMenu);
}

bool isMenuOn() {
    return consoleOn;
}


int getMenuOption(const char* optionName) {
    for (int i=0; i<numMenus; i++) {
        for (int j=0; j<menuList[i].numOptions; j++) {
            if (strcmpi(optionName, menuList[i].options[j].name) == 0) {
                return menuList[i].options[j].selection;
            }
        }
    }
    return 0;
}
void setMenuOption(const char* optionName, int value) {
    for (int i=0; i<numMenus; i++) {
        for (int j=0; j<menuList[i].numOptions; j++) {
            if (strcmpi(optionName, menuList[i].options[j].name) == 0) {
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
            if (strcmpi(optionName, menuList[i].options[j].name) == 0) {
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
            if (strcmpi(optionName, menuList[i].options[j].name) == 0) {
                menuList[i].options[j].enabled = false;
                /*
                if (isMenuOn())
                    redrawMenu();
                    */
            }
        }
    }
}

void enableConsoleBacklight2() {
    powerOn(backlights[consoleScreen]);
}
void enableConsoleBacklight() {
    // For some reason, waiting 2 frames helps eliminate a white flash.
    doAtVBlank(enableConsoleBacklight2);
}

PrintConsole dummyConsole;
// 2 frames delay
void setupScaledScreens2() {
    REG_DISPCNT &= ~(3<<16); // Disable main display
    REG_DISPCNT_SUB |= 1<<16; // Enable sub display
    if (consoleScreen == 0)
        lcdMainOnTop();
    else
        lcdMainOnBottom();
    powerOn(backlights[!consoleScreen]);

    refreshScaleMode();
}

// 1 frame delay
void setupScaledScreens1() {
    powerOff(backlights[consoleScreen]);
    REG_DISPCNT_SUB &= ~(3<<16); // Disable sub display (for 1 frame. Gotta hide the ugliness...)

    // By next vblank, the scaled image will be ready.
    doAtVBlank(setupScaledScreens2);
}

void setupUnscaledScreens() {
    int screensToSet[2];

    REG_DISPCNT &= ~(3<<16);
    REG_DISPCNT |= 1<<16; // Enable main display

    consoleSelect(NULL); // Select default console
    if (!consoleInitialized) {
        printConsole = consoleDemoInit();
        BG_PALETTE_SUB[8*16 - 1] = RGB15(17,17,17); // Grey (replaces a color established in consoleDemoInit)
        consoleInitialized = true;
    }
    if (consoleScreen == 0)
        lcdMainOnBottom();
    else
        lcdMainOnTop();

    screensToSet[!consoleScreen] = true;

    if (!(fpsOutput || timeOutput || consoleDebugOutput || isMenuOn() || isFileChooserOn())) {
        screensToSet[consoleScreen] = false;
        REG_DISPCNT_SUB &= ~(3<<16); // Disable sub display
    }
    else {
        screensToSet[consoleScreen] = true;
        REG_DISPCNT_SUB &= ~(3<<16);
        REG_DISPCNT_SUB |= 1<<16; // Enable sub display
    }

    for (int i=0; i<2; i++) {
        if (screensToSet[i]) {
            if (i == consoleScreen)
                doAtVBlank(enableConsoleBacklight);
            else
                powerOn(backlights[i]);
        }
        else
            powerOff(backlights[i]);
    }
}

void updateScreens(bool waitToFinish) {
    if (!gbsMode && !isMenuOn() && !isFileChooserOn() && scaleMode != 0) {
        // Manage screens in the case that scaling is enabled:
        sharedData->scalingOn = 1;
        // Give it a dummy console so it won't write over bank C
        consoleSelect(&dummyConsole);
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
                return;
            }
        }
    }
}

void consolePrintConfig(FILE* file) {
    for (int i=0; i<numMenus; i++) {
        for (int j=0; j<menuList[i].numOptions; j++) {
            if (menuList[i].options[j].numValues != 0)
                fiprintf(file, "%s=%d\n", menuList[i].options[j].name, menuList[i].options[j].selection);
        }
    }
}

void addToLog(const char* format, va_list args) {
    if (consoleDebugOutput)
        viprintf(format, args);
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


//3in1 stuff, find better place for it.
//I tried including the files these functions came from, but I got compile errors and didn't feel like fixing them.
#define FlashBase    	0x08000000
void		OpenNorWrite()
{
    *(vuint16 *)0x9fe0000 = 0xd200;
    *(vuint16 *)0x8000000 = 0x1500;
    *(vuint16 *)0x8020000 = 0xd200;
    *(vuint16 *)0x8040000 = 0x1500;
    *(vuint16 *)0x9C40000 = 0x1500;
    *(vuint16 *)0x9fc0000 = 0x1500;
}


void		CloseNorWrite()
{
    *(vuint16 *)0x9fe0000 = 0xd200;
    *(vuint16 *)0x8000000 = 0x1500;
    *(vuint16 *)0x8020000 = 0xd200;
    *(vuint16 *)0x8040000 = 0x1500;
    *(vuint16 *)0x9C40000 = 0xd200;
    *(vuint16 *)0x9fc0000 = 0x1500;
}
uint32   ReadNorFlashID()
{
        vuint16 id1,id2,id3,id4;
        uint32 ID = 0;
        //check intel 512M 3in1 card
        *((vuint16 *)(FlashBase+0)) = 0xFF ;
        *((vuint16 *)(FlashBase+0x1000*2)) = 0xFF ;
        *((vuint16 *)(FlashBase+0)) = 0x90 ;
        *((vuint16 *)(FlashBase+0x1000*2)) = 0x90 ;
        id1 = *((vuint16 *)(FlashBase+0)) ;
        id2 = *((vuint16 *)(FlashBase+0x1000*2)) ;
        id3 = *((vuint16 *)(FlashBase+1*2)) ;
        id4 = *((vuint16 *)(FlashBase+0x1001*2)) ;
        if(id3==0x8810)
            id3=0x8816;
        if(id4==0x8810)
            id4=0x8816;
        //_consolePrintf("id1=%x\,id2=%x,id3=%x,id4=%xn",id1,id2,id3,id4);
        if( (id1==0x89)&& (id2==0x89) &&(id3==0x8816) && (id4==0x8816))
        {
            ID = 0x89168916;
            return ID;
        }
        //256M
        *((vuint16 *)(FlashBase+0x555*2)) = 0xAA ;
        *((vuint16 *)(FlashBase+0x2AA*2)) = 0x55 ;
        *((vuint16 *)(FlashBase+0x555*2)) = 0x90 ;

        *((vuint16 *)(FlashBase+0x1555*2)) = 0xAA ;
        *((vuint16 *)(FlashBase+0x12AA*2)) = 0x55 ;
        *((vuint16 *)(FlashBase+0x1555*2)) = 0x90 ;

        id1 = *((vuint16 *)(FlashBase+0x2)) ;
        id2 = *((vuint16 *)(FlashBase+0x2002)) ;
        if( (id1!=0x227E)|| (id2!=0x227E))
            return 0;
        
        id1 = *((vuint16 *)(FlashBase+0xE*2)) ;
        id2 = *((vuint16 *)(FlashBase+0x100e*2)) ;
        if(id1==0x2218 && id2==0x2218)			//H6H6
        {
            ID = 0x227E2218;
            return 0x227E2218;
        }
            
        if(id1==0x2202 && id2==0x2202)			//VZ064
        {
            ID = 0x227E2202;
            return 0x227E2202;
        }
        if(id1==0x2202 && id2==0x2220)			//VZ064
        {
            ID = 0x227E2202;
            return 0x227E2202;
        }
        if(id1==0x2202 && id2==0x2215)			//VZ064
        {
            ID = 0x227E2202;
            return 0x227E2202;
        }
        
            
        return 0;
            
}

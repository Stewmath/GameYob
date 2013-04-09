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
bool gbaMode=false;

extern int interruptWaitMode;
extern bool windowDisabled;
extern bool hblankDisabled;
extern int frameskip;
extern int halt;

extern int rumbleInserted;
extern int rumbleStrength;

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
void exitNoSaveFunc(int value) {
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
    if (!startCheatMenu())
        printConsoleMessage("No cheats found!");
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

void gbaModeFunc(int value) {
    gbaMode = value;
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
    if (value > 0)
        rumbleStrength = value;
    else
        rumbleStrength = 0;

    sysSetCartOwner(BUS_OWNER_ARM9);

    OpenNorWrite();
    uint32 rumbleID = ReadNorFlashID();
    CloseNorWrite();


    if (isRumbleInserted())
        rumbleInserted = 1; //Warioware / Official rumble found.
    else if (rumbleID != 0)
        rumbleInserted = 2; //3in1 found
    else
        rumbleInserted = 0; //No rumble found
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
        8,
        {
            {"Suspend", suspendFunc, 0, {}, 0},
            {"State Slot", stateSelectFunc, 10, {"0","1","2","3","4","5","6","7","8","9"}, 0},
            {"Save State", stateSaveFunc, 0, {}, 0},
            {"Load State", stateLoadFunc, 0, {}, 0},
            {"Reset", resetFunc, 0, {}, 0},
            {"Quit to Launcher/Reboot", returnToLauncherFunc, 0, {}, 0},
            {"Exit without saving", exitNoSaveFunc, 0, {}, 0},
            {"Save & Exit", exitFunc, 0, {}, 0},
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
            {"Rumble Pak", setRumbleFunc, 4, {"Off","Low","Mid","High"}, 2},
            {"Save Settings", saveSettingsFunc, 0, {}, 0}
        }
    },
    {
        "GB Modes",
        3,
        {
            {"GBC Bios", biosEnableFunc, 2, {"Off","On"}, 1},
            {"GBC Mode", gameboyModeFunc, 3, {"Off","If Needed","On"}, 2},
            {"GBC on GBA", gbaModeFunc, 2, {"Off","On"}, 0}
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




//3in1 stuff, find better place for it.
//I tried including the files these functions came from, but I got compile errors and didn't feel like fixing them.
#define FlashBase		0x08000000
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
            return 0x89168916;
        }
        //¼ì²â256M¿¨
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

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


void versionInfoFunc(int value); // Defined in version.cpp


const int screenTileWidth = 32;
const int backlights[] = {PM_BACKLIGHT_TOP, PM_BACKLIGHT_BOTTOM};

bool consoleDebugOutput = false;
bool quitConsole = false;
bool consoleOn = false;
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

extern int interruptWaitMode;
extern bool windowDisabled;
extern bool hblankDisabled;
extern int frameskip;
extern int halt;

extern int rumbleInserted;
extern int rumbleStrength;

void suspendFunc(int value) {
    printConsoleMessage("Suspending...");
    if (!autoSavingEnabled)
        saveGame();
    saveState(-1);
    printMessage[0] = '\0';
    selectRom();
    quitConsole = true;
}
void exitFunc(int value) {
    if (!autoSavingEnabled) {
        printConsoleMessage("Saving...");
        saveGame();
    }
    printMessage[0] = '\0';
    selectRom();
    quitConsole = true;
}
void exitNoSaveFunc(int value) {
    selectRom();
    quitConsole = true;
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
    nukeBorder = false;
    initializeGameboy();
    quitConsole = true;
}
void returnFunc(int value) {
    quitConsole = true;
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
//    updateScreens();
}
void setScaleFilterFunc(int value) {
    scaleFilter = value;
//    updateScreens();
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
    rumbleStrength = value;

    if (__dsimode)
        return;

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

void hyperSoundFunc(int value) {
    hyperSound = value;
    sharedData->hyperSound = value;
    sharedData->fifosSent++;
    fifoSendValue32(FIFO_USER_01, GBSND_HYPERSOUND_ENABLE_COMMAND<<20 | hyperSound);
}

void setAutoSaveFunc(int value) {
    autoSavingEnabled = value;
    saveGame(); // Synchronizes save file with filesystem
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
    const char *name;
    int numOptions;
    MenuOption options[10];
};


ConsoleSubMenu menuList[] = {
    {
        "Options",
        8,
        {
            {"Save & Exit", exitFunc, 0, {}, 0},
            {"Reset", resetFunc, 0, {}, 0},
            {"State Slot", stateSelectFunc, 10, {"0","1","2","3","4","5","6","7","8","9"}, 0},
            {"Save State", stateSaveFunc, 0, {}, 0},
            {"Load State", stateLoadFunc, 0, {}, 0},
            {"Quit to Launcher/Reboot", returnToLauncherFunc, 0, {}, 0},
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
            {"Sound Timing Fix", hyperSoundFunc, 2, {"Off","On"}, 1},
            {"Console Output", consoleOutputFunc, 4, {"Off","Time","FPS+Time","Debug"}, 0},
            {"Wireless Link", nifiEnableFunc, 2, {"Off","On"}, 0},
            {"Rumble Pak", setRumbleFunc, 4, {"Off","Low","Mid","High"}, 2},
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
            {"Advance Frame", advanceFrameFunc, 0, {}, 0},
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

void initConsole() {
    for (int i=0; i<numMenus; i++) {
        for (int j=0; j<menuList[i].numOptions; j++) {
            menuList[i].options[j].selection = menuList[i].options[j].defaultSelection;
            if (menuList[i].options[j].numValues != 0) {
                menuList[i].options[j].function(menuList[i].options[j].defaultSelection);
            }
        }
    }
    updateScreens();
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

void displayConsole() {
    fastForwardMode = false;
    advanceFrame = 0;
    consoleOn = true;
    quitConsole = false;

    // Set volume to zero
    sharedData->fifosSent++;
    fifoSendValue32(FIFO_USER_01, GBSND_MUTE_COMMAND<<20 | 1);

    doRumble(0);
    updateScreens(); // Enable backlight if necessary

    while (!quitConsole) {
        consoleClear();
		int pos=0;
        int nameStart = (32-strlen(menuList[menu].name)-2)/2;
        if (option == -1)
            nameStart-=2;
		iprintf("<");
		pos++;
        for (; pos<nameStart; pos++)
            iprintf(" ");
        if (option == -1) {
            iprintf("* ");
			pos += 2;
		}
        iprintf("[");
        iprintf(menuList[menu].name);
        iprintf("]");
		pos += 2 + strlen(menuList[menu].name);
        if (option == -1) {
            iprintf(" *");
			pos += 2;
		}
		for (; pos < 31; pos++)
			iprintf(" ");
		iprintf(">");
        iprintf("\n");

        for (int i=0; i<menuList[menu].numOptions; i++) {
            if (menuList[menu].options[i].numValues == 0) {
                for (unsigned int j=0; j<(32-strlen(menuList[menu].options[i].name))/2-2; j++)
                    iprintf(" ");
                if (i == option)
                    iprintf("* %s *\n\n", menuList[menu].options[i].name);
                else
                    iprintf("  %s  \n\n", menuList[menu].options[i].name);
            }
            else {
                for (unsigned int j=0; j<17-strlen(menuList[menu].options[i].name); j++)
                    iprintf(" ");
                iprintf("%s ", menuList[menu].options[i].name);
                if (i == option)
                    iprintf("* ");
                else
                    iprintf("  ");
                iprintf("%s", menuList[menu].options[i].values[menuList[menu].options[i].selection]);
                if (i == option)
                    iprintf(" *");
                iprintf("\n\n");
            }
        }

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
                forceReleaseKey(KEY_A);
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
    if (!soundDisabled) {
        // Unmute
        sharedData->fifosSent++;
        fifoSendValue32(FIFO_USER_01, GBSND_MUTE_COMMAND<<20 | 0);
    }
    consoleClear();
    consoleOn = false;

    updateScreens();
}

PrintConsole blankConsole;
void updateScreens() {
    swiWaitForVBlank();

    if (!gbsMode && !consoleOn && scaleMode != 0) {
        sharedData->scalingOn = 1;

        REG_DISPCNT &= ~(3<<16);
        powerOff(backlights[consoleScreen]);
        if (consoleScreen == 0)
            lcdMainOnTop();
        else
            lcdMainOnBottom();
        powerOn(backlights[!consoleScreen]);

        // Give it a dummy console so it won't write over bank C
        consoleSelect(&blankConsole);
        // Clear bank C since it used to be used for the console
        memset(BG_GFX_SUB, 0, 256*144*2);
        refreshScaleMode();
    }
    else {
        sharedData->scalingOn = 0;

        REG_DISPCNT |= 1<<16;
        videoBgEnableSub(0);
        videoBgDisableSub(2);
        videoBgDisableSub(3);
        vramSetBankD(VRAM_D_MAIN_BG_0x06040000);

        consoleSelect(NULL);
        consoleDemoInit();
        if (consoleScreen == 0)
            lcdMainOnBottom();
        else
            lcdMainOnTop();

        powerOn(backlights[!consoleScreen]);

        if (!(fpsOutput || timeOutput || consoleDebugOutput || consoleOn))
            powerOff(backlights[consoleScreen]);
        else
            powerOn(backlights[consoleScreen]);
    }
    if (gbsMode) {
        powerOn(backlights[consoleScreen]);
        powerOff(backlights[!consoleScreen]);
        REG_DISPCNT &= ~(3<<16);
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

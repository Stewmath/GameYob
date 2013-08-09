#include <nds.h>
#include <fat.h>

#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>

#include "libfat_fake.h"
#include "inputhelper.h"
#include "mmu.h"
#include "gameboy.h"
#include "main.h"
#include "console.h"
#include "gbcpu.h"
#include "nifi.h"
#include "gbgfx.h"
#include "gbsnd.h"
#include "cheats.h"
#include "sgb.h"
#include "gbs.h"
#include "common.h"

FILE* romFile=NULL;
FILE* saveFile=NULL;
char filename[100];
char savename[100];
char basename[100];
char romTitle[20];

char* romPath = NULL;
char* biosPath = NULL;

// Values taken from the cartridge header
u8 ramSize;
u8 mapper;
u8 cgbFlag;
u8 romSize;

int keysPressed=0;
int lastKeysPressed=0;
int keysForceReleased=0;
int repeatStartTimer=0;
int repeatTimer=0;

bool advanceFrame=false;

u8* romSlot0;
u8* romSlot1;
int maxLoadedRomBanks;
int numLoadedRomBanks;
u8* romBankSlots = NULL; // Each 0x4000 bytes = one slot
int bankSlotIDs[MAX_ROM_BANKS]; // Keeps track of which bank occupies which slot
std::vector<int> lastBanksUsed;

bool suspendStateExists;

void initInput()
{
    //fatInit(2, true);
    fatInitDefault();
    chdir("/lameboy"); // Default rom directory

    if (__dsimode)
        maxLoadedRomBanks = 512; // 8 megabytes
    else
        maxLoadedRomBanks = 128; // 2 megabytes

    romBankSlots = (u8*)malloc(maxLoadedRomBanks*0x4000);
}

void flushFatCache() {
    // This involves things from libfat which aren't normally visible
    devoptab_t* devops = (devoptab_t*)GetDeviceOpTab ("sd");
    PARTITION* partition = (PARTITION*)devops->deviceData;
    _FAT_cache_flush(partition->cache); // Flush the cache manually
}

enum {
    KEY_NONE,
    KEY_GB_A, KEY_GB_B, KEY_GB_LEFT, KEY_GB_RIGHT, KEY_GB_UP, KEY_GB_DOWN, KEY_GB_START, KEY_GB_SELECT,
    KEY_MENU, KEY_SAVE, KEY_GB_AUTO_A, KEY_GB_AUTO_B, KEY_FAST_FORWARD, KEY_FAST_FORWARD_TOGGLE
};
const char* gbKeyNames[] = {"-","A","B","Left","Right","Up","Down","Start","Select",
    "Menu","Save","Autofire A","Autofire B", "Fast Forward", "FF Toggle"};
const char* dsKeyNames[] = {"A","B","Select","Start","Right","Left","Up","Down",
    "R","L","X","Y"};

const int NUM_GB_KEYS = sizeof(gbKeyNames)/sizeof(char*);
int keys[NUM_GB_KEYS];

struct KeyConfig {
    char name[32];
    int gbKeys[12];
};
KeyConfig defaultKeyConfig = {
    "Main",
    {KEY_GB_A,KEY_GB_B,KEY_GB_SELECT,KEY_GB_START,KEY_GB_RIGHT,KEY_GB_LEFT,KEY_GB_UP,KEY_GB_DOWN,
        KEY_MENU,KEY_FAST_FORWARD,KEY_SAVE,KEY_NONE}
};

std::vector<KeyConfig> keyConfigs;
unsigned int selectedKeyConfig=0;

void loadKeyConfig() {
    KeyConfig* keyConfig = &keyConfigs[selectedKeyConfig];
    for (int i=0; i<NUM_GB_KEYS; i++)
        keys[i] = 0;
    for (int i=0; i<12; i++) {
        keys[keyConfig->gbKeys[i]] |= BIT(i);
    }
}

void controlsParseConfig(const char* line2) {
    char line[100];
    strncpy(line, line2, 100);
    while (strlen(line) > 0 && (line[strlen(line)-1] == '\n' || line[strlen(line)-1] == ' '))
        line[strlen(line)-1] = '\0';
    if (line[0] == '(') {
        char* bracketEnd;
        if ((bracketEnd = strrchr(line, ')')) != 0) {
            *bracketEnd = '\0';
            const char* name = line+1;

            keyConfigs.push_back(KeyConfig());
            KeyConfig* config = &keyConfigs.back();
            strncpy(config->name, name, 32);
            config->name[31] = '\0';
            for (int i=0; i<12; i++)
                config->gbKeys[i] = KEY_NONE;
        }
        return;
    }
    char* equalsPos;
    if ((equalsPos = strrchr(line, '=')) != 0 && equalsPos != line+strlen(line)-1) {
        *equalsPos = '\0';

        if (strcmpi(line, "config") == 0) {
            selectedKeyConfig = atoi(equalsPos+1);
        }
        else {
            int dsKey = -1;
            for (int i=0; i<12; i++) {
                if (strcmpi(line, dsKeyNames[i]) == 0) {
                    dsKey = i;
                    break;
                }
            }
            int gbKey = -1;
            for (int i=0; i<NUM_GB_KEYS; i++) {
                if (strcmpi(equalsPos+1, gbKeyNames[i]) == 0) {
                    gbKey = i;
                    break;
                }
            }

            if (gbKey != -1 && dsKey != -1) {
                KeyConfig* config = &keyConfigs.back();
                config->gbKeys[dsKey] = gbKey;
            }
        }
    }
}
void controlsPrintConfig(FILE* file) {
    fiprintf(file, "config=%d\n", selectedKeyConfig);
    for (unsigned int i=0; i<keyConfigs.size(); i++) {
        fiprintf(file, "(%s)\n", keyConfigs[i].name);
        for (int j=0; j<12; j++) {
            fiprintf(file, "%s=%s\n", dsKeyNames[j], gbKeyNames[keyConfigs[i].gbKeys[j]]);
        }
    }
}

void startKeyConfigChooser() {
    bool quit=false;
    int option=-1;
    while (!quit) {
        KeyConfig* config = &keyConfigs[selectedKeyConfig];
        consoleClear();
        iprintf("Config: ");
        if (option == -1)
            iprintf("* %s *\n\n", config->name);
        else
            iprintf("  %s  \n\n", config->name);

        iprintf("       Button   Function\n\n");

        for (int i=0; i<12; i++) {
            int len = 11-strlen(dsKeyNames[i]);
            while (len > 0) {
                iprintf(" ");
                len--;
            }
            if (option == i) 
                iprintf("* %s | %s *\n", dsKeyNames[i], gbKeyNames[config->gbKeys[i]]);
            else
                iprintf("  %s | %s  \n", dsKeyNames[i], gbKeyNames[config->gbKeys[i]]);
        }
        iprintf("\n\n\n\nPress X to make a new config.");
        if (selectedKeyConfig != 0) /* can't erase the default */ {
            iprintf("\n\nPress Y to delete this config.");
        }

        while (true) {
            swiWaitForVBlank();
            readKeys();
            if (keyJustPressed(KEY_B)) {
                quit = true;
                break;
            }
            else if (keyJustPressed(KEY_X)) {
                keyConfigs.push_back(KeyConfig(*config));
                selectedKeyConfig = keyConfigs.size()-1;
                char name[32];
                siprintf(name, "Custom %d", keyConfigs.size()-1);
                strcpy(keyConfigs.back().name, name);
                option = -1;
                break;
            }
            else if (keyJustPressed(KEY_Y)) {
                if (selectedKeyConfig != 0) /* can't erase the default */ {
                    keyConfigs.erase(keyConfigs.begin() + selectedKeyConfig);
                    if (selectedKeyConfig >= keyConfigs.size())
                        selectedKeyConfig = keyConfigs.size() - 1;
                    break;
                }
            }
            else if (keyPressedAutoRepeat(KEY_DOWN)) {
                if (option < 11)
                    option++;
                break;
            }
            else if (keyPressedAutoRepeat(KEY_UP)) {
                if (option > -1)
                    option--;
                break;
            }
            else if (keyPressedAutoRepeat(KEY_LEFT)) {
                if (option == -1) {
                    if (selectedKeyConfig == 0)
                        selectedKeyConfig = keyConfigs.size()-1;
                    else
                        selectedKeyConfig--;
                }
                else {
                    config->gbKeys[option]--;
                    if (config->gbKeys[option] < 0)
                        config->gbKeys[option] = NUM_GB_KEYS-1;
                }
                break;
            }
            else if (keyPressedAutoRepeat(KEY_RIGHT)) {
                if (option == -1) {
                    selectedKeyConfig++;
                    if (selectedKeyConfig >= keyConfigs.size())
                        selectedKeyConfig = 0;
                }
                else {
                    config->gbKeys[option]++;
                    if (config->gbKeys[option] >= NUM_GB_KEYS)
                        config->gbKeys[option] = 0;
                }
                break;
            }
        }
    }
    loadKeyConfig();
}

void generalParseConfig(const char* line) {
    char* equalsPos;
    if ((equalsPos = strrchr(line, '=')) != 0 && equalsPos != line+strlen(line)-1) {
        *equalsPos = '\0';
        const char* parameter = line;
        const char* value = equalsPos+1;

        if (strcmpi(parameter, "biosfile") == 0) {
            if (biosPath != 0)
                free(biosPath);
            biosPath = (char*)malloc(strlen(value)+1);
            strcpy(biosPath, value);
            loadBios(biosPath);
        }
        else if (strcmpi(parameter, "rompath") == 0) {
            if (romPath != 0)
                free(romPath);
            romPath = (char*)malloc(strlen(value)+1);
            strcpy(romPath, value);
            chdir(romPath);
        }
    }
}

void generalPrintConfig(FILE* file) {
    if (biosPath == 0)
        fiprintf(file, "biosfile=\n");
    else
        fiprintf(file, "biosfile=%s\n", biosPath);
    if (romPath == 0)
        fiprintf(file, "rompath=\n");
    else
        fiprintf(file, "rompath=%s\n", romPath);
}

void readConfigFile() {
    FILE* file = fopen("/gameyob.ini", "r");
    char line[100];
    void (*configParser)(const char*) = generalParseConfig;

    if (file == NULL)
        goto end;

    while (!feof(file)) {
        fgets(line, 100, file);
        char c=0;
        while (*line != '\0' && ((c = line[strlen(line)-1]) == ' ' || c == '\n' || c == '\r'))
            line[strlen(line)-1] = '\0';
        if (line[0] == '[') {
            char* endBrace;
            if ((endBrace = strrchr(line, ']')) != 0) {
                *endBrace = '\0';
                const char* section = line+1;
                if (strcmpi(section, "general") == 0) {
                    configParser = generalParseConfig;
                }
                else if (strcmpi(section, "console") == 0) {
                    configParser = consoleParseConfig;
                }
                else if (strcmpi(section, "controls") == 0) {
                    configParser = controlsParseConfig;
                }
            }
        }
        else
            configParser(line);
    }
    fclose(file);
end:
    if (keyConfigs.empty())
        keyConfigs.push_back(defaultKeyConfig);
    if (selectedKeyConfig >= keyConfigs.size())
        selectedKeyConfig = 0;
    loadKeyConfig();
}

void writeConfigFile() {
    FILE* file = fopen("/gameyob.ini", "w");
    fiprintf(file, "[general]\n");
    generalPrintConfig(file);
    fiprintf(file, "[console]\n");
    consolePrintConfig(file);
    fiprintf(file, "[controls]\n");
    controlsPrintConfig(file);
    fclose(file);

    char nameBuf[100];
    siprintf(nameBuf, "%s.cht", basename);
    saveCheats(nameBuf);
}


void loadBios(const char* filename) {
    FILE* file = fopen(filename, "rb");
    biosExists = file != NULL;
    if (biosExists)
        fread(bios, 1, 0x900, file);
}

int loadProgram(char* f)
{
    if (romFile != NULL)
        fclose(romFile);
    strcpy(filename, f);

    // Check if this is a GBS file
    gbsMode = (strcmpi(strrchr(filename, '.'), ".gbs") == 0);

    romFile = fopen(filename, "rb");
    if (romFile == NULL)
    {
        printLog("Error opening %s.\n", filename);
        return 1;
    }

    if (gbsMode) {
        fread(gbsHeader, 1, 0x70, romFile);
        readGBSHeader();
        fseek(romFile, 0, SEEK_END);
        numRomBanks = (ftell(romFile)-0x70+0x3fff)/0x4000; // Get number of banks, rounded up
    }
    else {
        fseek(romFile, 0, SEEK_END);
        numRomBanks = (ftell(romFile)+0x3fff)/0x4000; // Get number of banks, rounded up
    }
    rewind(romFile);


    if (numRomBanks <= maxLoadedRomBanks)
        numLoadedRomBanks = numRomBanks;
    else
        numLoadedRomBanks = maxLoadedRomBanks;

    romSlot0 = romBankSlots;
    romSlot1 = romBankSlots + 0x4000;

    for (int i=0; i<numRomBanks; i++) {
        bankSlotIDs[i] = -1;
    }

    // Load rom banks and initialize all those "bank" arrays
    lastBanksUsed = std::vector<int>();
    // Read bank 0
    if (gbsMode) {
        bankSlotIDs[0] = 0;
        fseek(romFile, 0x70, SEEK_SET);
        fread(romBankSlots+gbsLoadAddress, 1, 0x4000-gbsLoadAddress, romFile);
    }
    else {
        bankSlotIDs[0] = 0;
        fread(romBankSlots, 1, 0x4000, romFile);
    }
    // Read the rest of the banks
    for (int i=1; i<numLoadedRomBanks; i++) {
        bankSlotIDs[i] = i;
        fread(romBankSlots+0x4000*i, 1, 0x4000, romFile);
        lastBanksUsed.push_back(i);
    }

    strcpy(basename, filename);
    *(strrchr(basename, '.')) = '\0';
    strcpy(savename, basename);
    strcat(savename, ".sav");

    cgbFlag = romSlot0[0x143];
    romSize = romSlot0[0x148];
    ramSize = romSlot0[0x149];
    mapper  = romSlot0[0x147];

    int nameLength = 16;
    if (cgbFlag == 0x80 || cgbFlag == 0xc0)
        nameLength = 15;
    for (int i=0; i<nameLength; i++) 
        romTitle[i] = (char)romSlot0[i+0x134];
    romTitle[nameLength] = '\0';

    hasRumble = false;

    if (gbsMode) {
        MBC = MBC5;
    }
    else {
        switch (mapper) {
            case 0: case 8: case 9:
                MBC = MBC0; 
                break;
            case 1: case 2: case 3:
                MBC = MBC1;
                break;
            case 5: case 6:
                MBC = MBC2;
                break;
                //case 0xb: case 0xc: case 0xd:
                //MBC = MMM01;
                //break;
            case 0xf: case 0x10: case 0x11: case 0x12: case 0x13:
                MBC = MBC3;
                break;
                //case 0x15: case 0x16: case 0x17:
                //MBC = MBC4;
                //break;
            case 0x19: case 0x1a: case 0x1b: 
                MBC = MBC5;
                break;
            case 0x1c: case 0x1d: case 0x1e:
                MBC = MBC5;
                hasRumble = true;
                break;
            case 0x22:
                MBC = MBC7;
                break;
            case 0xea: /* Hack for SONIC5 */
                MBC = MBC1;
                break;
            case 0xfe:
                MBC = HUC3;
                break;
            case 0xff:
                MBC = HUC1;
                break;
            default:
                printLog("Unsupported MBC %02x\n", mapper);
                return 1;
        }

        // Little hack to preserve "quickread" from gbcpu.cpp.
        if (biosExists) {
            for (int i=0x100; i<0x150; i++)
                bios[i] = romSlot0[i];
        }

        char nameBuf[100];
        // Check for a suspend state
        siprintf(nameBuf, "%s.yss", basename);
        FILE* stateFile = fopen(nameBuf, "r");
        suspendStateExists = stateFile;
        if (stateFile)
            fclose(stateFile);

        // Load cheats
        siprintf(nameBuf, "%s.cht", basename);
        loadCheats(nameBuf);

    } // !gbsMode

    loadSave();

    // If we've loaded everything, close the rom file
    if (numRomBanks <= numLoadedRomBanks) {
        fclose(romFile);
        romFile = NULL;
    }

    // We haven't calculated the # of cycles to the next hardware event.
    cyclesToEvent = 1;

    return 0;
}

void loadRomBank() {
    if (bankSlotIDs[romBank] != -1 || numRomBanks <= numLoadedRomBanks || romBank == 0) {
        romSlot1 = romBankSlots+bankSlotIDs[romBank]*0x4000;
        return;
    }
    int bankToUnload = lastBanksUsed.back();
    lastBanksUsed.pop_back();
    int slot = bankSlotIDs[bankToUnload];
    bankSlotIDs[bankToUnload] = -1;
    bankSlotIDs[romBank] = slot;

    fseek(romFile, 0x4000*romBank, SEEK_SET);
    fread(romBankSlots+slot*0x4000, 1, 0x4000, romFile);

    lastBanksUsed.insert(lastBanksUsed.begin(), romBank);

    applyGGCheatsToBank(romBank);

    romSlot1 = romBankSlots+slot*0x4000;
}

bool isRomBankLoaded(int bank) {
    return bankSlotIDs[bank] != -1;
}
u8* getRomBank(int bank) {
    if (!isRomBankLoaded(bank))
        return 0;
    return romBankSlots+bankSlotIDs[bank]*0x4000;
}

int loadSave()
{
    if (saveFile != NULL)
        fclose(saveFile);
    saveFile = NULL;
    // unload previous save
    if (externRam != NULL) {
        for (int i=0; i<numRamBanks; i++) 
            free(externRam[i]);
        free(externRam);
    }
    externRam = NULL;

    if (gbsMode)
        numRamBanks = 1;
    else {
        // Get the game's external memory size and allocate the memory
        switch(ramSize)
        {
            case 0:
                numRamBanks = 0;
                break;
            case 1:
            case 2:
                numRamBanks = 1;
                break;
            case 3:
                numRamBanks = 4;
                break;
            default:
                printLog("Invalid RAM bank number: %x\nDefaulting to 4 banks\n", ramSize);
                numRamBanks = 4;
                break;
        }
        if (MBC == MBC2)
            numRamBanks = 1;
    }

    if (numRamBanks == 0)
        return 0;

    externRam = (u8**)malloc(numRamBanks*sizeof(u8*));
    for (int i=0; i<numRamBanks; i++)
        externRam[i] = (u8*)malloc(0x2000*sizeof(u8));

    if (gbsMode)
        return 0; // GBS files don't get to save.

    // Now load the data.
    saveFile = fopen(savename, "r+b");
    if (!saveFile) {
        // Create the file if it didn't exist
        saveFile = fopen(savename, "w");
        fseek(saveFile, numRamBanks*0x2000-1, SEEK_SET);
        fputc(0, saveFile);
        fclose(saveFile);

        saveFile = fopen(savename, "r+b");
    }

    for (int i=0; i<numRamBanks; i++)
        fread(externRam[i], 1, 0x2000, saveFile);

    switch (MBC) {
        case MBC3:
        case HUC3:
            fread(&gbClock, 1, sizeof(gbClock), saveFile);
            break;
    }
    return 0;
}

int saveGame()
{
    if (numRamBanks == 0 || saveFile == NULL)
        return 0;

    fseek(saveFile, 0, SEEK_SET);

    for (int i=0; i<numRamBanks; i++)
        fwrite(externRam[i], 1, 0x2000, saveFile);

    switch (MBC) {
        case MBC3:
        case HUC3:
            fwrite(&gbClock, 1, sizeof(gbClock), saveFile);
            break;
    }

    flushFatCache();

    return 0;
}

bool keyPressed(int key) {
    return keysPressed&key;
}
bool keyPressedAutoRepeat(int key) {
    if (keyJustPressed(key)) {
        repeatStartTimer = 14;
        return true;
    }
    if (keyPressed(key) && repeatStartTimer == 0 && repeatTimer == 0) {
        repeatTimer = 2;
        return true;
    }
    return false;
}
bool keyJustPressed(int key) {
    return ((keysPressed^lastKeysPressed)&keysPressed) & key;
}

void readKeys() {
    scanKeys();

    lastKeysPressed = keysPressed;
    keysPressed = keysHeld();
    for (int i=0; i<16; i++) {
        if (keysForceReleased & (1<<i)) {
            if (!(keysPressed & (1<<i)))
                keysForceReleased &= ~(1<<i);
        }
    }
    keysPressed &= ~keysForceReleased;
    if (repeatStartTimer > 0)
        repeatStartTimer--;
    if (repeatTimer > 0)
        repeatTimer--;
}

void forceReleaseKey(int key) {
    keysForceReleased |= key;
    keysPressed &= ~key;
}

char* getRomTitle() {
    return romTitle;
}

const char *mbcName[] = {"ROM","MBC1","MBC2","MBC3","MBC4","MBC5","MBC7","HUC3","HUC1"};

void printRomInfo() {
    consoleClear();
    iprintf("ROM Title: \"%s\"\n", romTitle);
    iprintf("Cartridge type: %.2x (%s)\n", mapper, mbcName[MBC]);
    iprintf("ROM Size: %.2x (%d banks)\n", romSize, numRomBanks);
    iprintf("RAM Size: %.2x (%d banks)\n", ramSize, numRamBanks);
    while (true) {
        swiWaitForVBlank();
        readKeys();
        if (keyJustPressed(KEY_A) || keyJustPressed(KEY_B))
            break;
    }
}

int autoFireCounterA=0,autoFireCounterB=0;
void updateInput()
{
    readKeys();

    buttonsPressed = 0xff;
    if (keyPressed(keys[KEY_GB_UP])) {
        buttonsPressed &= (0xFF ^ UP);
        if (!(ioRam[0x00] & 0x10))
            requestInterrupt(JOYPAD);
    }
    if (keyPressed(keys[KEY_GB_DOWN])) {
        buttonsPressed &= (0xFF ^ DOWN);
        if (!(ioRam[0x00] & 0x10))
            requestInterrupt(JOYPAD);
    }
    if (keyPressed(keys[KEY_GB_LEFT])) {
        buttonsPressed &= (0xFF ^ LEFT);
        if (!(ioRam[0x00] & 0x10))
            requestInterrupt(JOYPAD);
    }
    if (keyPressed(keys[KEY_GB_RIGHT])) {
        buttonsPressed &= (0xFF ^ RIGHT);
        if (!(ioRam[0x00] & 0x10))
            requestInterrupt(JOYPAD);
    }
    if (keyPressed(keys[KEY_GB_A])) {
        buttonsPressed &= (0xFF ^ BUTTONA);
        if (!(ioRam[0x00] & 0x20))
            requestInterrupt(JOYPAD);
    }
    if (keyPressed(keys[KEY_GB_B])) {
        buttonsPressed &= (0xFF ^ BUTTONB);
        if (!(ioRam[0x00] & 0x20))
            requestInterrupt(JOYPAD);
    }
    if (keyPressed(keys[KEY_GB_START])) {
        buttonsPressed &= (0xFF ^ START);
        if (!(ioRam[0x00] & 0x20))
            requestInterrupt(JOYPAD);
    }
    if (keyPressed(keys[KEY_GB_SELECT])) {
        buttonsPressed &= (0xFF ^ SELECT);
        if (!(ioRam[0x00] & 0x20))
            requestInterrupt(JOYPAD);
    }

    if (keyPressed(keys[KEY_GB_AUTO_A])) {
        if (autoFireCounterA <= 0) {
            buttonsPressed &= (0xFF ^ BUTTONA);
            if (!(ioRam[0x00] & 0x20))
                requestInterrupt(JOYPAD);
            autoFireCounterA = 2;
        }
        autoFireCounterA--;
    }
    if (keyPressed(keys[KEY_GB_AUTO_B])) {
        if (autoFireCounterB <= 0) {
            buttonsPressed &= (0xFF ^ BUTTONB);
            if (!(ioRam[0x00] & 0x20))
                requestInterrupt(JOYPAD);
            autoFireCounterB = 2;
        }
        autoFireCounterB--;
    }

    if (keyJustPressed(keys[KEY_SAVE])) {
        if (!autoSavingEnabled)
            saveGame();
    }
    if (advanceFrame || keyJustPressed(keys[KEY_MENU] | KEY_TOUCH)) {
        advanceFrame = 0;
        displayConsole();
    }

    fastForwardKey = keyPressed(keys[KEY_FAST_FORWARD]);
    if (keyJustPressed(keys[KEY_FAST_FORWARD_TOGGLE]))
        fastForwardMode = !fastForwardMode;

    if (fastForwardKey || fastForwardMode) {
        sharedData->hyperSound = false;
    }
    else {
        sharedData->hyperSound = hyperSound;
    }
}

const int STATE_VERSION = 4;

struct StateStruct {
    // version
    // bg/sprite PaletteData
    // vram
    // wram
    // hram
    // sram
    Registers regs;
    int halt, ime;
    bool doubleSpeed, biosOn;
    int gbMode;
    int romBank, ramBank, wramBank, vramBank;
    int memoryModel;
    clockStruct clock;
    int scanlineCounter, timerCounter, phaseCounter, dividerCounter;
    // v2
    int serialCounter;
    // v3
    bool ramEnabled;
    // MBC-specific stuff
    // v4
    //  bool sgbMode;
    //  If sgbMode == true:
    //   int sgbPacketLength;
    //   int sgbPacketsTransferred;
    //   int sgbPacketBit;
    //   u8 sgbCommand;
    //   u8 gfxMask;
    //   u8[20*18] sgbMap;
};

void saveState(int num) {
    FILE* outFile;
    StateStruct state;
    char statename[100];

    if (num == -1)
        siprintf(statename, "%s.yss", basename);
    else
        siprintf(statename, "%s.ys%d", basename, num);
    outFile = fopen(statename, "w");

    if (outFile == 0) {
        printConsoleMessage("Error opening file for writing.");
        return;
    }

    state.regs = gbRegs;
    state.halt = halt;
    state.ime = ime;
    state.doubleSpeed = doubleSpeed;
    state.biosOn = biosOn;
    state.gbMode = gbMode;
    state.romBank = romBank;
    state.ramBank = currentRamBank;
    state.wramBank = wramBank;
    state.vramBank = vramBank;
    state.memoryModel = memoryModel;
    state.clock = gbClock;
    state.scanlineCounter = scanlineCounter;
    state.timerCounter = timerCounter;
    state.phaseCounter = phaseCounter;
    state.dividerCounter = dividerCounter;
    state.serialCounter = serialCounter;
    state.ramEnabled = ramEnabled;

    fwrite(&STATE_VERSION, sizeof(int), 1, outFile);
    fwrite((char*)bgPaletteData, 1, sizeof(bgPaletteData), outFile);
    fwrite((char*)sprPaletteData, 1, sizeof(sprPaletteData), outFile);
    fwrite((char*)vram, 1, sizeof(vram), outFile);
    fwrite((char*)wram, 1, sizeof(wram), outFile);
    fwrite((char*)hram, 1, 0x200, outFile);
    for (int i=0; i<numRamBanks; i++)
        fwrite((char*)externRam[i], 1, 0x2000, outFile);

    fwrite((char*)&state, 1, sizeof(StateStruct), outFile);

    switch (MBC) {
        case HUC3:
            fwrite(&HuC3Mode,  1, sizeof(u8), outFile);
            fwrite(&HuC3Value, 1, sizeof(u8), outFile);
            fwrite(&HuC3Shift, 1, sizeof(u8), outFile);
            break;
    }

    fwrite(&sgbMode, 1, sizeof(bool), outFile);
    if (sgbMode) {
        fwrite(&sgbPacketLength, 1, sizeof(int), outFile);
        fwrite(&sgbPacketsTransferred, 1, sizeof(int), outFile);
        fwrite(&sgbPacketBit, 1, sizeof(int), outFile);
        fwrite(&sgbCommand, 1, sizeof(u8), outFile);
        fwrite(&gfxMask, 1, sizeof(u8), outFile);
        fwrite(sgbMap, 1, sizeof(sgbMap), outFile);
    }

    fclose(outFile);
}

int loadState(int num) {
    FILE *inFile;
    StateStruct state;
    char statename[100];
    int version;

    memset(&state, 0, sizeof(StateStruct));

    if (num == -1)
        siprintf(statename, "%s.yss", basename);
    else
        siprintf(statename, "%s.ys%d", basename, num);
    inFile = fopen(statename, "r");

    if (inFile == 0) {
        printConsoleMessage("State doesn't exist.");
        return 1;
    }

    fread(&version, sizeof(int), 1, inFile);

    if (version == 0 || version > STATE_VERSION) {
        printConsoleMessage("State is from an incompatible version.");
        return 1;
    }

    fread((char*)bgPaletteData, 1, sizeof(bgPaletteData), inFile);
    fread((char*)sprPaletteData, 1, sizeof(sprPaletteData), inFile);
    fread((char*)vram, 1, sizeof(vram), inFile);
    fread((char*)wram, 1, sizeof(wram), inFile);
    fread((char*)hram, 1, 0x200, inFile);
    for (int i=0; i<numRamBanks; i++)
        fread((char*)externRam[i], 1, 0x2000, inFile);
    fread((char*)&state, 1, sizeof(StateStruct), inFile);

    /* MBC-specific values have been introduced in v3 */
    if (version >= 3) {
        switch (MBC) {
            case MBC3:
                if (version == 3) {
                    u8 rtcReg;
                    fread(&rtcReg, 1, sizeof(u8), inFile);
                    if (rtcReg != 0)
                        currentRamBank = rtcReg;
                }
                break;
            case HUC3:
                fread(&HuC3Mode,  1, sizeof(u8), inFile);
                fread(&HuC3Value, 1, sizeof(u8), inFile);
                fread(&HuC3Shift, 1, sizeof(u8), inFile);
                break;
        }

        fread(&sgbMode, 1, sizeof(bool), inFile);
        if (sgbMode) {
            fread(&sgbPacketLength, 1, sizeof(int), inFile);
            fread(&sgbPacketsTransferred, 1, sizeof(int), inFile);
            fread(&sgbPacketBit, 1, sizeof(int), inFile);
            fread(&sgbCommand, 1, sizeof(u8), inFile);
            fread(&gfxMask, 1, sizeof(u8), inFile);
            fread(sgbMap, 1, sizeof(sgbMap), inFile);
        }
    }
    else
        sgbMode = false;


    fclose(inFile);
    if (num == -1) {
        unlink(statename);
        suspendStateExists = false;
    }

    gbRegs = state.regs;
    halt = state.halt;
    ime = state.ime;
    doubleSpeed = state.doubleSpeed;
    biosOn = state.biosOn;
    if (!biosExists)
        biosOn = false;
    gbMode = state.gbMode;
    romBank = state.romBank;
    currentRamBank = state.ramBank;
    wramBank = state.wramBank;
    vramBank = state.vramBank;
    memoryModel = state.memoryModel;
    gbClock = state.clock;
    scanlineCounter = state.scanlineCounter;
    timerCounter = state.timerCounter;
    phaseCounter = state.phaseCounter;
    dividerCounter = state.dividerCounter;
    serialCounter = state.serialCounter;
    ramEnabled = state.ramEnabled;
    if (version < 3)
        ramEnabled = true;

    transferReady = false;
    timerPeriod = periods[ioRam[0x07]&0x3];
    cyclesToEvent = 1;

    mapMemory();
    setDoubleSpeed(doubleSpeed);
    refreshGFX();
    refreshSND();


    if (autoSavingEnabled)
        saveGame(); // Synchronize save file on sd with file in ram

    return 0;
}

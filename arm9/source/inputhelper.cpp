#include <nds.h>
#include <fat.h>

#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>

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

FILE* romFile=NULL;
char filename[100];
char savename[100];
char basename[100];
char romTitle[20];

char* romPath = NULL;
char* biosPath = NULL;

// Values taken from the cartridge header
u8 ramSize;
u8 mapperNumber;
u8 cgbFlag;
u8 romSize;
bool hasRumble;

int keysPressed=0;
int lastKeysPressed=0;
int keysForceReleased=0;
int repeatStartTimer=0;
int repeatTimer=0;

bool advanceFrame=false;

u8 romBankSlots[MAX_LOADED_ROM_BANKS][0x4000];
int bankSlotIDs[MAX_ROM_BANKS];
std::vector<int> lastBanksUsed;

bool suspendStateExists;

void initInput()
{
    fatInitDefault();
    chdir("/lameboy"); // Default rom directory
}

enum {
    KEY_NONE,
    KEY_GB_A, KEY_GB_B, KEY_GB_LEFT, KEY_GB_RIGHT, KEY_GB_UP, KEY_GB_DOWN, KEY_GB_START, KEY_GB_SELECT,
    KEY_MENU, KEY_SAVE, KEY_AUTO_GB_A, KEY_AUTO_GB_B, KEY_FAST_FORWARD
};
const int NUM_GB_KEYS = 14;
const char* gbKeyNames[] = {"-","A","B","Left","Right","Up","Down","Start","Select",
    "Menu","Save","Autofire A","Autofire B", "Fast Forward"};
const char* dsKeyNames[] = {"A","B","Select","Start","Right","Left","Up","Down",
    "R","L","X","Y"};
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
    fprintf(file, "config=%d\n", selectedKeyConfig);
    for (unsigned int i=0; i<keyConfigs.size(); i++) {
        fprintf(file, "(%s)\n", keyConfigs[i].name);
        for (int j=0; j<12; j++) {
            fprintf(file, "%s=%s\n", dsKeyNames[j], gbKeyNames[keyConfigs[i].gbKeys[j]]);
        }
    }
}

void startKeyConfigChooser() {
    bool quit=false;
    int option=-1;
    while (!quit) {
        KeyConfig* config = &keyConfigs[selectedKeyConfig];
        consoleClear();
        printf("Config: ");
        if (option == -1)
            printf("* %s *\n\n", config->name);
        else
            printf("  %s  \n\n", config->name);

        printf("       Button   Function\n\n");

        for (int i=0; i<12; i++) {
            int len = 11-strlen(dsKeyNames[i]);
            while (len > 0) {
                printf(" ");
                len--;
            }
            if (option == i) 
                printf("* %s | %s *\n", dsKeyNames[i], gbKeyNames[config->gbKeys[i]]);
            else
                printf("  %s | %s  \n", dsKeyNames[i], gbKeyNames[config->gbKeys[i]]);
        }
        printf("\n\n\n\nPress X to make a new config.");
        if (selectedKeyConfig != 0) /* can't erase the default */ {
            printf("\n\nPress Y to delete this config.");
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
                sprintf(name, "Custom %d", keyConfigs.size()-1);
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
                    selectedKeyConfig--;
                    if (selectedKeyConfig < 0)
                        selectedKeyConfig = keyConfigs.size()-1;
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
        fprintf(file, "biosfile=\n");
    else
        fprintf(file, "biosfile=%s\n", biosPath);
    if (romPath == 0)
        fprintf(file, "rompath=\n");
    else
        fprintf(file, "rompath=%s\n", romPath);
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
        while (*line != '\0' && (c = line[strlen(line)-1]) == ' ' || c == '\n' || c == '\r')
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
    fprintf(file, "[general]\n");
    generalPrintConfig(file);
    fprintf(file, "[console]\n");
    consolePrintConfig(file);
    fprintf(file, "[controls]\n");
    controlsPrintConfig(file);
    fclose(file);

    char nameBuf[100];
    sprintf(nameBuf, "%s.cht", basename);
    saveCheats(nameBuf);
}



void loadBios(const char* filename) {
    FILE* file = fopen(filename, "rb");
    biosExists = file != NULL;
    if (biosExists)
        fread(bios, 1, 0x900, file);
    else
        biosEnabled = false;
}

int loadProgram(char* f)
{
    if (romFile != NULL)
        fclose(romFile);
    strcpy(filename, f);

    romFile = fopen(filename, "rb");
    if (romFile == NULL)
    {
        printLog("Error opening %s.\n", filename);
        return 1;
    }

    // First calculate the size
    fseek(romFile, 0, SEEK_END);
    numRomBanks = ftell(romFile)/0x4000;

    rewind(romFile);

    for (int i=0; i<numRomBanks; i++) {
        bankSlotIDs[i] = -1;
    }

    int banksToLoad = numRomBanks;
    if (numRomBanks > MAX_LOADED_ROM_BANKS)
        banksToLoad = MAX_LOADED_ROM_BANKS;

    lastBanksUsed = std::vector<int>();
    for (int i=0; i<banksToLoad; i++)
    {
        rom[i] = romBankSlots[i];
        bankSlotIDs[i] = i;
        fread(rom[i], 1, 0x4000, romFile);
        if (i != 0)
            lastBanksUsed.push_back(i);
        applyGGCheats(i);
    }

    strcpy(basename, filename);
    *(strrchr(basename, '.')) = '\0';
    strcpy(savename, basename);
    strcat(savename, ".sav");

    cgbFlag = rom[0][0x143];
    mapperNumber = rom[0][0x147];
    romSize = rom[0][0x148];
    ramSize = rom[0][0x149];

    int nameLength = 16;
    if (cgbFlag == 0x80 || cgbFlag == 0xc0)
        nameLength = 15;
    for (int i=0; i<nameLength; i++) {
        romTitle[i] = (char)rom[0][i+0x134];
    }
    romTitle[nameLength] = '\0';

    if (mapperNumber == 0)
        MBC = 0;
    else if (mapperNumber <= 3)
        MBC = 1;
    else if (mapperNumber >= 5 && mapperNumber <= 6)
        MBC = 2;
    else if (mapperNumber == 0x10 || mapperNumber == 0x12 || mapperNumber == 0x13)
        MBC = 3;
    else if (mapperNumber >= 0x19 && mapperNumber <= 0x1E)
        MBC = 5;
    else {
        printLog("Unknown MBC: %.2x\nDefaulting to MBC5\n", mapperNumber);
        MBC = 5;
    }

    hasRumble = false;

    if (mapperNumber >= 0x1C && mapperNumber <= 0x1E)
        hasRumble = true;

    // Little hack to preserve "quickread" from gbcpu.cpp.
    if (biosExists) {
        for (int i=0x100; i<0x150; i++)
            bios[i] = rom[0][i];
    }

    char nameBuf[100];
    sprintf(nameBuf, "%s.yss", basename);
    FILE* stateFile = fopen(nameBuf, "r");
    suspendStateExists = stateFile;
    if (stateFile)
        fclose(stateFile);

    loadSave();

    sprintf(nameBuf, "%s.cht", basename);
    loadCheats(nameBuf);

    cyclesToEvent = 1;
    return 0;
}

void loadRomBank() {
    if (numRomBanks <= MAX_LOADED_ROM_BANKS || currentRomBank == 0 || bankSlotIDs[currentRomBank] != -1)
        return;
    int bankToUnload = lastBanksUsed.back();
    lastBanksUsed.pop_back();
    int slot = bankSlotIDs[bankToUnload];
    bankSlotIDs[bankToUnload] = -1;
    bankSlotIDs[currentRomBank] = slot;
    rom[currentRomBank] = romBankSlots[slot];

    fseek(romFile, 0x4000*currentRomBank, SEEK_SET);
    fread(rom[currentRomBank], 1, 0x4000, romFile);

    lastBanksUsed.insert(lastBanksUsed.begin(), currentRomBank);

    applyGGCheats(currentRomBank);
}

bool bankLoaded(int bank) {
    return bankSlotIDs[bank] != -1;
}

int loadSave()
{
    // unload previous save
    if (externRam != NULL) {
        for (int i=0; i<numRamBanks; i++) {
            free(externRam[i]);
        }
        free(externRam);
    }
    externRam = NULL;

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
        case 4:
            numRamBanks = 16;
            break;
        default:
            printLog("Invalid RAM bank number: %x\nDefaulting to 4 banks\n", ramSize);
            numRamBanks = 4;
            break;
    }
    if (MBC == 2)
        numRamBanks = 1;

    if (numRamBanks == 0)
        return 0;

    externRam = (u8**)malloc(numRamBanks*sizeof(u8*));
    int i;
    for (i=0; i<numRamBanks; i++)
    {
        externRam[i] = (u8*)malloc(0x2000*sizeof(u8));
    }

    // Now load the data.
    FILE* file;
    file = fopen(savename, "r");
    if (file != NULL)
    {
        for (i=0; i<numRamBanks; i++)
            fread(externRam[i], 1, 0x2000, file);
        if (MBC == 3)
        {
            fread(&gbClock.clockSeconds, 1, sizeof(int)*10+sizeof(time_t), file);
        }
        fclose(file);
    }
    else
    {
        printLog("Couldn't open file \"%s\".\n", savename);
        return 1;
    }
    return 0;
}

int saveGame()
{
    if (numRamBanks == 0)
        return 0;
    FILE* file;
    file = fopen(savename, "w");
    if (file != NULL)
    {
        int i;
        for (i=0; i<numRamBanks; i++)
            fwrite(externRam[i], 1, 0x2000, file);
        if (MBC == 3)
        {
            fwrite(&gbClock.clockSeconds, 1, sizeof(int)*10+sizeof(time_t), file);
        }
        fclose(file);
    }
    else
    {
        printLog("There was an error while saving.\n");
        return 1;
    }
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

void printRomInfo() {
    consoleClear();
    printf("ROM Title: \"%s\"\n", romTitle);
    if (mapperNumber == 0)
        printf("Cartridge type: %.2x (No MBC)\n", mapperNumber);
    else
        printf("Cartridge type: %.2x (MBC%d)\n", mapperNumber, MBC);
    printf("ROM Size: %.2x (%d banks)\n", romSize, numRomBanks);
    printf("RAM Size: %.2x (%d banks)\n", ramSize, numRamBanks);
    while (true) {
        swiWaitForVBlank();
        readKeys();
        if (keyJustPressed(KEY_A) || keyJustPressed(KEY_B))
            break;
    }
}

int autoFireCounterA=0,autoFireCounterB=0;
int handleEvents()
{
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

    if (keyPressed(keys[KEY_AUTO_GB_A])) {
        if (autoFireCounterA == 0) {
            buttonsPressed &= (0xFF ^ BUTTONA);
            if (!(ioRam[0x00] & 0x20))
                requestInterrupt(JOYPAD);
            autoFireCounterA = 2;
        }
        autoFireCounterA--;
    }
    if (keyPressed(keys[KEY_AUTO_GB_B])) {
        if (autoFireCounterB == 0) {
            buttonsPressed &= (0xFF ^ BUTTONB);
            if (!(ioRam[0x00] & 0x20))
                requestInterrupt(JOYPAD);
            autoFireCounterB = 2;
        }
        autoFireCounterB--;
    }

    if (keyJustPressed(keys[KEY_SAVE]))
        saveGame();
    if (advanceFrame || keyJustPressed(keys[KEY_MENU] | KEY_TOUCH)) {
        advanceFrame = 0;
        return displayConsole();
    }
    fastForwardKey = keyPressed(keys[KEY_FAST_FORWARD]);

    if (yellowHack) {
        if (fastForwardKey || fastForwardMode)
            maxWaitCycles = 800;
        else
            maxWaitCycles = 50;
    }

    return 0;
}

const int STATE_VERSION = 2;

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
    //int dmaSource,dmaDest,dmaMode,dmaLength;
};

void saveState(int num) {
    StateStruct state;

    char statename[100];
    if (num == -1)
        sprintf(statename, "%s.yss", basename);
    else
        sprintf(statename, "%s.ys%d", basename, num);
    FILE* outFile = fopen(statename, "w");

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
    state.romBank = currentRomBank;
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

    fwrite(&STATE_VERSION, sizeof(int), 1, outFile);
    fwrite((char*)bgPaletteData, 1, sizeof(bgPaletteData), outFile);
    fwrite((char*)sprPaletteData, 1, sizeof(sprPaletteData), outFile);
    fwrite((char*)vram, 1, sizeof(vram), outFile);
    fwrite((char*)wram, 1, sizeof(wram), outFile);
    fwrite((char*)hram, 1, 0x200, outFile);
    for (int i=0; i<numRamBanks; i++)
        fwrite((char*)externRam[i], 1, 0x2000, outFile);
    fwrite((char*)&state, 1, sizeof(StateStruct), outFile);

    fclose(outFile);
}

int loadState(int num) {
    StateStruct state;
    memset(&state, 0, sizeof(StateStruct));

    char statename[100];
    if (num == -1)
        sprintf(statename, "%s.yss", basename);
    else
        sprintf(statename, "%s.ys%d", basename, num);
    FILE* inFile = fopen(statename, "r");

    if (inFile == 0) {
        printConsoleMessage("Error opening state file.");
        return 1;
    }

    int version;
    fread(&version, sizeof(int), 1, inFile);

    /*
    if (state.version > STATE_VERSION) {
        printConsoleMessage("State is from a newer GameYob.");
        return 1;
    }
    */
    if (version == 0) {
        printConsoleMessage("State is incompatible.");
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
    currentRomBank = state.romBank;
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

    screenOn = ioRam[0x40] & 0x80;
    transferReady = false;
    timerPeriod = periods[ioRam[0x07]&0x3];
    cyclesToEvent = 1;

    mapMemory();
    setDoubleSpeed(doubleSpeed);
    refreshGFX();
    refreshSND();

    return 0;
}

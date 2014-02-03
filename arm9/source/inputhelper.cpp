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
#include "nifi.h"
#include "gbgfx.h"
#include "gbsnd.h"
#include "cheats.h"
#include "sgb.h"
#include "gbs.h"
#include "common.h"
#include "filechooser.h"

int keysPressed=0;
int lastKeysPressed=0;
int keysForceReleased=0;
int repeatStartTimer=0;
int repeatTimer=0;

u8 buttonsPressed;

char* biosPath = NULL;
char* borderPath = NULL;
char* romPath = NULL;

bool fastForwardMode = false; // controlled by the toggle hotkey
bool fastForwardKey = false;  // only while its hotkey is pressed


void initInput()
{
    fatInit(FAT_CACHE_SIZE, true);
    //fatInitDefault();
}

void flushFatCache() {
    // This involves things from libfat which aren't normally visible
    devoptab_t* devops = (devoptab_t*)GetDeviceOpTab ("sd");
    PARTITION* partition = (PARTITION*)devops->deviceData;
    _FAT_cache_flush(partition->cache); // Flush the cache manually
}


const char* gbKeyNames[] = {"-","A","B","Left","Right","Up","Down","Start","Select",
    "Menu","Menu/Pause","Save","Autofire A","Autofire B", "Fast Forward", "FF Toggle", "Scale","Reset"};
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
        KEY_MENU,KEY_FAST_FORWARD,KEY_SAVE,KEY_SCALE}
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

int keyConfigChooser_option;

void redrawKeyConfigChooser() {
    int& option = keyConfigChooser_option;
    KeyConfig* config = &keyConfigs[selectedKeyConfig];

    consoleClear();

    iprintf("Config: ");
    if (option == -1)
        iprintfColored(CONSOLE_COLOR_LIGHT_YELLOW, "* %s *\n\n", config->name);
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
            iprintfColored(CONSOLE_COLOR_LIGHT_YELLOW, "* %s | %s *\n", dsKeyNames[i], gbKeyNames[config->gbKeys[i]]);
        else
            iprintf("  %s | %s  \n", dsKeyNames[i], gbKeyNames[config->gbKeys[i]]);
    }
    iprintf("\n\n\n\nPress X to make a new config.");
    if (selectedKeyConfig != 0) /* can't erase the default */ {
        iprintf("\n\nPress Y to delete this config.");
    }
}

void updateKeyConfigChooser() {
    bool redraw = false;

    int& option = keyConfigChooser_option;
    KeyConfig* config = &keyConfigs[selectedKeyConfig];

    if (keyJustPressed(KEY_B)) {
        loadKeyConfig();
        closeSubMenu();
    }
    else if (keyJustPressed(KEY_X)) {
        keyConfigs.push_back(KeyConfig(*config));
        selectedKeyConfig = keyConfigs.size()-1;
        char name[32];
        siprintf(name, "Custom %d", keyConfigs.size()-1);
        strcpy(keyConfigs.back().name, name);
        option = -1;
        redraw = true;
    }
    else if (keyJustPressed(KEY_Y)) {
        if (selectedKeyConfig != 0) /* can't erase the default */ {
            keyConfigs.erase(keyConfigs.begin() + selectedKeyConfig);
            if (selectedKeyConfig >= keyConfigs.size())
                selectedKeyConfig = keyConfigs.size() - 1;
            redraw = true;
        }
    }
    else if (keyPressedAutoRepeat(KEY_DOWN)) {
        if (option == 11)
            option = -1;
        else
            option++;
        redraw = true;
    }
    else if (keyPressedAutoRepeat(KEY_UP)) {
        if (option == -1)
            option = 11;
        else
            option--;
        redraw = true;
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
        redraw = true;
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
        redraw = true;
    }
    if (redraw)
        doAtVBlank(redrawKeyConfigChooser);
}

void startKeyConfigChooser() {
    keyConfigChooser_option = -1;
    displaySubMenu(updateKeyConfigChooser);
    redrawKeyConfigChooser();
}

void generalParseConfig(const char* line) {
    char* equalsPos;
    if ((equalsPos = strrchr(line, '=')) != 0 && equalsPos != line+strlen(line)-1) {
        *equalsPos = '\0';
        const char* parameter = line;
        const char* value = equalsPos+1;

        if (strcmpi(parameter, "rompath") == 0) {
            if (romPath != 0)
                free(romPath);
            romPath = (char*)malloc(strlen(value)+1);
            strcpy(romPath, value);
            romChooserState.directory = romPath;
        }
        else if (strcmpi(parameter, "biosfile") == 0) {
            if (biosPath != 0)
                free(biosPath);
            biosPath = (char*)malloc(strlen(value)+1);
            strcpy(biosPath, value);
            gameboy->loadBios(biosPath);
        }
        else if (strcmpi(parameter, "borderfile") == 0) {
            if (borderPath != 0)
                free(borderPath);
            borderPath = (char*)malloc(strlen(value)+1);
            strcpy(borderPath, value);
        }
    }
    if (borderPath == NULL || *borderPath == '\0') {
        free(borderPath);
        borderPath = (char*)malloc(strlen("/border.bmp")+1);
        strcpy(borderPath, "/border.bmp");
    }
}

void generalPrintConfig(FILE* file) {
    if (romPath == 0)
        fiprintf(file, "rompath=\n");
    else
        fiprintf(file, "rompath=%s\n", romPath);
    if (biosPath == 0)
        fiprintf(file, "biosfile=\n");
    else
        fiprintf(file, "biosfile=%s\n", biosPath);
    if (borderPath == 0)
        fiprintf(file, "borderfile=\n");
    else
        fiprintf(file, "borderfile=%s\n", borderPath);
}

bool readConfigFile() {
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
                    configParser = menuParseConfig;
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

    return file != NULL;
}

void writeConfigFile() {
    FILE* file = fopen("/gameyob.ini", "w");
    fiprintf(file, "[general]\n");
    generalPrintConfig(file);
    fiprintf(file, "[console]\n");
    menuPrintConfig(file);
    fiprintf(file, "[controls]\n");
    controlsPrintConfig(file);
    fclose(file);

    char nameBuf[100];
    siprintf(nameBuf, "%s.cht", gameboy->getRomBasename());
    gameboy->getCheatEngine()->saveCheats(nameBuf);
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

int readKeysLastFrameCounter=0;
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

    if (dsFrameCounter != readKeysLastFrameCounter) { // Double-check that it's been 1/60th of a second
        if (repeatStartTimer > 0)
            repeatStartTimer--;
        if (repeatTimer > 0)
            repeatTimer--;
        readKeysLastFrameCounter = dsFrameCounter;
    }
}

void forceReleaseKey(int key) {
    keysForceReleased |= key;
    keysPressed &= ~key;
}

int mapGbKey(int gbKey) {
    return keys[gbKey];
}

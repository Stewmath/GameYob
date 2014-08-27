#include <SDL.h>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>

#include "inputhelper.h"
#include "gameboy.h"
#include "main.h"
#include "console.h"
#include "menu.h"
#include "nifi.h"
#include "gbgfx.h"
#include "soundengine.h"
#include "cheats.h"
#include "gbs.h"
#include "filechooser.h"
#include "romfile.h"

bool keysPressed[512];
bool keysJustPressed[512];

int keysForceReleased=0;
int repeatStartTimer=0;
int repeatTimer=0;

u8 buttonsPressed;

char* biosPath = NULL;
char* borderPath = NULL;
char* romPath = NULL;

bool fastForwardMode = false; // controlled by the toggle hotkey
bool fastForwardKey = false;  // only while its hotkey is pressed

bool biosExists = false;
int rumbleInserted = 0;


void initInput()
{
    memset(keysPressed, 0, sizeof(keysPressed));
}

void flushFatCache() {
}


void controlsParseConfig(const char* line2) {
}
void controlsPrintConfig(FILE* file) {
}

void startKeyConfigChooser() {
}

void generalParseConfig(const char* line) {
    char* equalsPos;
    if ((equalsPos = (char*)strrchr(line, '=')) != 0 && equalsPos != line+strlen(line)-1) {
        *equalsPos = '\0';
        const char* parameter = line;
        const char* value = equalsPos+1;

        if (strcasecmp(parameter, "rompath") == 0) {
            if (romPath != 0)
                free(romPath);
            romPath = (char*)malloc(strlen(value)+1);
            strcpy(romPath, value);
#ifdef DS
            romChooserState.directory = romPath;
#endif
        }
        else if (strcasecmp(parameter, "biosfile") == 0) {
            if (biosPath != 0)
                free(biosPath);
            biosPath = (char*)malloc(strlen(value)+1);
            strcpy(biosPath, value);
        }
        else if (strcasecmp(parameter, "borderfile") == 0) {
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
        fprintf(file, "rompath=\n");
    else
        fprintf(file, "rompath=%s\n", romPath);
    if (biosPath == 0)
        fprintf(file, "biosfile=\n");
    else
        fprintf(file, "biosfile=%s\n", biosPath);
    if (borderPath == 0)
        fprintf(file, "borderfile=\n");
    else
        fprintf(file, "borderfile=%s\n", borderPath);
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
                if (strcasecmp(section, "general") == 0) {
                    configParser = generalParseConfig;
                }
                else if (strcasecmp(section, "console") == 0) {
                    configParser = menuParseConfig;
                }
                else if (strcasecmp(section, "controls") == 0) {
                    configParser = controlsParseConfig;
                }
            }
        }
        else
            configParser(line);
    }
    fclose(file);
end:

    return file != NULL;
}

void writeConfigFile() {
    FILE* file = fopen("/gameyob.ini", "w");
    fprintf(file, "[general]\n");
    generalPrintConfig(file);
    fprintf(file, "[console]\n");
    menuPrintConfig(file);
    fprintf(file, "[controls]\n");
    controlsPrintConfig(file);
    fclose(file);

    char nameBuf[256];
    sprintf(nameBuf, "%s.cht", gameboy->getRomFile()->getBasename());
    gameboy->getCheatEngine()->saveCheats(nameBuf);
}


bool keyPressed(int key) {
    return keysPressed[key];
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
    return keysJustPressed[key];
}

int readKeysLastFrameCounter=0;
void readKeys() {
    /*
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
    */
}

void forceReleaseKey(int key) {
}

int mapFuncKey(int funcKey) {
    switch(funcKey) {
        case FUNC_KEY_NONE:
            return 0;
        case FUNC_KEY_A:
            return SDLK_SEMICOLON;
        case FUNC_KEY_B:
            return SDLK_q;
        case FUNC_KEY_LEFT:
            return SDLK_LEFT;
        case FUNC_KEY_RIGHT:
            return SDLK_RIGHT;
        case FUNC_KEY_UP:
            return SDLK_UP;
        case FUNC_KEY_DOWN:
            return SDLK_DOWN;
        case FUNC_KEY_START:
            return SDLK_RETURN;
        case FUNC_KEY_SELECT:
            return SDLK_BACKSLASH;
        case FUNC_KEY_MENU:
            return SDLK_o;
        case FUNC_KEY_MENU_PAUSE:
            return 0;
        case FUNC_KEY_SAVE:
            return 0;
        case FUNC_KEY_AUTO_A:
            return 0;
        case FUNC_KEY_AUTO_B:
            return 0;
        case FUNC_KEY_FAST_FORWARD:
            return SDLK_SPACE;
        case FUNC_KEY_FAST_FORWARD_TOGGLE:
            return 0;
        case FUNC_KEY_SCALE:
            return 0;
        case FUNC_KEY_RESET:
            return 0;
    }
}

int mapMenuKey(int menuKey) {
    return 0;
}

void inputUpdateVBlank() {
    memset(keysJustPressed, 0, sizeof(keysJustPressed));

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
            case SDL_QUIT:
                gameboy->saveGame();
#ifdef LOG
				fclose(logFile);
#endif
                exit(0);
				break;
			case SDL_KEYDOWN:
                keysPressed[event.key.keysym.sym] = true;
                keysJustPressed[event.key.keysym.sym] = true;
				break;
			case SDL_KEYUP:
                keysPressed[event.key.keysym.sym] = false;
				break;
		}
	}
}

void doRumble(bool rumbleVal)
{
}

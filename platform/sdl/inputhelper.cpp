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
#include "io.h"
#include "gbmanager.h"

bool keysPressed[512];
bool keysJustPressed[512];

int keysForceReleased=0;
int repeatStartTimer=0;
int repeatTimer=0;

u8 buttonsPressed;

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

void inputUpdateVBlank() {
}

void system_doRumble(bool rumbleVal)
{
}

int system_getMotionSensorX() {
    return 0;
}
int system_getMotionSensorY() {
    return 0;
}

void system_checkPolls() {
    memset(keysJustPressed, 0, sizeof(keysJustPressed));

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
            case SDL_QUIT:
                mgr_save();
                mgr_exit();
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

void system_waitForVBlank() {

}

void system_cleanup() {
    SDL_Quit();
}

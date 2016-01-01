extern "C" {
#include "libfat/cache.h"
#include "libfat/partition.h"
}

#include <nds.h>
#include <fat.h>
#include "common.h"

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
#include "gbmanager.h"

int keysPressed=0;
int lastKeysPressed=0;
int keysForceReleased=0;
int repeatStartTimer=0;
int repeatTimer=0;

u8 buttonsPressed;

bool fastForwardMode = false; // controlled by the toggle hotkey
bool fastForwardKey = false;  // only while its hotkey is pressed

bool biosExists = false;
int rumbleInserted = 0;


touchPosition touchData;

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

void forceReleaseKey(int key) {
    keysForceReleased |= key;
    keysPressed &= ~key;
}

int readKeysLastFrameCounter=0;
void inputUpdateVBlank() {
    scanKeys();
    touchRead(&touchData);

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

void system_doRumble(bool rumbleVal)
{
    if (rumbleInserted == 1)
    {
        setRumble(rumbleVal);
    }
    else if (rumbleInserted == 2)
    {
        GBA_BUS[0x1FE0000/2] = 0xd200;
        GBA_BUS[0x0000000/2] = 0x1500;
        GBA_BUS[0x0020000/2] = 0xd200;
        GBA_BUS[0x0040000/2] = 0x1500;
        GBA_BUS[0x1E20000/2] = rumbleVal ? (0xF0 + rumbleStrength) : 0x08;
        GBA_BUS[0x1FC0000/2] = 0x1500;
    }
}

#define MOTION_SENSOR_RANGE 128

int system_getMotionSensorX() {
    int px = touchData.px;
    if (!keyPressed(KEY_TOUCH))
        px = 128;

    double val = (128 - px) * ((double)MOTION_SENSOR_RANGE / 256)
        + MOTION_SENSOR_MID;
    /*
    if (val < 0)
        return (-(int)val) | 0x8000;
        */
    return (int)val + 0x8000;
}

int system_getMotionSensorY() {
    int py = touchData.py;
    if (!keyPressed(KEY_TOUCH))
        py = 96;

    double val = (96 - py) * ((double)MOTION_SENSOR_RANGE / 192)
        + MOTION_SENSOR_MID - 80;
    return (int)val;
}

void system_checkPolls() {

}

void system_waitForVBlank() {
    swiWaitForVBlank();
}

void system_cleanup() {
    mgr_save();
    mgr_exit();
}

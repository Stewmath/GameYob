#pragma once
#include <stdio.h>
#include "io.h"
#include "config.h"

#define FAT_CACHE_SIZE 16

#define GB_A		    0x01
#define GB_B		    0x02
#define GB_SELECT		0x04
#define GB_START		0x08
#define GB_RIGHT		0x10
#define GB_LEFT		    0x20
#define GB_UP			0x40
#define GB_DOWN		    0x80


/*
#define MOTION_SENSOR_MAX 2197
#define MOTION_SENSOR_MIN 1897
*/
#define MOTION_SENSOR_RANGE 256
#define MOTION_SENSOR_MID 0


extern bool fastForwardMode; // controlled by the toggle hotkey
extern bool fastForwardKey;  // only while its hotkey is pressed
extern u8 buttonsPressed;

extern char borderPath[MAX_FILENAME_LEN];
extern bool biosExists;
extern int rumbleInserted;


void initInput();
void flushFatCache();

bool keyPressed(int key);
bool keyPressedAutoRepeat(int key);
bool keyJustPressed(int key);
// Consider this key unpressed until released and pressed again
void forceReleaseKey(int key);

void inputUpdateVBlank();

void system_doRumble(bool rumbleVal);
int system_getMotionSensorX();
int system_getMotionSensorY();

void system_checkPolls();
void system_waitForVBlank();

#pragma once
#include "global.h"

extern int scanlineCounter;
extern int phaseCounter;
extern int dividerCounter;
extern int timerCounter;
extern int serialCounter;
extern int timerPeriod;
extern long periods[4];

extern int doubleSpeed;
extern int cyclesToEvent;

extern bool fastForwardMode;
extern bool fastForwardKey;

extern bool fpsOutput;
extern bool timeOutput;

extern int gbMode;
extern bool sgbMode;

extern int cyclesSinceVblank;
extern bool probingForBorder;
extern int interruptTriggered;
extern int gameboyFrameCounter;

void setEventCycles(int cycles);

void gameboyCheckInput();
void gameboyUpdateVBlank();
void gameboyAutosaveCheck();
void gameboySyncAutosave();

void resetGameboy(); // This may be called even from the context of "runOpcode".
void pauseGameboy();
void unpauseGameboy();
bool isGameboyPaused();
void runEmul();
void initLCD();
void initGameboyMode();
void checkLYC();
void updateLCD(int cycles);
void updateTimers(int cycles);
void requestInterrupt(int id);
void setDoubleSpeed(int val);

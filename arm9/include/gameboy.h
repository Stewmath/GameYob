#pragma once
#include "global.h"

#define min(a,b) (a<b?a:b)

//#define LOG

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

void resetGameboy(); // This may be called even from the context of "runOpcode".

void setEventCycles(int cycles);
void runEmul();
void initLCD();
void initGameboyMode();
void checkLYC();
void updateLCD(int cycles);
void updateTimers(int cycles);
void requestInterrupt(int id);
void setDoubleSpeed(int val);

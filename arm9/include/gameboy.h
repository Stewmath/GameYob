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
extern int maxWaitCycles;

extern bool fastForwardMode;
extern bool fastForwardKey;

extern bool yellowHack;

extern bool fpsOutput;
extern bool timeOutput;

extern int gbMode;
extern bool sgbMode;

extern bool probingForBorder;

void resetGameboy(); // This may be called even from the context of "runOpcode".

void runEmul();
void initLCD();
void initGameboyMode();
void updateLCD(int cycles);
void updateTimers(int cycles);
void handleInterrupts();
void requestInterrupt(int id);
void setDoubleSpeed(int val);

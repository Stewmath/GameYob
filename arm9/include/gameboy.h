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

extern bool screenOn;

extern int mode2Cycles;
extern int mode3Cycles;

extern int doubleSpeed;
extern int cyclesToEvent;

extern bool screenOn;

extern int turbo;
extern bool fpsOutput;
extern bool timeOutput;

void runEmul();
void initLCD();
void updateLCD(int cycles);
void updateTimers(int cycles);
void handleInterrupts();
void requestInterrupt(int id);
void setDoubleSpeed(int val);

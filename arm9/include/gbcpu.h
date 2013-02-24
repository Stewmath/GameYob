#pragma once
#include "global.h"

#define GB			0
#define CGB			1

typedef union
{
    u16 w;
    struct B
    {
        u8 l;
        u8 h;
    } b;
} Register;


//extern Register af,bc,de,hl;
extern u16 gbSP,gbPC;
extern int fps;
extern int halt;
extern int ime;
extern int gbMode;
extern int totalCycles;
extern int cyclesToExecute;

void initCPU();
void enableInterrupts();
void disableInterrupts();
void handleInterrupts();
int runOpcode(int cycles);

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

struct Registers
{
    Register sp; /* Stack Pointer */
    Register pc; /* Program Counter */
    Register af;
    Register bc;
    Register de;
    Register hl;
};

extern struct Registers gbRegs;
extern int fps;
extern int halt;
extern int ime;
extern int extraCycles;
extern int cyclesToExecute;

void initCPU();
void enableInterrupts();
void disableInterrupts();
int handleInterrupts(unsigned int interruptTriggered);
int runOpcode(int cycles);

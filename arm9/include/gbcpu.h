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
    u16      sp; /* Stack Pointer */
    u16      pc; /* Program Counter */
    Register af;
    Register bc;
    Register de;
    Register hl;
};

extern struct Registers gbRegs;
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

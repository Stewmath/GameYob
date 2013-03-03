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

void initCPU();
void enableInterrupts();
void disableInterrupts();
void handleInterrupts();

extern "C" {
    // ASM functions which use C syntax.
    // C++ syntax, when compiled, is weird and hard to predict?
    void initASM(int a,int b);
    int runCpuASM(int cycles);

    // C++ functions
    void cheat(int opcode);
    void cheatcb(int opcode);
}
extern int cyclesToExecute;

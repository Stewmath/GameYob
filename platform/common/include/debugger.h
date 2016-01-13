#pragma once
class Gameboy;
struct Registers;

extern int debugMode;

extern int breakpointAddr;
extern int readWatchAddr;
extern int readWatchBank;
extern int writeWatchAddr;
extern int writeWatchBank;

void startDebugger();
void stopDebugger();
int runDebugger(Gameboy* gameboy, const Registers& regs);

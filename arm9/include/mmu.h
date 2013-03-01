#pragma once
#include "time.h"
#include <stdio.h>
#include "global.h"

struct clockStruct
{
    int clockSeconds;
    int clockMinutes;
    int clockHours;
    int clockDays;
    int clockControl;
    int clockSecondsL;
    int clockMinutesL;
    int clockHoursL;
    int clockDaysL;
    int clockControlL;
    time_t clockLastTime;
};

extern clockStruct gbClock;

extern int watchAddr;
extern int readWatchAddr;
extern int bankWatchAddr;

// Functions which interact with assembly should have "extern C" in their 
// declarations.
extern "C" {
    u8 readMemory(u16 addr);
    void writeMemory(u16 addr, u8 val);
    void writehword(u16 addr, u16 val);
    u16 readhword(u16 addr);
}

bool updateHblankDMA();
void latchClock();
void initMMU();

extern int numRomBanks;
extern int numRamBanks;

// whether the bios exists and has been loaded
extern bool biosExists;
// whether the bios should be used
extern bool biosEnabled;
// whether the bios is mapped to memory
extern bool biosOn;

extern u8 bios[0x900];

// memory[x][yyy] = ram value at xyyy
extern u8* memory[0x10];

extern u8* rom[];
extern u8 vram[2][0x2000];
extern u8** externRam;
extern u8 wram[8][0x1000];
extern u8 hram[];
extern u8* ioRam;
extern u8 spriteData[];
extern int wramBank;
extern int vramBank;

extern int MBC;
extern int memoryModel;
extern bool hasClock;
extern int currentRomBank;
extern int currentRamBank;

extern u16 dmaSource;
extern u16 dmaDest;
extern u16 dmaLength;
extern int dmaMode;

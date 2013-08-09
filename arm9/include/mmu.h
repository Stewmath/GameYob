#pragma once
#include "time.h"
#include <stdio.h>
#include "global.h"


// Be careful changing this; it affects save state compatibility.
struct clockStruct
{
    union {
        struct {
            int s, m, h, d, ctrl;
        } mbc3;
        struct {
            int m, d, y;
            int u[2]; /* Unused */
        } huc3;
    };
    int latch[5]; /* For VBA compatibility */
    time_t last;
};

extern clockStruct gbClock;

void initMMU();
void mapMemory();
u8 readMemory(u16 addr);
u16 readMemory16(u16 addr);
u8 readIO(u8 ioReg);
void writeMemory(u16 addr, u8 val);
void writeIO(u8 ioReg, u8 val);

bool updateHblankDMA();
void latchClock();

extern int numRomBanks;
extern int numRamBanks;
extern bool hasRumble;
extern int rumbleStrength;
extern int rumbleInserted;

void doRumble(bool rumbleVal);

extern bool ramEnabled;
extern char rtcReg;
extern u8   HuC3Mode;
extern u8   HuC3Value;
extern u8   HuC3Shift;

extern int resultantGBMode;

// whether the bios exists and has been loaded
extern bool biosExists;
// how/when the bios should be used
extern int biosEnabled;
// whether the bios is mapped to memory
extern bool biosOn;

extern u8 bios[0x900];

// memory[x][yyy] = ram value at xyyy
extern u8* memory[0x10];

extern u8 vram[2][0x2000];
extern u8** externRam;
extern u8 wram[8][0x1000];
extern u8* const hram;
extern u8* const ioRam;
extern u8 spriteData[];
extern int wramBank;
extern int vramBank;

extern int MBC;
extern int memoryModel;
extern bool hasClock;
extern int romBank;
extern int currentRamBank;

extern u16 dmaSource;
extern u16 dmaDest;
extern u16 dmaLength;
extern int dmaMode;

extern bool saveModified;
extern int autosaveStart;
extern int autosaveEnd;
extern int numSaveWrites;

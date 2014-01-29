#pragma once
#include <stdio.h>
#include "time.h"
#ifdef DS
#include <nds.h>
#endif

#define MAX_SRAM_SIZE   0x20000

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
    int latch[5]; /* For VBA/Lameboy compatibility */
    time_t last;
};


class Gameboy {
    public:
        // gameboy.cpp

        void setEventCycles(int cycles);

        void gameboyCheckInput();
        void gameboyUpdateVBlank();
        void gameboyAutosaveCheck();

        void resetGameboy(); // This may be called even from the context of "runOpcode".
        void pauseGameboy();
        void unpauseGameboy();
        bool isGameboyPaused();
        void runEmul();
        void initLCD();
        void initGameboyMode();
        void checkLYC();
        void updateLCD(int cycles);
        void updateTimers(int cycles);
        void requestInterrupt(int id);
        void setDoubleSpeed(int val);

        int loadRom(char* filename);
        void unloadRom();
        int loadSave();
        int saveGame();
        void gameboySyncAutosave();
        void updateAutosave();


        void saveState(int num);
        int loadState(int num);
        void deleteState(int num);
        bool checkStateExists(int num);

        // mmu.cpp

        void initMMU();
        void mapMemory();
        u8 readMemory(u16 addr) ITCM_CODE;
        u16 readMemory16(u16 addr);
        u8 readIO(u8 ioReg) ITCM_CODE;
        void writeMemory(u16 addr, u8 val) ITCM_CODE;
        void writeIO(u8 ioReg, u8 val) ITCM_CODE;
        void refreshP1();

        bool updateHblankDMA();
        void latchClock();

        void doRumble(bool rumbleVal);

        // mbc functions (in mmu.cpp)
        u8 h3r(u16 addr);
        u8 m3r(u16 addr);

        void m0w (u16 addr, u8 val);
        void m2w(u16 addr, u8 val);
        void m3w(u16 addr, u8 val);
        void m1w (u16 addr, u8 val);
        void h1w(u16 addr, u8 val);
        void m5w (u16 addr, u8 val);
        void h3w (u16 addr, u8 val);


        // variables
        int scanlineCounter;
        int phaseCounter;
        int dividerCounter;
        int timerCounter;
        int serialCounter;
        int timerPeriod;
        long periods[4];

        int doubleSpeed;

        bool fastForwardMode; // controlled by the toggle hotkey
        bool fastForwardKey;  // only while its hotkey is pressed

        bool fpsOutput;
        bool timeOutput;

        int gbMode;
        bool sgbMode;

        int cyclesToEvent;
        int cyclesSinceVblank;
        bool probingForBorder;
        int interruptTriggered;
        int gameboyFrameCounter;

        // mmu variables

        clockStruct gbClock;

        int numRomBanks;
        int numRamBanks;
        bool hasRumble;
        int rumbleStrength;

        // MBC flags
        bool ramEnabled;
        char rtcReg;
        u8   HuC3Mode;
        u8   HuC3Value;
        u8   HuC3Shift;

        int resultantGBMode;

        // whether the bios is mapped to memory
        bool biosOn;

        u8 bios[0x900];

        // memory[x][yyy] = ram value at xyyy
        u8* memory[0x10];

        u8 vram[2][0x2000];
        u8* externRam;
        u8 wram[8][0x1000];

        u8 highram[0x1000];

        u8 spriteData[];
        int wramBank;
        int vramBank;

        int MBC;
        int memoryModel;
        bool hasClock;
        int romBank;
        int currentRamBank;

        u16 dmaSource;
        u16 dmaDest;
        u16 dmaLength;
        int dmaMode;

        bool saveModified;
        bool dirtySectors[MAX_SRAM_SIZE/512];
        int numSaveWrites;
        bool autosaveStarted;

    private:
        // mmu functions
        void refreshRomBank(int bank);
        void refreshRamBank(int bank);
        void handleHuC3Command(u8 command);
        void writeSram(u16 addr, u8 val);
        void writeClockStruct();


        // variables
        volatile bool gameboyPaused;
        int fps;
        bool resettingGameboy;

        bool wroteToSramThisFrame;
        int framesSinceAutosaveStarted;

        bool rockmanMapper;

        void (*writeFunc)(u16, u8);
        u8 (*readFunc)(u16);

        // mmu variables
        bool nifiTryAgain;

};

#define hram highram+0xe00
#define ioRam hram+0x100

typedef void (Gameboy::*mbcWrite)(u16,u8);
typedef u8   (Gameboy::*mbcRead )(u16);

const mbcRead mbcReads[] = {
    NULL, NULL, NULL, &Gameboy::m3r, NULL, NULL, NULL, &Gameboy::h3r, NULL
};
const mbcWrite mbcWrites[] = {
    &Gameboy::m0w, &Gameboy::m1w, &Gameboy::m2w, &Gameboy::m3w, NULL, &Gameboy::m5w, NULL, &Gameboy::h3w, &Gameboy::h1w
};


extern bool biosExists;

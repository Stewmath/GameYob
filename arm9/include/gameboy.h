#pragma once
#include <stdio.h>
#include <vector>
#include "time.h"
#include "gbgfx.h"

#ifdef DS
#include <nds.h>
#endif

#define MAX_SRAM_SIZE   0x20000

#define GB			0
#define CGB			1

class CheatEngine;
class SoundEngine;
class RomFile;

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

// Cpu stuff
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

// IMPORTANT: This is unchanging, it DOES NOT change in double speed mode!
#define clockSpeed 4194304


class Gameboy {
    public:
        // gameboy.cpp

        Gameboy();
        void init();
        void initGBMode();
        void initGBCMode();
        void initSND();

        inline void setEventCycles(int cycles) {
            if (cycles < cyclesToEvent) {
                cyclesToEvent = cycles;
                /*
                   if (cyclesToEvent <= 0) {
                   cyclesToEvent = 1;
                   }
                   */
            }
        }


        void gameboyCheckInput();
        void gameboyUpdateVBlank();
        void gameboyAutosaveCheck();

        void resetGameboy(); // This may be called even from the context of "runOpcode".
        void pause();
        void unpause();
        bool isGameboyPaused();
        int runEmul() ITCM_CODE;
        void initGameboyMode();
        void checkLYC();
        int updateLCD(int cycles) ITCM_CODE;
        void updateTimers(int cycles) ITCM_CODE;
        void requestInterrupt(int id);
        void setDoubleSpeed(int val);

        void setRomFile(RomFile* r);
        void unloadRom();
        void printRomInfo();
        bool isRomLoaded();

        int loadSave();
        int saveGame();
        void gameboySyncAutosave();
        void updateAutosave();

        void saveState(int num);
        int loadState(int num);
        void deleteState(int num);
        bool checkStateExists(int num);

        inline int getCyclesSinceVblank() { return cyclesSinceVblank + extraCycles; }
        inline bool isDoubleSpeed() { return doubleSpeed; }

        inline CheatEngine* getCheatEngine() { return cheatEngine; }
        inline SoundEngine* getSoundEngine() { return soundEngine; }
        inline RomFile* getRomFile() { return romFile; }

        // gbcpu.cpp
        void initCPU();
        void enableInterrupts();
        void disableInterrupts();
        int handleInterrupts(unsigned int interruptTriggered);
        int runOpcode(int cycles) ITCM_CODE;

        inline u8 quickRead(u16 addr) { return memory[addr>>12][addr&0xFFF]; }
        inline u8 quickReadIO(u8 addr) { return ioRam[addr]; }
        inline u16 quickRead16(u16 addr) { return quickRead(addr)|(quickRead(addr+1)<<8); }
        // Currently unused because this can actually overwrite the rom, in rare cases
        inline void quickWrite(u16 addr, u8 val) { memory[addr>>12][addr&0xFFF] = val; }


        // mmu.cpp

        void initMMU();
        void mapMemory();
//        u8 readMemory(u16 addr) ITCM_CODE;
        u8 readMemoryFast(u16 addr) ITCM_CODE;
        u16 readMemory16(u16 addr);
        u8 readIO(u8 ioReg) ITCM_CODE;
//        void writeMemory(u16 addr, u8 val) ITCM_CODE;
        void writeIO(u8 ioReg, u8 val) ITCM_CODE;
        void refreshP1();
        u8 readMemoryOther(u16 addr);
        void writeMemoryOther(u16 addr, u8 val);

inline u8 readMemory(u16 addr)
{
    int area = addr>>12;
    if (!(area & 0x8) || area == 0xc || area == 0xd) {
        return memory[area][addr&0xfff];
    }
    else
        return readMemoryOther(addr);
}
inline void writeMemory(u16 addr, u8 val)
{
    int area = addr>>12;
    if (area == 0xc) {
        // Checking for this first is a tiny bit more efficient.
        wram[0][addr&0xfff] = val;
        return;
    }
    else if (area == 0xd) {
        wram[wramBank][addr&0xfff] = val;
        return;
    }
    writeMemoryOther(addr, val);
}



        bool updateHblankDMA();
        void latchClock();

        inline int getNumRamBanks() { return numRamBanks; }
        inline u8 getWramBank() { return wramBank; }
        inline void setWramBank(u8 bank) { wramBank = bank; }

        inline void setSoundChannel(int which) {ioRam[0x26] |= which;}
        inline void clearSoundChannel(int which) {ioRam[0x26] &= ~which;}

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

        void writeSaveFileSectors(int startSector, int numSectors);
        u8 controllers[4];


        // variables
        int scanlineCounter;
        int phaseCounter;
        int dividerCounter;
        int timerCounter;
        int serialCounter;
        int timerPeriod;
        long periods[4];

        int doubleSpeed;

        int gbMode;
        bool sgbMode;

        int cyclesToEvent;
        int cyclesSinceVblank;
        int interruptTriggered;
        int gameboyFrameCounter;

        // mmu variables

        clockStruct gbClock;

        int numRamBanks;
        int rumbleValue;
        int lastRumbleValue;

        // MBC flags
        bool ramEnabled;
        char rtcReg;
        u8   HuC3Mode;
        u8   HuC3Value;
        u8   HuC3Shift;

        int resultantGBMode;

        // whether the bios is mapped to memory
        bool biosOn;

        // memory[x][yyy] = ram value at xyyy
        u8* memory[0x10];

        u8 vram[2][0x2000];
        u8* externRam;
        u8 wram[8][0x1000];

        u8 highram[0x1000];
        u8* hram;
        u8* ioRam;

        int wramBank;
        int vramBank;

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

        // cpu variables
        // TODO: try dtcm
        int halt;
        int ime;
        int extraCycles;
        int soundCycles;
        int cyclesToExecute;
        int cyclesUntilSwap;
//        struct Registers gbRegs;
        
        int fps;

    private:
        CheatEngine* cheatEngine;
        SoundEngine* soundEngine;
        RomFile* romFile;

        FILE* saveFile;
        char savename[256];

        // mmu functions
        void refreshRomBank(int bank);
        void refreshRamBank(int bank);
        void handleHuC3Command(u8 command);
        void writeSram(u16 addr, u8 val);
        void writeClockStruct();


        // variables
        volatile bool gameboyPaused;
        bool resettingGameboy;

        bool wroteToSramThisFrame;
        int framesSinceAutosaveStarted;

        bool rockmanMapper;

        void (Gameboy::*writeFunc)(u16, u8);
        u8 (Gameboy::*readFunc)(u16);

        bool suspendStateExists;

        int saveFileSectors[MAX_SRAM_SIZE/512];

        // sgb.cpp
    public:
        void initSGB();
        void sgbHandleP1(u8 val);
        u8 sgbReadP1();
        void sgbSetActiveController(u8 controller) { sgbActiveController = controller; }
        inline u8 sgbGetActiveController() { return sgbActiveController; }

        u8 sgbMap[20*18];

        // SGB packet commands
        void doVramTransfer(u8* dest);
        void setBackdrop(u16 val);
        void sgbLoadAttrFile(int index);
        void sgbPalXX(int block);
        void sgbAttrBlock(int block);
        void sgbAttrLin(int block);
        void sgbAttrDiv(int block);
        void sgbAttrChr(int block);
        void sgbPalSet(int block);
        void sgbPalTrn(int block);
        void sgbDataSnd(int block);
        void sgbMltReq(int block);
        void sgbChrTrn(int block);
        void sgbPctTrn(int block);
        void sgbAttrTrn(int block);
        void sgbAttrSet(int block);
        void sgbMask(int block);
    private:

        int sgbPacketLength; // Number of packets to be transferred this command
        int sgbPacketsTransferred; // Number of packets which have been transferred so far
        int sgbPacketBit; // Next bit # to be sent in the packet. -1 if no packet is being transferred.
        u8 sgbPacket[16];
        u8 sgbCommand;

        u8 sgbNumControllers;
        u8 sgbSelectedController; // Which controller is being observed
        u8 sgbButtonsChecked;
        u8 sgbActiveController; // Which controller does this DS act as

        // Data for various different sgb commands
        struct SgbCmdData {
            int numDataSets;

            union {
                struct {
                    u8 data[6];
                    u8 dataBytes;
                } attrBlock;

                struct {
                    u8 writeStyle;
                    u8 x,y;
                } attrChr;
            };
        } cmdData;
};

typedef void (Gameboy::*mbcWrite)(u16,u8);
typedef u8   (Gameboy::*mbcRead )(u16);

const mbcRead mbcReads[] = {
    NULL, NULL, NULL, &Gameboy::m3r, NULL, NULL, NULL, &Gameboy::h3r, NULL
};
const mbcWrite mbcWrites[] = {
    &Gameboy::m0w, &Gameboy::m1w, &Gameboy::m2w, &Gameboy::m3w, NULL, &Gameboy::m5w, NULL, &Gameboy::h3w, &Gameboy::h1w
};


extern Gameboy* gameboy;

extern struct Registers gbRegs;

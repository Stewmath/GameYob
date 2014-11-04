#pragma once
#include <stdio.h>
#include <vector>
#include "time.h"
#include "gbgfx.h"
#include "io.h"

#ifdef DS
#include <nds.h>
#endif

#define MAX_SRAM_SIZE   0x20000

// IMPORTANT: This is unchanging, it DOES NOT change in double speed mode!
#define clockSpeed 4194304

// Same deal
#define CYCLES_PER_FRAME 70224


#define GB			0
#define CGB			1

class CheatEngine;
class SoundEngine;
class RomFile;

// Interrupts
#define INT_VBLANK  0x01
#define INT_LCD     0x02
#define INT_TIMER   0x04
#define INT_SERIAL  0x08
#define INT_JOYPAD  0x10

// Return codes for runEmul
#define RET_VBLANK  1
#define RET_LINK    2

// Be careful changing this; it affects save state compatibility.
struct ClockStruct
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



class Gameboy {
    public:
        // gameboy.cpp

        Gameboy();
        ~Gameboy();
        void init();
        void initGBMode();
        void initGBCMode();
        void initSND();
        void initGFXPalette();
        bool isMainGameboy();

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
        int runEmul()
#ifdef DS
            ITCM_CODE
#endif
            ;
        void initGameboyMode();
        void checkLYC();
        int updateLCD(int cycles)
#ifdef DS
            ITCM_CODE
#endif
            ;
        void updateTimers(int cycles)
#ifdef DS
            ITCM_CODE
#endif
            ;
        void requestInterrupt(int id);
        void setDoubleSpeed(int val);

        void setRomFile(RomFile* r);
        void unloadRom();
        void printRomInfo();
        bool isRomLoaded();

        int loadSave(int saveId);
        int saveGame();
        void gameboySyncAutosave();
        void updateAutosave();

        void saveState(int num);
        int loadState(int num);
        void deleteState(int num);
        bool checkStateExists(int num);

        inline int getCyclesSinceVBlank() { return cyclesSinceVBlank + extraCycles; }
        inline bool isDoubleSpeed() { return doubleSpeed; }

        inline CheatEngine* getCheatEngine() { return cheatEngine; }
        inline SoundEngine* getSoundEngine() { return soundEngine; }
        inline RomFile* getRomFile() { return romFile; }

        // variables
        Gameboy* linkedGameboy;

        u8 controllers[4];

        int doubleSpeed;

        int gbMode;
        bool sgbMode;

        int scanlineCounter;
        int phaseCounter;
        int dividerCounter;
        int timerCounter;
        int serialCounter;
        int timerPeriod;
        long periods[4];

        int cyclesToEvent;
        int cyclesSinceVBlank;
        int interruptTriggered;
        int gameboyFrameCounter;

        int emuRet;
        int cycleToSerialTransfer;

        int halt;
        int ime;
        int extraCycles;
        int soundCycles;
        int cyclesToExecute;
        struct Registers gbRegs;

    private:
        volatile bool gameboyPaused;
        bool resettingGameboy;

        CheatEngine* cheatEngine;
        SoundEngine* soundEngine;
        RomFile* romFile;

        FileHandle* saveFile;
        char savename[MAX_FILENAME_LEN];

        // gbcpu.cpp

    public:
        void initCPU();
        void enableInterrupts();
        void disableInterrupts();
        int handleInterrupts(unsigned int interruptTriggered);
        int runOpcode(int cycles)
#ifdef DS
            ITCM_CODE
#endif
            ;

        inline u8 quickRead(u16 addr) { return memory[addr>>12][addr&0xFFF]; }
        inline u8 quickReadIO(u8 addr) { return ioRam[addr]; }
        inline u16 quickRead16(u16 addr) { return quickRead(addr)|(quickRead(addr+1)<<8); }
        // Currently unused because this can actually overwrite the rom, in rare cases
        inline void quickWrite(u16 addr, u8 val) { memory[addr>>12][addr&0xFFF] = val; }


        // mmu.cpp

        void initMMU();
        void mapMemory();
//        u8 readMemory(u16 addr) ITCM_CODE;
        u8 readMemoryFast(u16 addr)
#ifdef DS
            ITCM_CODE
#endif
            ;
        u16 readMemory16(u16 addr);
        u8 readIO(u8 ioReg)
#ifdef DS
            ITCM_CODE
#endif
            ;
//        void writeMemory(u16 addr, u8 val) ITCM_CODE;
        void writeIO(u8 ioReg, u8 val)
#ifdef DS
            ITCM_CODE
#endif
            ;
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



        bool updateHBlankDMA();
        void latchClock();
        void writeSaveFileSectors(int startSector, int numSectors);

        inline int getNumRamBanks() { return numRamBanks; }
        inline u8 getWramBank() { return wramBank; }
        inline void setWramBank(u8 bank) { wramBank = bank; }

        inline void setSoundChannel(int which) {ioRam[0x26] |= which;}
        inline void clearSoundChannel(int which) {ioRam[0x26] &= ~which;}

        void refreshRomBank(int bank);
        void refreshRamBank(int bank);
        void writeSram(u16 addr, u8 val);

        // mmu variables

        int resultantGBMode;

        // whether the bios is mapped to memory
        bool biosOn;

        // memory[x][yyy] = ram value at xyyy
        u8* memory[0x10];

        u8 vram[2][0x2000];
        u8* externRam;
        u8 wram[8][0x1000];

        u8 highram[0x1000];
        u8* const hram;
        u8* const ioRam;

        u8 bgPaletteData[0x40]
#ifdef DS
        ALIGN(4)
#endif
    ;

        u8 sprPaletteData[0x40]
#ifdef DS
        ALIGN(4)
#endif
    ;

        int wramBank;
        int vramBank;

        u16 dmaSource;
        u16 dmaDest;
        u16 dmaLength;
        int dmaMode;

        bool saveModified;
        bool dirtySectors[MAX_SRAM_SIZE/512];
        int numSaveWrites;
        bool autosaveStarted;

        int rumbleValue;
        int lastRumbleValue;

        bool wroteToSramThisFrame;
        int framesSinceAutosaveStarted;

        void (Gameboy::*writeFunc)(u16, u8);
        u8 (Gameboy::*readFunc)(u16);

        bool suspendStateExists;

        int saveFileSectors[MAX_SRAM_SIZE/512];


        // mbc.cpp

        u8 m3r(u16 addr);
        u8 m7r(u16 addr);
        u8 h3r(u16 addr);

        void m0w(u16 addr, u8 val);
        void m1w(u16 addr, u8 val);
        void m2w(u16 addr, u8 val);
        void m3w(u16 addr, u8 val);
        void m5w(u16 addr, u8 val);
        void m7w(u16 addr, u8 val);
        void h1w(u16 addr, u8 val);
        void h3w(u16 addr, u8 val);

        void handleHuC3Command(u8 command);
        void writeClockStruct();


        // mbc variables

        ClockStruct gbClock;

        int numRamBanks;

        bool ramEnabled;

        int memoryModel;
        bool hasClock;
        int romBank;
        int currentRamBank;

        bool rockmanMapper;

        // HuC3
        u8   HuC3Mode;
        u8   HuC3Value;
        u8   HuC3Shift;

        // MBC7
        u8 mbc7State;
        u16 mbc7Buffer;
        u8 mbc7RA; // Ram Access register 0xa080

        // sgb.cpp
    public:
        void sgbInit();
        void sgbHandleP1(u8 val);
        u8 sgbReadP1();

        u8 sgbMap[20*18];

        // SGB packet commands
        void sgbDoVramTransfer(u8* dest);
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
        } sgbCmdData;
};

typedef void (Gameboy::*mbcWrite)(u16,u8);
typedef u8   (Gameboy::*mbcRead )(u16);

const mbcRead mbcReads[] = {
    NULL, NULL, NULL, &Gameboy::m3r, NULL, NULL, &Gameboy::m7r, NULL, &Gameboy::h3r
};
const mbcWrite mbcWrites[] = {
    &Gameboy::m0w, &Gameboy::m1w, &Gameboy::m2w, &Gameboy::m3w, NULL, &Gameboy::m5w, &Gameboy::m7w, &Gameboy::h1w, &Gameboy::h3w
};

extern Gameboy* gameboy;

extern struct Registers g_gbRegs;

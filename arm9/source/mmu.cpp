#include <stdio.h>
#include <cstdlib>
#include "mmu.h"
#include "gbcpu.h"
#include "gameboy.h"
#include "gbgfx.h"
#include "gbsnd.h"
#include "inputhelper.h"
#include "main.h"
#include "nifi.h"
#include "sgb.h"
#include "console.h"
#include "gbs.h"
#ifdef DS
#include <nds.h>
#endif

extern time_t rawTime;

#define refreshVramBank() { \
    memory[0x8] = vram[vramBank]; \
    memory[0x9] = vram[vramBank]+0x1000; }
#define refreshWramBank() { \
    memory[0xd] = wram[wramBank]; }

bool hasRumble;
int rumbleStrength;
int rumbleInserted = 0;
bool rumbleValue = 0;
bool lastRumbleValue = 0;

clockStruct gbClock;

int numRomBanks=0;
int numRamBanks=0;

int resultantGBMode;

u8 bios[0x900];
bool biosExists = false;
int biosEnabled;
bool biosOn = false;

u8 buttonsPressed = 0xff;

u8* memory[0x10]
#ifdef DS
DTCM_BSS
#endif
;
u8 vram[2][0x2000];
u8** externRam = NULL;
u8 wram[8][0x1000];

u8 highram[0x1000];
u8* const hram = highram+0xe00;
u8* const ioRam = hram+0x100;

int wramBank;
int vramBank;

int MBC;
int memoryModel;

bool rockmanMapper;

int romBank;
int currentRamBank;

u16 dmaSource;
u16 dmaDest;
u16 dmaLength;
int dmaMode;

// Autosaving stuff
bool saveModified=false;
int numSaveWrites=0;
int autosaveStart = -1;
int autosaveEnd = -1;


/* MBC flags */
bool ramEnabled;

u8   HuC3Mode;
u8   HuC3Value;
u8   HuC3Shift;

typedef void (* mbcWrite)(u16,u8);
typedef u8   (* mbcRead )(u16);

mbcWrite writeFunc;
mbcRead readFunc;

void refreshRomBank(int bank) 
{
    if (bank < numRomBanks) {
        romBank = bank;
        loadRomBank(); 
        memory[0x4] = romSlot1;
        memory[0x5] = romSlot1+0x1000;
        memory[0x6] = romSlot1+0x2000;
        memory[0x7] = romSlot1+0x3000; 
    }
}

void refreshRamBank (int bank) 
{
    if (bank < numRamBanks) {
        currentRamBank = bank;
        memory[0xa] = externRam[currentRamBank];
        memory[0xb] = externRam[currentRamBank]+0x1000; 
    }
}

void handleHuC3Command (u8 cmd) 
{
    switch (cmd&0xf0) {
        case 0x10: /* Read clock */
            if (HuC3Shift > 24)
                break;

            switch (HuC3Shift) {
                case 0: case 4: case 8:     /* Minutes */
                    HuC3Value = (gbClock.huc3.m >> HuC3Shift) & 0xf;
                    break;
                case 12: case 16: case 20:  /* Days */
                    HuC3Value = (gbClock.huc3.d >> (HuC3Shift - 12)) & 0xf;
                    break;
                case 24:                    /* Year */
                    HuC3Value = gbClock.huc3.y & 0xf;
                    break;
            }
            HuC3Shift += 4;
            break;
        case 0x40:
            switch (cmd&0xf) {
                case 0: case 4: case 7:
                    HuC3Shift = 0;
                    break;
            }

            latchClock();
            break;
        case 0x50:
            break;
        case 0x60: 
            HuC3Value = 1;
            break;
        default:
            printLog("undandled HuC3 cmd %02x\n", cmd);
    }
}


/* MBC read handlers */

/* HUC3 */
u8 h3r (u16 addr) {
    switch (HuC3Mode) {
        case 0xc:
            return HuC3Value;
        case 0xb:
        case 0xd:
            /* Return 1 as a fixed value, needed for some games to
             * boot, the meaning is unknown. */
            return 1;
    }
    return (ramEnabled) ? memory[addr>>12][addr&0xfff] : 0xff;
}

/* MBC3 */
u8 m3r (u16 addr) {
    if (!ramEnabled)
        return 0xff;

    switch (currentRamBank) { // Check for RTC register
        case 0x8:
            return gbClock.mbc3.s;
        case 0x9:
            return gbClock.mbc3.m;
        case 0xA:
            return gbClock.mbc3.h;
        case 0xB:
            return gbClock.mbc3.d&0xff;
        case 0xC:
            return gbClock.mbc3.ctrl;
        default: // Not an RTC register
            return memory[addr>>12][addr&0xfff];
    }
}

const mbcRead mbcReads[] = { 
    NULL, NULL, NULL, m3r, NULL, NULL, NULL, h3r, NULL
};


void writeSram(u16 addr, u8 val) {
    if (externRam[currentRamBank][addr] != val) {
        externRam[currentRamBank][addr] = val;
        if (autoSavingEnabled) {
            /*
            fseek(saveFile, currentRamBank*0x2000+addr, SEEK_SET);
            fputc(val, saveFile);
            */
            int pos = addr + currentRamBank*0x2000;
            saveModified = true;
            if (autosaveStart == -1) {
                autosaveStart = pos;
                autosaveEnd = pos;
            }
            else {
                if (pos < autosaveStart)
                    autosaveStart = pos;
                if (pos > autosaveEnd)
                    autosaveEnd = pos;
            }
            numSaveWrites++;
        }
    }
}

void writeClockStruct() {
    if (autoSavingEnabled) {
        fseek(saveFile, numRamBanks*0x2000, SEEK_SET);
        fwrite(&gbClock, 1, sizeof(gbClock), saveFile);
        saveModified = true;
    }
}


/* MBC Write handlers */

/* MBC0 */
void m0w (u16 addr, u8 val) {
    switch (addr >> 12) {
        case 0x0: /* 0000 - 1fff */
        case 0x1:
            break;
        case 0x2: /* 2000 - 3fff */
        case 0x3:
            break;
        case 0x4: /* 4000 - 5fff */
        case 0x5:
            break;
        case 0x6: /* 6000 - 7fff */
        case 0x7:
            break;
        case 0xa: /* a000 - bfff */
        case 0xb:
            if (numRamBanks)
                writeSram(addr&0x1fff, val);
            break;
    }
}

/* MBC2 */
void m2w(u16 addr, u8 val)
{
    switch (addr >> 12) {
        case 0x0: /* 0000 - 1fff */
        case 0x1:
            ramEnabled = ((val & 0xf) == 0xa);
            break;
        case 0x2: /* 2000 - 3fff */
        case 0x3:
            refreshRomBank((val) ? val : 1);
            break;
        case 0x4: /* 4000 - 5fff */
        case 0x5:
            break;
        case 0x6: /* 6000 - 7fff */
        case 0x7:
            break;
        case 0xa: /* a000 - bfff */
        case 0xb:
            if (ramEnabled && numRamBanks)
                writeSram(addr&0x1fff, val&0xf);
            break;
    }
}

/* MBC3 */
void m3w(u16 addr, u8 val)
{
    switch (addr >> 12) {
        case 0x0: /* 0000 - 1fff */
        case 0x1:
            ramEnabled = ((val & 0xf) == 0xa);
            break;
        case 0x2: /* 2000 - 3fff */
        case 0x3:
            val &= 0x7f;
            refreshRomBank((val) ? val : 1);
            break;
        case 0x4: /* 4000 - 5fff */
        case 0x5:
            /* The RTC register is selected by writing values 0x8-0xc, ram banks
             * are selected by values 0x0-0x3 */
            if (val <= 0x3)
                refreshRamBank(val);
            else if (val >= 8 && val <= 0xc)
                currentRamBank = val;
            break;
        case 0x6: /* 6000 - 7fff */
        case 0x7:
            if (val)
                latchClock();
            break;
        case 0xa: /* a000 - bfff */
        case 0xb:
            if (!ramEnabled)
                break;

            switch (currentRamBank) { // Check for RTC register
                case 0x8:
                    if (gbClock.mbc3.s != val) {
                        gbClock.mbc3.s = val;
                        writeClockStruct();
                    }
                    return;
                case 0x9:
                    if (gbClock.mbc3.m != val) {
                        gbClock.mbc3.m = val;
                        writeClockStruct();
                    }
                    return;
                case 0xA:
                    if (gbClock.mbc3.h != val) {
                        gbClock.mbc3.h = val;
                        writeClockStruct();
                    }
                    return;
                case 0xB:
                    if ((gbClock.mbc3.d&0xff) != val) {
                        gbClock.mbc3.d &= 0x100;
                        gbClock.mbc3.d |= val;
                        writeClockStruct();
                    }
                    return;
                case 0xC:
                    if (gbClock.mbc3.ctrl != val) {
                        gbClock.mbc3.d &= 0xFF;
                        gbClock.mbc3.d |= (val&1)<<8;
                        gbClock.mbc3.ctrl = val;
                        writeClockStruct();
                    }
                    return;
                default: // Not an RTC register
                    if (numRamBanks)
                        writeSram(addr&0x1fff, val);
            }
            break;
    }
}

/* MBC1 */
void m1w (u16 addr, u8 val) {
    int newBank;

    switch (addr >> 12) {
        case 0x0: /* 0000 - 1fff */
        case 0x1:
            ramEnabled = ((val & 0xf) == 0xa);
            break;
        case 0x2: /* 2000 - 3fff */
        case 0x3:
            val &= 0x1f;
            if (rockmanMapper)
                newBank = ((val > 0xf) ? val - 8 : val);
            else
                newBank = (romBank & 0xe0) | val;
            refreshRomBank((newBank) ? newBank : 1);
            break;
        case 0x4: /* 4000 - 5fff */
        case 0x5:
            val &= 3;
            /* ROM mode */
            if (memoryModel == 0) {
                newBank = (romBank & 0x1F) | (val<<5);
                refreshRomBank((newBank) ? newBank : 1);
            }
            /* RAM mode */
            else
                refreshRamBank(val);
            break;
        case 0x6: /* 6000 - 7fff */
        case 0x7:
            memoryModel = val & 1;
            break;
        case 0xa: /* a000 - bfff */
        case 0xb:
            if (ramEnabled && numRamBanks)
                writeSram(addr&0x1fff, val);
            break;
    }
}

/* HUC1 */
void h1w(u16 addr, u8 val)
{
    switch (addr >> 12) {
        case 0x0: /* 0000 - 1fff */
        case 0x1:
            ramEnabled = ((val & 0xf) == 0xa);
            break;
        case 0x2: /* 2000 - 3fff */
        case 0x3:
            refreshRomBank(val & 0x3f);
            break;
        case 0x4: /* 4000 - 5fff */
        case 0x5:
            val &= 3;
            /* ROM mode */
            if (memoryModel == 0) 
                refreshRomBank(val);
            /* RAM mode */
            else
                refreshRamBank(val);
            break;
        case 0x6: /* 6000 - 7fff */
        case 0x7:
            memoryModel = val & 1;
            break;
        case 0xa: /* a000 - bfff */
        case 0xb:
            if (ramEnabled && numRamBanks)
                writeSram(addr&0x1fff, val);
            break;
    }
}

/* MBC5 */
void m5w (u16 addr, u8 val) {
    switch (addr >> 12) {
        case 0x0: /* 0000 - 1fff */
        case 0x1:
            ramEnabled = ((val & 0xf) == 0xa);
            break;
        case 0x2: /* 2000 - 3fff */
            refreshRomBank((romBank & 0x100) |  val);
            break;
        case 0x3:
            refreshRomBank((romBank & 0xff ) | (val&1) << 8);
            break;
        case 0x4: /* 4000 - 5fff */
        case 0x5:
            val &= 0xf;
            /* MBC5 might have a rumble motor, which is triggered by the
             * 4th bit of the value written */
            if (hasRumble) {
                if (rumbleStrength) {
                    if (rumbleInserted) {
                        rumbleValue = (val & 0x8) ? 1 : 0;
                        if (rumbleValue != lastRumbleValue)
                        {
                            doRumble(rumbleValue);
                            lastRumbleValue = rumbleValue;
                        }
                    }
                }

                val &= 0x07;
            }

            refreshRamBank(val);
            break;
        case 0x6: /* 6000 - 7fff */
        case 0x7:
            break;
        case 0xa: /* a000 - bfff */
        case 0xb:
            if (ramEnabled && numRamBanks)
                writeSram(addr&0x1fff, val);
            break;
    }
}

/* HUC3 */
void h3w (u16 addr, u8 val) {
    switch (addr >> 12) {
        case 0x0: /* 0000 - 1fff */
        case 0x1:
            ramEnabled = ((val & 0xf) == 0xa);
            HuC3Mode = val;
            break;
        case 0x2: /* 2000 - 3fff */
        case 0x3:
            refreshRomBank((val) ? val : 1);
            break;
        case 0x4: /* 4000 - 5fff */
        case 0x5:
            refreshRamBank(val & 0xf);
            break;
        case 0x6: /* 6000 - 7fff */
        case 0x7:
            break;
        case 0xa: /* a000 - bfff */
        case 0xb:
            switch (HuC3Mode) {
                case 0xb:
                    handleHuC3Command(val);
                    break;
                case 0xc:
                case 0xd:
                case 0xe:
                    break;
                default:
                    if (ramEnabled && numRamBanks)
                        writeSram(addr&0x1fff, val);
            }
            break;
    }
}

const mbcWrite mbcWrites[] = {
    m0w, m1w, m2w, m3w, NULL, m5w, NULL, h3w, h1w
};

void initMMU()
{
    wramBank = 1;
    vramBank = 0;
    romBank = 1;
    currentRamBank = 0;

    memoryModel = 0;
    dmaSource=0;
    dmaDest=0;
    dmaLength=0;
    dmaMode=0;

    initSGB();

    ramEnabled = false;

    HuC3Value = 0;
    HuC3Shift = 0;

    /* Rockman8 by Yang Yang uses a silghtly different MBC1 variant */
    rockmanMapper = !strcmp(getRomTitle(), "ROCKMAN 99");

    readFunc = mbcReads[MBC];
    writeFunc = mbcWrites[MBC];

    biosOn = false;
    if (biosExists && !probingForBorder && !gbsMode) {
        if (biosEnabled == 2)
            biosOn = true;
        else if (biosEnabled == 1 && resultantGBMode == 0)
            biosOn = true;
    }
    mapMemory();
    memset(hram, 0, 0x200); // Initializes sprites and IO registers

    writeIO(0x02, 0x00);
    writeIO(0x05, 0x00);
    writeIO(0x06, 0x00);
    writeIO(0x07, 0x00);
    writeIO(0x40, 0x91);
    writeIO(0x42, 0x00);
    writeIO(0x43, 0x00);
    writeIO(0x45, 0x00);
    writeIO(0x47, 0xfc);
    writeIO(0x48, 0xff);
    writeIO(0x49, 0xff);
    writeIO(0x4a, 0x00);
    writeIO(0x4b, 0x00);
    writeIO(0xff, 0x00);

    ioRam[0x55] = 0xff;
}

void mapMemory() {
    if (biosOn)
        memory[0x0] = bios;
    else
        memory[0x0] = romSlot0;
    memory[0x1] = romSlot0+0x1000;
    memory[0x2] = romSlot0+0x2000;
    memory[0x3] = romSlot0+0x3000;
    refreshRomBank(romBank);
    refreshRamBank(currentRamBank);
    refreshVramBank();
    memory[0xc] = wram[0];
    refreshWramBank();
    memory[0xe] = wram[0];
    memory[0xf] = highram;

    dmaSource = (ioRam[0x51]<<8) | (ioRam[0x52]);
    dmaSource &= 0xFFF0;
    dmaDest = (ioRam[0x53]<<8) | (ioRam[0x54]);
    dmaDest &= 0x1FF0;
}

/* Increment y if x is greater than val */
#define OVERFLOW(x,val,y)   \
    do {                    \
        while (x >= val) {  \
            x -= val;       \
            y++;            \
        }                   \
    } while (0) 

void latchClock()
{
    // +2h, the same as lameboy
    time_t now = rawTime-120*60;
    time_t difference = now - gbClock.last;
    struct tm* lt = gmtime((const time_t *)&difference);

    switch (MBC) {
        case MBC3:
            gbClock.mbc3.s += lt->tm_sec;
            OVERFLOW(gbClock.mbc3.s, 60, gbClock.mbc3.m);
            gbClock.mbc3.m += lt->tm_min;
            OVERFLOW(gbClock.mbc3.m, 60, gbClock.mbc3.h);
            gbClock.mbc3.h += lt->tm_hour;
            OVERFLOW(gbClock.mbc3.h, 24, gbClock.mbc3.d);
            gbClock.mbc3.d += lt->tm_yday;
            /* Overflow! */
            if (gbClock.mbc3.d > 0x1FF)
            {
                /* Set the carry bit */
                gbClock.mbc3.ctrl |= 0x80;
                gbClock.mbc3.d -= 0x200;
            }
            /* The 9th bit of the day register is in the control register */ 
            gbClock.mbc3.ctrl &= ~1;
            gbClock.mbc3.ctrl |= (gbClock.mbc3.d > 0xff);
            break;
        case HUC3:
            gbClock.huc3.m += lt->tm_min;
            OVERFLOW(gbClock.huc3.m, 60*24, gbClock.huc3.d);
            gbClock.huc3.d += lt->tm_yday;
            OVERFLOW(gbClock.huc3.d, 365, gbClock.huc3.y);
            gbClock.huc3.y += lt->tm_year - 70;
            break;
    }

    gbClock.last = now;
}

#ifdef DS
u8 readMemory(u16 addr) ITCM_CODE;
#endif

u8 readMemory(u16 addr)
{
    int area = addr>>13;
    if (area & 0x04) { // Addr >= 0x8000
        if (area == 0xe/2) {
            if (addr >= 0xff00)
                return readIO(addr&0xff);
            // Check for echo area
            else if (addr < 0xfe00)
                addr -= 0x2000;
        }
        /* Check if in range a000-bfff */
        else if (area == 0xa/2) {
            /* Check if there's an handler for this mbc */
            if (readFunc != NULL)
                return readFunc(addr);
        }
    }

    return memory[addr>>12][addr&0xfff];
}

u16 readMemory16(u16 addr) {
    return readMemory(addr) | readMemory(addr+1)<<8;
}

#ifdef DS
u8 readIO(u8 ioReg) ITCM_CODE;
#endif

u8 readIO(u8 ioReg)
{
    switch (ioReg)
    {
        case 0x00:
            if (!(ioRam[ioReg]&0x20))
                return 0xc0 | (ioRam[ioReg] & 0xF0) | (buttonsPressed & 0xF);
            else if (!(ioRam[ioReg]&0x10))
                return 0xc0 | (ioRam[ioReg] & 0xF0) | ((buttonsPressed & 0xF0)>>4);
            else
                return ioRam[ioReg];
        case 0x10: // NR10, sweep register 1, bit 7 set on read
            return ioRam[ioReg] | 0x80;
        case 0x11: // NR11, sound length/pattern duty 1, bits 5-0 set on read
            return ioRam[ioReg] | 0x3F;
        case 0x13: // NR13, sound frequency low byte 1, all bits set on read
            return 0xFF;
        case 0x14: // NR14, sound frequency high byte 1, bits 7,5-0 set on read
            return ioRam[ioReg] | 0xBF;
        case 0x15: // No register, all bits set on read
            return 0xFF;
        case 0x16: // NR21, sound length/pattern duty 2, bits 5-0 set on read
            return ioRam[ioReg] | 0x3F;
        case 0x18: // NR23, sound frequency low byte 2, all bits set on read
            return 0xFF;
        case 0x19: // NR24, sound frequency high byte 2, bits 7,5-0 set on read
            return ioRam[ioReg] | 0xBF;
        case 0x1A: // NR30, sound mode 3, bits 6-0 set on read
            return ioRam[ioReg] | 0x7F;
        case 0x1B: // NR31, sound length 3, all bits set on read
            return 0xFF;
        case 0x1C: // NR32, sound output level 3, bits 7,4-0 set on read
            return ioRam[ioReg] | 0x9F;
        case 0x1D: // NR33, sound frequency low byte 2, all bits set on read
            return 0xFF;
        case 0x1E: // NR34, sound frequency high byte 2, bits 7,5-0 set on read
            return ioRam[ioReg] | 0xBF;
        case 0x1F: // No register, all bits set on read
            return 0xFF;
        case 0x20: // NR41, sound mode/length 4, all bits set on read
            return 0xFF;
        case 0x23: // NR44, sound counter/consecutive, bits 7,5-0 set on read
            return ioRam[ioReg] | 0xBF;
        case 0x26: // NR52, global sound status, bits 6-4 set on read
            return ioRam[ioReg] | 0x70;
        case 0x27: // No register, all bits set on read
        case 0x28:
        case 0x29:
        case 0x2A:
        case 0x2B:
        case 0x2C:
        case 0x2D:
        case 0x2E:
        case 0x2F:
            return 0xFF;
        case 0x70: // wram register
            return ioRam[ioReg] | 0xf8;
        default:
            return ioRam[ioReg];
    }
}

#ifdef DS
void writeMemory(u16 addr, u8 val) ITCM_CODE;
#endif

void writeMemory(u16 addr, u8 val)
{
    switch (addr >> 12)
    {
        case 0x8:
        case 0x9:
            writeVram(addr&0x1fff, val);
            return;
        case 0xC:
            wram[0][addr&0xFFF] = val;
            return;
        case 0xD:
            wram[wramBank][addr&0xFFF] = val;
            return;
        case 0xE: // Echo area
            wram[0][addr&0xFFF] = val;
            return;
        case 0xF:
            if (addr >= 0xFF00)
                writeIO(addr & 0xFF, val);
            else if (addr >= 0xFE00)
                writeHram(addr&0x1ff, val);
            else // Echo area
                wram[wramBank][addr&0xFFF] = val;
            return;

    }

    if (writeFunc != NULL)
        writeFunc(addr, val);
}


bool nifiTryAgain=true;
void nifiTimeoutFunc() {
    printLog("Nifi timeout\n");
    if (nifiTryAgain) {
        nifiSendid--;
        sendPacketByte(55, ioRam[0x01]);
        nifiSendid++;
        nifiTryAgain = false;
    }
    else {
        // There was no response from nifi, assume no connection.
        printLog("No nifi response received\n");
        ioRam[0x01] = 0xff;
        requestInterrupt(SERIAL);
        ioRam[0x02] &= ~0x80;
        timerStop(2);
    }
}

#ifdef DS
void writeIO(u8 ioReg, u8 val) ITCM_CODE;
#endif

void writeIO(u8 ioReg, u8 val)
{
    switch (ioReg)
    {
        case 0x00:
            if (sgbMode)
                sgbHandleP1(val);
            else
                ioRam[0x00] = val|0xcf;
            return;
        case 0x02:
            {
                ioRam[ioReg] = val;
                if (!nifiEnabled) {
                    if (val & 0x80 && val & 0x01) {
                        serialCounter = clockSpeed/1024;
                        if (cyclesToExecute > serialCounter)
                            cyclesToExecute = serialCounter;
                    }
                    else
                        serialCounter = 0;
                    return;
                }
                sendData = ioRam[0x01];
                if (val & 0x80) {
                    if (transferWaiting) {
                        sendPacketByte(56, ioRam[0x01]);
                        ioRam[0x01] = packetData;
                        requestInterrupt(SERIAL);
                        ioRam[ioReg] &= ~0x80;
                        transferWaiting = false;
                    }
                    if (val & 1) {
                        nifiTryAgain = true;
                        timerStart(2, ClockDivider_64, 10000, nifiTimeoutFunc);
                        sendPacketByte(55, ioRam[0x01]);
                        nifiSendid++;
                    }
                }
                return;
            }
        case 0x04:
            ioRam[ioReg] = 0;
            return;
        case 0x05:
            ioRam[ioReg] = val;
            break;
        case 0x06:
            ioRam[ioReg] = val;
            break;
        case 0x07:
            timerPeriod = periods[val&0x3];
            ioRam[ioReg] = val;
            break;
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
        case 0x14:
        case 0x16:
        case 0x17:
        case 0x18:
        case 0x19:
        case 0x1A:
        case 0x1B:
        case 0x1C:
        case 0x1D:
        case 0x1E:
        case 0x20:
        case 0x21:
        case 0x22:
        case 0x23:
        case 0x24:
        case 0x25:
        case 0x26:
        case 0x30:
        case 0x31:
        case 0x32:
        case 0x33:
        case 0x34:
        case 0x35:
        case 0x36:
        case 0x37:
        case 0x38:
        case 0x39:
        case 0x3A:
        case 0x3B:
        case 0x3C:
        case 0x3D:
        case 0x3E:
        case 0x3F:
            handleSoundRegister(ioReg, val);
            return;
        case 0x40:
        case 0x42:
        case 0x43:
        case 0x46:
        case 0x47:
        case 0x48:
        case 0x49:
        case 0x4A:
        case 0x4B:
        case 0x69:
        case 0x6B:
            handleVideoRegister(ioReg, val);
            return;
        case 0x41:
            ioRam[ioReg] &= 0x7;
            ioRam[ioReg] |= val&0xF8;
            return;
        case 0x44:
            //ioRam[0x44] = 0;
            printLog("LY Write %d\n", val);
            return;
        case 0x45:
            ioRam[ioReg] = val;
            checkLYC();
            cyclesToExecute = -1;
            return;
        case 0x68:
            ioRam[ioReg] = val;
            ioRam[0x69] = bgPaletteData[val&0x3F];
            return;
        case 0x6A:
            ioRam[ioReg] = val;
            ioRam[0x6B] = sprPaletteData[val&0x3F];
            return;
        case 0x4D:
            ioRam[ioReg] &= 0x80;
            ioRam[ioReg] |= (val&1);
            return;
        case 0x4F: // Vram bank
            if (gbMode == CGB)
            {
                vramBank = val & 1;
                refreshVramBank();
            }
            ioRam[ioReg] = val&1;
            return;
            // Special register, used by the gameboy bios
        case 0x50:
            biosOn = 0;
            memory[0x0] = romSlot0;
            initGameboyMode();
            return;
        case 0x55: // CGB DMA
            if (gbMode == CGB)
            {
                if (dmaLength > 0)
                {
                    if ((val&0x80) == 0)
                    {
                        ioRam[ioReg] |= 0x80;
                        dmaLength = 0;
                    }
                    return;
                }
                dmaLength = ((val & 0x7F)+1);
                dmaSource = (ioRam[0x51]<<8) | (ioRam[0x52]);
                dmaSource &= 0xFFF0;
                dmaDest = (ioRam[0x53]<<8) | (ioRam[0x54]);
                dmaDest &= 0x1FF0;
                dmaMode = val>>7;
                ioRam[ioReg] = dmaLength-1;
                if (dmaMode == 0)
                {
                    int i;
                    for (i=0; i<dmaLength; i++)
                    {
                        writeVram16(dmaDest, dmaSource);
                        dmaDest += 0x10;
                        dmaSource += 0x10;
                    }
                    extraCycles += dmaLength*8*(doubleSpeed+1);
                    dmaLength = 0;
                    ioRam[ioReg] = 0xFF;
                    ioRam[0x51] = dmaSource>>8;
                    ioRam[0x52] = dmaSource&0xff;
                    ioRam[0x53] = dmaDest>>8;
                    ioRam[0x54] = dmaDest&0xff;
                }
            }
            else
                ioRam[ioReg] = val;
            return;
        case 0x70:				// WRAM bank, for CGB only
            if (gbMode == CGB)
            {
                wramBank = val & 0x7;
                if (wramBank == 0)
                    wramBank = 1;
                refreshWramBank();
            }
            ioRam[ioReg] = val&0x7;
            return;
        case 0x0F:
            ioRam[ioReg] = val;
            if (val & ioRam[0xff])
                cyclesToExecute = -1;
            break;
        case 0xFF:
            ioRam[ioReg] = val;
            if (gbsMode)
                ioRam[ioReg] = TIMER; // Allow only timer interrupts for GBS stuff
            if (val & ioRam[0x0f])
                cyclesToExecute = -1;
            break;
        default:
            ioRam[ioReg] = val;
            return;
    }
}


bool updateHblankDMA()
{
    if (dmaLength > 0)
    {
        writeVram16(dmaDest, dmaSource);
        dmaDest += 16;
        dmaSource += 16;
        dmaLength --;
        if (dmaLength == 0)
        {
            ioRam[0x55] = 0xFF;
        }
        else
            ioRam[0x55] = dmaLength-1;
        ioRam[0x51] = dmaSource>>8;
        ioRam[0x52] = dmaSource&0xff;
        ioRam[0x53] = dmaDest>>8;
        ioRam[0x54] = dmaDest&0xff;
        return true;
    }
    else
        return false;
}


void doRumble(bool rumbleVal)
{
    if (rumbleInserted == 1)
    {
        setRumble(rumbleVal);
    }
    else if (rumbleInserted == 2)
    {
        GBA_BUS[0x1FE0000/2] = 0xd200;
        GBA_BUS[0x0000000/2] = 0x1500;
        GBA_BUS[0x0020000/2] = 0xd200;
        GBA_BUS[0x0040000/2] = 0x1500;
        GBA_BUS[0x1E20000/2] = rumbleVal ? (0xF0 + rumbleStrength) : 0x08;
        GBA_BUS[0x1FC0000/2] = 0x1500;
    }
}

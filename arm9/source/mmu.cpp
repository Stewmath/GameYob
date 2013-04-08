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
#ifdef DS
#include <nds.h>
#endif


#define refreshVramBank() { \
    memory[0x8] = vram[vramBank]; \
    memory[0x9] = vram[vramBank]+0x1000; }
#define refreshWramBank() { \
    memory[0xd] = wram[wramBank]; }

bool rumbleEnabled;

int watchAddr=-1;
int readWatchAddr=-1;
int bankWatchAddr=-1;

clockStruct gbClock;

int numRomBanks=0;
int numRamBanks=0;

u8 bios[0x900];
bool biosExists = false;
bool biosEnabled = false;
bool biosOn = false;

u8* memory[0x10]
#ifdef DS
DTCM_BSS
#endif
;
u8* rom[MAX_ROM_BANKS];
u8 vram[2][0x2000];
u8** externRam = NULL;
u8 wram[8][0x1000];

u8 highram[0x1000];
u8* hram = highram+0xe00;
u8* ioRam = hram+0x100;

u8 spriteData[0xA0]
#ifdef DS
DTCM_BSS
#endif
;

int wramBank;
int vramBank;

int MBC;
int memoryModel;

int currentRomBank;
int currentRamBank;

u16 dmaSource;
u16 dmaDest;
u16 dmaLength;
int dmaMode;

/* MBC flags */
bool ramEnabled;
u8   HuC3Mode;
u8   HuC3Value;
u8   HuC3Shift;
u8   HuC3CmdMode;

typedef void (* mbcWrite)(u16,u8);
typedef u8   (* mbcRead )(u16);

void refreshRomBank(int bank) 
{
    if (bank < numRomBanks) {
        currentRomBank = bank;
        loadRomBank(); 
        memory[0x4] = rom[currentRomBank];
        memory[0x5] = rom[currentRomBank]+0x1000;
        memory[0x6] = rom[currentRomBank]+0x2000;
        memory[0x7] = rom[currentRomBank]+0x3000; 
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

void initMMU()
{
    wramBank = 1;
    vramBank = 0;
    memoryModel = 0;
    dmaSource=0;
    dmaDest=0;
    dmaLength=0;
    dmaMode=0;

    HuC3Value = 0;
    HuC3Shift = 0;

    if (!biosExists)
        biosEnabled = false;
    biosOn = biosEnabled;
    mapMemory();
    memset(ioRam, 0, 0x100);
}

void mapMemory() {
    if (biosOn)
        memory[0x0] = bios;
    else
        memory[0x0] = rom[0];
    memory[0x1] = rom[0]+0x1000;
    memory[0x2] = rom[0]+0x2000;
    memory[0x3] = rom[0]+0x3000;
    refreshRomBank(1);
    refreshVramBank();
    refreshRamBank(0);
    memory[0xc] = wram[0];
    refreshWramBank();
    memory[0xe] = wram[0];
    memory[0xf] = highram;

    dmaSource = (ioRam[0x51]<<8) | (ioRam[0x52]);
    dmaSource &= 0xFFF0;
    dmaDest = (ioRam[0x53]<<8) | (ioRam[0x54]);
    dmaDest &= 0x1FF0;
}

void latchClock()
{
    // +2h, the same as lameboy
    time_t now = time(NULL)-120*60;
    time_t difference = now - gbClock.clockLastTime;

    int seconds = difference%60;
    gbClock.clockSeconds += seconds;
    if (gbClock.clockSeconds >= 60)
    {
        gbClock.clockMinutes++;
        gbClock.clockSeconds -= 60;
    }

    difference /= 60;
    int minutes = difference%60;
    gbClock.clockMinutes += minutes;
    if (gbClock.clockMinutes >= 60)
    {
        gbClock.clockHours++;
        gbClock.clockMinutes -= 60;
    }

    difference /= 60;
    gbClock.clockHours += difference%24;
    if (gbClock.clockHours >= 24)
    {
        gbClock.clockDays++;
        gbClock.clockHours -= 24;
    }

    difference /= 24;
    gbClock.clockDays += difference;
    if (gbClock.clockDays > 0x1FF)
    {
        gbClock.clockControl |= 0x80;
        gbClock.clockDays -= 0x200;
    }
    gbClock.clockControl &= ~1;
    gbClock.clockControl |= gbClock.clockDays>>8;
    gbClock.clockLastTime = now;

    gbClock.clockSecondsL = gbClock.clockSeconds;
    gbClock.clockMinutesL = gbClock.clockMinutes;
    gbClock.clockHoursL = gbClock.clockHours;
    gbClock.clockDaysL = gbClock.clockDays;
    gbClock.clockControlL = gbClock.clockControl;
}

void handleHuC3Command (u8 cmd) 
{
    switch (cmd&0xf0) {
        case 0x60: 
            HuC3Value = 1;
            break;
        default:
            printLog("undandled HuC3 cmd %02x\n", cmd);
    }
}

#ifdef DS
u8 readMemory(u16 addr) ITCM_CODE;
#endif

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
    switch (currentRamBank) {
        case 0x8:
            return gbClock.clockSecondsL;
        case 0x9:
            return gbClock.clockMinutesL;
        case 0xA:
            return gbClock.clockHoursL;
        case 0xB:
            return gbClock.clockDaysL&0xFF;
        case 0xC:
            return gbClock.clockControlL;
    }
    return (ramEnabled) ? memory[addr>>12][addr&0xfff] : 0xff;
}

const mbcRead mbcReads[] = { 
    NULL, NULL, NULL, m3r, NULL, h3r, NULL
};

u8 readMemory(u16 addr)
{    
    if (addr >= 0xff00)
        return readIO(addr&0xff);

    /* Check if in range a000-bfff */
    if ((addr & 0xe000) == 0xa000) {
        /* Check if there's an handler for this mbc */
        if (mbcReads[MBC])
            return mbcReads[MBC](addr);
    }

    return memory[addr>>12][addr&0xfff];
}

#ifdef DS
u8 readIO(u8 ioReg) ITCM_CODE;
#endif

u8 readIO(u8 ioReg)
{
    switch (ioReg)
    {
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
        default:
            return ioRam[ioReg];
    }
}

#ifdef DS
void writeMemory(u16 addr, u8 val) ITCM_CODE;
#endif

/* MBC Write handlers */

/* MBC0 */
void m0w (u16 addr, u8 val) {
    switch (addr & 0xe000) {
        case 0x0000: /* 0000 - 1fff */
            break;
        case 0x2000: /* 2000 - 3fff */
            break;
        case 0x4000: /* 4000 - 5fff */
            break;
        case 0x6000: /* 6000 - 7fff */
            break;
        case 0xa000: /* a000 - bfff */
            if (ramEnabled)
                externRam[currentRamBank][addr&0x1fff] = val;
            break;
    }
}

/* MBC2 */
void m2w(u16 addr, u8 val)
{
    switch (addr & 0xe000) {
        case 0x0000: /* 0000 - 1fff */
            ramEnabled = ((val & 0xf) == 0xa);
            break;
        case 0x2000: /* 2000 - 3fff */
            refreshRomBank((val) ? val : 1);
            break;
        case 0x4000: /* 4000 - 5fff */
            break;
        case 0x6000: /* 6000 - 7fff */
            break;
        case 0xa000: /* a000 - bfff */
            if (ramEnabled)
                externRam[currentRamBank][addr&0x1fff] = val&0xf;
            break;
    }
}

/* MBC3 */
void m3w(u16 addr, u8 val)
{
    switch (addr & 0xe000) {
        case 0x0000: /* 0000 - 1fff */
            ramEnabled = ((val & 0xf) == 0xa);
            break;
        case 0x2000: /* 2000 - 3fff */
            val &= 0x7f;
            refreshRomBank((val) ? val : 1);
            break;
        case 0x4000: /* 4000 - 5fff */
            refreshRamBank(val);
            break;
        case 0x6000: /* 6000 - 7fff */
            if (val)
                latchClock();
            break;
        case 0xa000: /* a000 - bfff */
            switch (currentRamBank) {
                case 0x8:
                    gbClock.clockSeconds = val%60;
                    return;
                case 0x9:
                    gbClock.clockMinutes = val%60;
                    return;
                case 0xA:
                    gbClock.clockHours = val%24;
                    return;
                case 0xB:
                    gbClock.clockDays &= 0x100;
                    gbClock.clockDays |= val;
                    return;
                case 0xC:
                    gbClock.clockDays &= 0xFF;
                    gbClock.clockDays |= (val&1)<<8;
                    gbClock.clockControl = val;
                    return;
            }
            if (ramEnabled)
                externRam[currentRamBank][addr&0x1fff] = val;
            break;
    }
}

/* MBC1 */
void m1w (u16 addr, u8 val) {
    int newBank;

    switch (addr & 0xe000) {
        case 0x0000: /* 0000 - 1fff */
            ramEnabled = ((val & 0xf) == 0xa);
            break;
        case 0x2000: /* 2000 - 3fff */
            newBank = (currentRomBank & 0xe0) | (val & 0x1f);
            refreshRomBank((newBank) ? newBank : 1);
            break;
        case 0x4000: /* 4000 - 5fff */
            val &= 3;
            /* ROM mode */
            if (memoryModel == 0) {
                newBank = (currentRomBank & 0x1F) | (val<<5);
                refreshRomBank((newBank) ? newBank : 1);
            }
            /* RAM mode */
            else
                refreshRamBank(val);
            break;
        case 0x6000: /* 6000 - 7fff */
            memoryModel = val & 1;
            break;
        case 0xa000: /* a000 - bfff */
            if (ramEnabled)
                externRam[currentRamBank][addr&0x1fff] = val;
            break;
    }
}

/* HUC1 */
void h1w(u16 addr, u8 val)
{
    switch (addr & 0xe000) {
        case 0x0000: /* 0000 - 1fff */
            ramEnabled = ((val & 0xf) == 0xa);
            break;
        case 0x2000: /* 2000 - 3fff */
            refreshRomBank(val & 0x3f);
            break;
        case 0x4000: /* 4000 - 5fff */
            val &= 3;
            /* ROM mode */
            if (memoryModel == 0) 
                refreshRomBank(val);
            /* RAM mode */
            else
                refreshRamBank(val);
            break;
        case 0x6000: /* 6000 - 7fff */
            memoryModel = val & 1;
            break;
        case 0xa000: /* a000 - bfff */
            if (ramEnabled)
                externRam[currentRamBank][addr&0x1fff] = val;
            break;
    }
}

/* MBC5 */
void m5w (u16 addr, u8 val) {
    switch (addr & 0xe000) {
        case 0x0000: /* 0000 - 1fff */
            ramEnabled = ((val & 0xf) == 0xa);
            break;
        case 0x2000: /* 2000 - 3fff */
            switch (addr >> 12) {
                case 0x2: refreshRomBank((currentRomBank & 0x100) |  val);          break;
                case 0x3: refreshRomBank((currentRomBank & 0xff ) | (val&1) << 8);  break;
            }
            break;
        case 0x4000: /* 4000 - 5fff */
            val &= 0xf;
            /* MBC5 might have a rumble motor, which is triggered by the
             * 4th bit of the value written */
            if (hasRumble) {
                if (rumbleEnabled)
                    RUMBLE_PAK = (val&0x8) ? 0x00 : 0x02;
                val &= 0x07;
            }
            refreshRamBank(val);
            break;
        case 0x6000: /* 6000 - 7fff */
            break;
        case 0xa000: /* a000 - bfff */
            if (ramEnabled)
                externRam[currentRamBank][addr&0x1fff] = val;;
            break;
    }
}

/* HUC3 */
void h3w (u16 addr, u8 val) {
    switch (addr & 0xe000) {
        case 0x0000: /* 0000 - 1fff */
            ramEnabled = ((val & 0xf) == 0xa);
            HuC3Mode = val;
            break;
        case 0x2000: /* 2000 - 3fff */
            refreshRomBank((val) ? val : 1);
            break;
        case 0x4000: /* 4000 - 5fff */
            refreshRamBank(val & 0xf);
            break;
        case 0x6000: /* 6000 - 7fff */
            break;
        case 0xa000: /* a000 - bfff */
            switch (HuC3Mode) {
                case 0xb:
                    handleHuC3Command(val);
                    break;
                case 0xc:
                case 0xd:
                case 0xe:
                    break;
                default:
                    if (ramEnabled)
                        externRam[currentRamBank][addr&0x1fff] = val;
            }
            break;
    }
}

const mbcWrite mbcWrites[] = {
    m0w, m1w, m2w, m3w, m5w, h3w, h1w
};

void writeMemory(u16 addr, u8 val)
{
    /* TODO : numRamBanks == 0 should be handled ? */
    switch (addr >> 12)
    {
        case 0x8:
        case 0x9:
            writeVram(addr&0x1fff, val);
            break;
        case 0xC:
            wram[0][addr&0xFFF] = val;
            break;
        case 0xD:
            wram[wramBank][addr&0xFFF] = val;
            break;
        case 0xE:
            break;
        case 0xF:
            if (addr >= 0xFF00) 
                writeIO(addr & 0xFF, val);
            else
                writeHram(addr&0x1ff, val);
            break;
    }

    if (mbcWrites[MBC])
        mbcWrites[MBC](addr, val);
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
            if (!(val&0x20))
                ioRam[ioReg] = 0xc0 | (val & 0xF0) | (buttonsPressed & 0xF);
            else if (!(val&0x10))
                ioRam[ioReg] = 0xc0 | (val & 0xF0) | ((buttonsPressed & 0xF0)>>4);
            else
                ioRam[ioReg] = 0xff;
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
        case 0x41:
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
        case 0x44:
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
            memory[0x0] = rom[0];
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
                cyclesToExecute = 0;
            break;
        case 0xFF:
            ioRam[ioReg] = val;
            if (val & ioRam[0x0f])
                cyclesToExecute = 0;
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

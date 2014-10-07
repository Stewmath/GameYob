#include "gameboy.h"
#include "console.h"
#include "inputhelper.h"
#include "menu.h"
#include "romfile.h"
#include "timer.h"


/* MBC read handlers */

/* MBC3 */
u8 Gameboy::m3r (u16 addr) {
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

/* MBC7 */
u8 Gameboy::m7r (u16 addr) {
    switch (addr & 0xa0f0) {
        case 0xa000:
        case 0xa010:
        case 0xa060:
        case 0xa070:
            return 0;
        case 0xa020:
            return system_getMotionSensorX() & 0xff;
        case 0xa030:
            return system_getMotionSensorX() >> 8;
        case 0xa040:
            return system_getMotionSensorY() & 0xff;
        case 0xa050:
            return system_getMotionSensorY() >> 8;
            /*
        case 0xa080:
            return mbc7RA | 0x7e;
            */
        default:
            return 0xff;
    }
}

/* HUC3 */
u8 Gameboy::h3r (u16 addr) {
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


/* MBC Write handlers */

/* MBC0 (ROM) */
void Gameboy::m0w (u16 addr, u8 val) {
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

/* MBC1 */
void Gameboy::m1w (u16 addr, u8 val) {
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

/* MBC2 */
void Gameboy::m2w(u16 addr, u8 val) {
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
void Gameboy::m3w(u16 addr, u8 val) {
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

void Gameboy::writeClockStruct() {
    if (autoSavingEnabled) {
        file_seek(saveFile, numRamBanks*0x2000, SEEK_SET);
        file_write(&gbClock, 1, sizeof(gbClock), saveFile);
        saveModified = true;
    }
}


/* MBC5 */
void Gameboy::m5w (u16 addr, u8 val) {
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
            if (romFile->hasRumble()) {
                if (rumbleStrength) {
                    if (rumbleInserted) {
                        rumbleValue = (val & 0x8) ? 1 : 0;
                        if (rumbleValue != lastRumbleValue)
                        {
                            system_doRumble(rumbleValue);
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

enum {
    MBC7_RA_IDLE,
    MBC7_RA_READY,
};

/* MBC7 */
void Gameboy::m7w (u16 addr, u8 val) {
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

            refreshRamBank(val);
            break;
        case 0x6: /* 6000 - 7fff */
        case 0x7:
            break;
        case 0xa: /* a000 - bfff */
        case 0xb:
            if (addr == 0xa080) {
                bool finalize = val & 0x80;
                bool oldFinalize = mbc7RA & 0x80;
                bool sendBit = val & 0x40;
                bool oldSendBit = mbc7RA & 0x40;

                if (!oldFinalize && finalize) {
                    if (mbc7State == MBC7_RA_READY) {

                    }
                }

                if (!oldSendBit && sendBit) {

                }

                mbc7RA = val;
            }
            break;
    }
}

/* HUC1 */
void Gameboy::h1w(u16 addr, u8 val) {
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

/* HUC3 */

void Gameboy::h3w (u16 addr, u8 val) {
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

void Gameboy::handleHuC3Command (u8 cmd) 
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
            printLog("unhandled HuC3 cmd %02x\n", cmd);
    }
}


/* Increment y if x is greater than val */
#define OVERFLOW(x,val,y)   \
    do {                    \
        while (x >= val) {  \
            x -= val;       \
            y++;            \
        }                   \
    } while (0) 

void Gameboy::latchClock()
{
    // +2h, the same as lameboy
    time_t now = rawTime-120*60;
    time_t difference = now - gbClock.last;
    struct tm* lt = gmtime((const time_t *)&difference);

    switch (romFile->getMBC()) {
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
                gbClock.mbc3.d &= 0x1FF;
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

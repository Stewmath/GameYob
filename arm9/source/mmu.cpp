
#include <stdio.h>
#include <cstdlib>
#include "mmu.h"
#include "gbcpu.h"
#include "gameboy.h"
#include "gbgfx.h"
#include "gbsnd.h"
#include "inputhelper.h"
#include "main.h"

#define refreshRomBank() { \
    loadRomBank(); \
    memory[0x4] = rom[currentRomBank]; \
    memory[0x5] = rom[currentRomBank]+0x1000; \
    memory[0x6] = rom[currentRomBank]+0x2000; \
    memory[0x7] = rom[currentRomBank]+0x3000; }
#define refreshVramBank() { \
    memory[0x8] = vram[vramBank]; \
    memory[0x9] = vram[vramBank]+0x1000; }
#define refreshRamBank() { \
    memory[0xa] = externRam[currentRamBank]; \
    memory[0xb] = externRam[currentRamBank]+0x1000; }
#define refreshWramBank() { \
    memory[0xd] = wram[wramBank]; }


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

u8* memory[0x10];
u8* rom[MAX_ROM_BANKS];
u8 vram[2][0x2000];
u8** externRam = NULL;
u8 wram[8][0x1000];
u8 hram[0x200];
u8* highram = hram-0xe00;
u8* ioRam = &hram[0x100];
u8 spriteData[0xA0];
int wramBank;
int vramBank;

int MBC;
int memoryModel;
bool hasClock=false;
int currentRomBank;
int currentRamBank;

u16 dmaSource;
u16 dmaDest;
u16 dmaLength;
int dmaMode;

extern int dmaLine;

void initMMU()
{
    wramBank = 1;
    vramBank = 0;
    currentRomBank = 1;
    currentRamBank = 0;
    memoryModel = 1;

    if (biosEnabled)
        memory[0x0] = bios;
    else
        memory[0x0] = rom[0];
    memory[0x1] = rom[0]+0x1000;
    memory[0x2] = rom[0]+0x2000;
    memory[0x3] = rom[0]+0x3000;
    memory[0x4] = rom[currentRomBank];
    memory[0x5] = rom[currentRomBank]+0x1000;
    memory[0x6] = rom[currentRomBank]+0x2000;
    memory[0x7] = rom[currentRomBank]+0x3000;
    memory[0x8] = vram[vramBank];
    memory[0x9] = vram[vramBank]+0x1000;
    memory[0xa] = externRam[currentRamBank];
    memory[0xb] = externRam[currentRamBank]+0x1000;
    memory[0xc] = wram[0];
    memory[0xd] = wram[wramBank];
    memory[0xe] = wram[0];
    memory[0xf] = highram;
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

void writeVram(u16 addr, u8 val) {
    vram[vramBank][addr] = val;

    if (addr < 0x1800) {
        int tileNum = addr/16;
        int scanline = ioRam[0x44];
        if (scanline >= 128 && scanline < 144) {
            if (!changedTileInFrame[vramBank][tileNum]) {
                changedTileInFrame[vramBank][tileNum] = true;
                changedTileInFrameQueue[changedTileInFrameQueueLength++] = tileNum|(vramBank<<9);
            }
        }
        else {
            if (!changedTile[vramBank][tileNum]) {
                changedTile[vramBank][tileNum] = true;
                changedTileQueue[changedTileQueueLength++] = tileNum|(vramBank<<9);
            }
        }
    }
    else {
        int map = (addr-0x1800)/0x400;
        if (map)
            updateTileMap(map, addr-0x1c00, val);
        else
            updateTileMap(map, addr-0x1800, val);
    }
}
void writeVram16(u16 dest, u16 src) {
    for (int i=0; i<16; i++) {
        vram[vramBank][dest++] = readMemory(src++);
    }
    dest -= 16;
    src -= 16;
    if (dest < 0x1800) {
        int tileNum = dest/16;
        if (ioRam[0x44] < 144) {
            if (!changedTileInFrame[vramBank][tileNum]) {
                changedTileInFrame[vramBank][tileNum] = true;
                changedTileInFrameQueue[changedTileInFrameQueueLength++] = tileNum|(vramBank<<9);
            }
        }
        else {
            if (!changedTile[vramBank][tileNum]) {
                changedTile[vramBank][tileNum] = true;
                changedTileQueue[changedTileQueueLength++] = tileNum|(vramBank<<9);
            }
        }
    }
    else {
        for (int i=0; i<16; i++) {
            int addr = dest+i;
            int map = (addr-0x1800)/0x400;
            if (map)
                updateTileMap(map, addr-0x1c00, vram[vramBank][src+i]);
            else
                updateTileMap(map, addr-0x1800, vram[vramBank][src+i]);
        }
    }
}

u8 readMemory(u16 addr)
{
    switch (addr & 0xF000)
    {
        case 0x0000:
            if (biosOn)
                return bios[addr];
        case 0x1000:
        case 0x2000:
        case 0x3000:
            return rom[0][addr];
            break;
        case 0x4000:
        case 0x5000:
        case 0x6000:
        case 0x7000:
            return rom[currentRomBank][addr&0x3fff];
            break;
        case 0x8000:
        case 0x9000:
            return vram[vramBank][addr&0x1fff];
            break;
        case 0xA000:
            //	if (addr == 0xa080)
            //		return 1;
        case 0xB000:
            if (MBC == 3)
            {
                switch (currentRamBank)
                {
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
                    case 0:
                    case 1:
                    case 2:
                    case 3:
                        break;
                    default:
                        return 0;
                }
            }
            return externRam[currentRamBank][addr&0x1fff];
            break;
        case 0xC000:
            return wram[0][addr&0xFFF];
            break;
        case 0xD000:
            return wram[wramBank][addr&0xFFF];
            break;
        case 0xE000:
            return wram[0][addr&0xFFF];
            break;
        case 0xF000:
            if (addr < 0xFE00)
                return wram[wramBank][addr&0xFFF];
            else
            {
                switch (addr)
                {
                    case 0xFF00:
                        if (ioRam[0x00] & 0x20)
                            return (ioRam[0x00] & 0xF0) | ((buttonsPressed & 0xF0)>>4);
                        else
                            return (ioRam[0x00] & 0xF0) | (buttonsPressed & 0xF);
                        break;
                    case 0xFF26:
                        return ioRam[addr&0xff];
                        break;
                    case 0xFF69:
                        {
                            int index = ioRam[0x68] & 0x3F;
                            return bgPaletteData[index];
                            break;
                        }
                    case 0xFF6B:
                        {
                            int index = ioRam[0x6A] & 0x3F;
                            return sprPaletteData[index];
                            break;
                        }
                    case 0xFF6C:
                    case 0xFF72:
                    case 0xFF73:
                    case 0xFF74:
                    case 0xFF75:
                    case 0xFF76:
                    case 0xFF77:
                        return ioRam[addr&0xff];
                        break;
                    default:
                        return hram[addr&0x1ff];
                }
            }
            break;
        default:
            //return memory[addr];
            break;
    }
    return 0;
}

u16 readhword(u16 addr)
{
    return (readMemory(addr))|(readMemory(addr+1)<<8);
}

void writeMemory(u16 addr, u8 val)
{
#ifndef DS
    if (addr == watchAddr)
        debugMode = 1;
#endif
    switch (addr & 0xF000)
    {
        case 0x2000:
            if (MBC == 5)
            {
                currentRomBank &= 0x100;
                currentRomBank |= val;
                if (currentRomBank >= numRomBanks)
                {
                    currentRomBank = numRomBanks-1;
                    printLog("Game tried to access more rom than it has\n");
                }
                refreshRomBank();
                return;
            }
        case 0x3000:
            switch (MBC)
            {
                case 1:
                    currentRomBank &= 0xE0;
                    currentRomBank |= (val & 0x1F);
                    if (currentRomBank == 0)
                        currentRomBank = 1;
                    break;
                case 3:
                    currentRomBank = (val & 0x7F);
                    if (currentRomBank == 0)
                        currentRomBank = 1;
                    break;
                case 5:
                    currentRomBank &= 0xFF;
                    currentRomBank |= (val&1) << 8;
                    break;
                default:
                    break;
            }
            if (currentRomBank >= numRomBanks)
            {
                currentRomBank = numRomBanks-1;
                printLog("Game tried to access more rom than it has\n");
            }
            refreshRomBank();
            return;
        case 0x4000:
        case 0x5000:
            switch (MBC)
            {
                case 1:
                    if (memoryModel == 0)
                    {
                        currentRomBank &= 0x1F;
                        val &= 0xE0;
                        currentRomBank |= val;
                        if (currentRomBank == 0)
                            currentRomBank = 1;
                        if (currentRomBank >= numRomBanks)
                        {
                            currentRomBank = numRomBanks-1;
                            printLog("Game tried to access more rom than it has\n");
                        }
                        refreshRomBank();
                    }
                    else
                    {
                        currentRamBank = val & 0x3;
                        refreshRamBank();
                    }
                    break;
                case 3:
                case 5:
                    currentRamBank = val;
                    currentRamBank = val;
                    refreshRamBank();
                    break;
                default:
                    break;
            }
            if (currentRamBank >= numRamBanks && MBC != 3)
            {
                currentRamBank = numRamBanks-1;
                printLog("Game tried to access more ram than it has\n");
                refreshRamBank();
            }
            return;
        case 0x6000:
        case 0x7000:
            if (MBC == 1)
            {
                memoryModel = val & 1;
            }
            else if (MBC == 3)
            {
                if (val == 1)
                    latchClock();
            }
            return;
        case 0x8000:
        case 0x9000:
            writeVram(addr&0x1fff, val);
            return;
        case 0xA000:
        case 0xB000:
            if (MBC == 3)
            {
                switch (currentRamBank)
                {
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
                    case 0:
                    case 1:
                    case 2:
                    case 3:
                        break;
                    default:
                        return;
                }
            }
            externRam[currentRamBank][addr&0x1fff] = val;
            return;
        case 0xC000:
            wram[0][addr&0xFFF] = val;
            return;
        case 0xD000:
            wram[wramBank][addr&0xFFF] = val;
            return;
        case 0xE000:
            return;
        case 0xF000:
            switch (addr)
            {
                case 0xFF04:
                    ioRam[0x04] = 0;
                    return;
                case 0xFF05:
                    ioRam[0x05] = val;
                    break;
                case 0xFF07:
                    timerPeriod = periods[val&0x3];
                    ioRam[0x07] = val;
                    break;
                case 0xFF10:
                case 0xFF11:
                case 0xFF12:
                case 0xFF13:
                case 0xFF14:
                case 0xFF16:
                case 0xFF17:
                case 0xFF18:
                case 0xFF19:
                case 0xFF1A:
                case 0xFF1B:
                case 0xFF1C:
                case 0xFF1D:
                case 0xFF1E:
                case 0xFF20:
                case 0xFF21:
                case 0xFF22:
                case 0xFF23:
                case 0xFF24:
                case 0xFF25:
                case 0xFF26:
                case 0xFF30:
                case 0xFF31:
                case 0xFF32:
                case 0xFF33:
                case 0xFF34:
                case 0xFF35:
                case 0xFF36:
                case 0xFF37:
                case 0xFF38:
                case 0xFF39:
                case 0xFF3A:
                case 0xFF3B:
                case 0xFF3C:
                case 0xFF3D:
                case 0xFF3E:
                case 0xFF3F:
                    handleSoundRegister(addr, val);
                    return;
                case 0xFF40:    // LCDC
                    if ((val & 0x7F) != (ioRam[0x40] & 0x7F))
                        lineModified = true;
                    ioRam[0x40] = val;
                    return;
                case 0xFF41:
                    ioRam[0x41] &= 0x7;
                    ioRam[0x41] |= val&0xF8;
                    return;
                case 0xFF46:				// DMA
                    {
                        u16 src = val << 8;
                        int i;
                        for (i=0; i<0xA0; i++) {
                            u8 val = readMemory(src+i);
                            hram[i] = val;
                            if (ioRam[0x44] >= 144 || ioRam[0x44] <= 1)
                                spriteData[i] = val;
                        }

                        totalCycles += 50;
                        dmaLine = ioRam[0x44];
                        //printLog("dma write %d\n", ioRam[0x44]);
                        return;
                    }
                case 0xFF42:
                case 0xFF43:
                case 0xFF4A:
                case 0xFF4B:
                    {
                        int dest = addr&0xff;
                        if (val != ioRam[dest]) {
                            ioRam[addr&0xff] = val;
                            lineModified = true;
                        }
                    }
                    break;
                case 0xFF44:
                    //ioRam[0x44] = 0;
                    return;
                case 0xFF47:				// BG Palette (GB classic only)
                    ioRam[0x47] = val;
                    if (gbMode == GB)
                    {
                        updateClassicBgPalette();
                    }
                    return;
                case 0xFF48:				// Spr Palette (GB classic only)
                    ioRam[0x48] = val;
                    if (gbMode == GB)
                    {
                        updateClassicSprPalette(0);
                    }
                    return;
                case 0xFF49:				// Spr Palette (GB classic only)
                    ioRam[0x49] = val;
                    if (gbMode == GB)
                    {
                        updateClassicSprPalette(1);
                    }
                    return;
                case 0xFF68:				// BG Palette Index (GBC only)
                    ioRam[0x68] = val;
                    return;
                case 0xFF69:				// BG Palette Data (GBC only)
                    {
                        int index = ioRam[0x68] & 0x3F;
                        bgPaletteData[index] = val;
                        if (index%8 == 7)
                            updateBgPalette(index/8);

                        if (ioRam[0x68] & 0x80)
                            ioRam[0x68]++;
                        return;
                    }
                case 0xFF6B:				// Sprite Palette Data (GBC only)
                    {
                        int index = ioRam[0x6A] & 0x3F;
                        sprPaletteData[index] = val;
                        if (index%8 == 7)
                            updateSprPalette(index/8);

                        if (ioRam[0x6A] & 0x80)
                            ioRam[0x6A]++;
                        return;
                    }
                case 0xFF4D:
                    if (gbMode == CGB)
                    {
                        ioRam[0x4D] &= ~1;
                        ioRam[0x4D] |= (val&1);
                    }
                    return;
                case 0xFF4F:
                    if (gbMode == CGB)
                    {
                        vramBank = val & 1;
                        refreshVramBank();
                    }
                    ioRam[0x4F] = val&1;
                    return;
                    // Special register, used by the gameboy bios
                case 0xFF50:
                    biosOn = 0;
                    memory[0x0] = rom[0];
                    if (rom[0][0x143] == 0x80 || rom[0][0x143] == 0xC0)
                        gbMode = CGB;
                    else
                        gbMode = GB;
                    return;
                case 0xFF55: // CGB DMA
                    if (gbMode == CGB)
                    {
                        if (dmaLength > 0)
                        {
                            if ((val&0x80) == 0)
                            {
                                ioRam[0x55] |= 0x80;
                                dmaLength = 0;
                            }
                            return;
                        }
                        int i;
                        dmaLength = ((val & 0x7F)+1);
                        int length = dmaLength*0x10;
                        int source = (ioRam[0x51]<<8) | (ioRam[0x52]);
                        source &= 0xFFF0;
                        int dest = (ioRam[0x53]<<8) | (ioRam[0x54]);
                        dest &= 0x1FF0;
                        dmaSource = source;
                        dmaDest = dest;
                        dmaMode = val&0x80;
                        ioRam[0x55] = dmaLength-1;
                        if (dmaMode == 0)
                        {
                            int i;
                            for (i=0; i<dmaLength; i++)
                            {
                                writeVram16(dest, source);
                                dest += 0x10;
                                source += 0x10;
                            }
                            totalCycles += dmaLength*8*(doubleSpeed+1);
                            dmaLength = 0;
                            ioRam[0x55] = 0xFF;
                        }
                    }
                    else
                        ioRam[0x55] = val;
                    return;
                case 0xFF6C:
                case 0xFF72:
                case 0xFF73:
                case 0xFF74:
                case 0xFF75:
                case 0xFF76:
                case 0xFF77:
                    ioRam[addr&0xff] = val;
                    return;
                case 0xFF70:				// WRAM bank, for CGB only
                    if (gbMode == CGB)
                    {
                        wramBank = val & 0x7;
                        if (wramBank == 0)
                            wramBank = 1;
                        refreshWramBank();
                    }
                    ioRam[0x70] = val&0x7;
                    return;
                case 0xFF0F:
                    ioRam[0x0f] = val;
                    if (val & ioRam[0xff])
                        cyclesToExecute = 0;
                    break;
                case 0xFFFF:
                    ioRam[0xff] = val;
                    if (val & ioRam[0x0f])
                        cyclesToExecute = 0;
                    break;
                default:
                    if (addr >= 0xfe00 && addr < 0xfea0) {
                    }
                    hram[addr&0x1ff] = val;
                    return;
            }
            return;
        default:
            return;
            //memory[addr] = val;
    }
}

void writehword(u16 addr, u16 val)
{
    writeMemory(addr, val&0xFF);
    writeMemory(addr+1, val>>8);
}

bool updateHblankDMA()
{
    static int dmaCycles;

    if (dmaLength > 0)
    {
        int i;
        for (i=0; i<0x10; i++)
        {
            writeVram((dmaDest++)&0x1fff, readMemory(dmaSource++));
        }
        dmaLength --;
        if (dmaLength == 0)
        {
            ioRam[0x55] = 0xFF;
        }
        else
            ioRam[0x55] = dmaLength-1;
        return true;
    }
    else
        return false;
}

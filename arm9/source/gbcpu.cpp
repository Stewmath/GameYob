#ifndef DS
#include <string.h>
#endif
#include <stddef.h>
#include <stdio.h>
#include "gbcpu.h"
#include "mmu.h"
#include "gbgfx.h"
#include "gbsnd.h"
#include "gameboy.h"
#include "main.h"
#ifdef DS
#include <nds.h>
#endif

#define setZFlag()		locF |= 0x80
#define clearZFlag()	locF &= 0x7F
#define setNFlag()		locF |= 0x40
#define clearNFlag()	locF &= 0xBF
#define setHFlag()		locF |= 0x20
#define clearHFlag()	locF &= 0xDF
#define setCFlag()		locF |= 0x10
#define clearCFlag()	locF &= 0xEF
#define carrySet() 	(locF & 0x10 ? 1 : 0)
#define zeroSet()	(locF & 0x80 ? 1 : 0)
#define negativeSet()	(locF & 0x40 ? 1 : 0)
#define halfSet()		(locF & 0x20 ? 1 : 0)
#define numberedGbReg(n)	((u8 *) &gbRegs + reg8Offsets[n])

inline u8 quickRead(u16 addr) {
    return memory[addr>>12][addr&0xFFF];
}
inline u8 quickReadIO(u8 addr) {
    return ioRam[addr];
}
inline u16 quickRead16(u16 addr) {
    return quickRead(addr)|(quickRead(addr+1)<<8);
}

// Currently unused because this can actually overwrite the rom, in rare cases
inline void quickWrite(u16 addr, u8 val) {
    memory[addr>>12][addr&0xFFF] = val;
}

struct Registers gbRegs
#ifdef DS
DTCM_BSS
#endif
;
int halt;

u8 opCycles[0x100]
#ifdef DS
DTCM_DATA
#endif
= {
    /* Low nybble -> */
    /* High nybble v */
    /*         0, 1, 2, 3, 4, 5, 6, 7, 8, 9, A, B, C, D, E, F  */
    /* 0X */   4,12, 8, 8, 4, 4, 8, 4,20, 8, 8, 8, 4, 4, 8, 4,
    /* 1X */   4,12, 8, 8, 4, 4, 8, 4,12, 8, 8, 8, 4, 4, 8, 4,
    /* 2X */  12,12, 8, 8, 4, 4, 8, 4,12, 8, 8, 8, 4, 4, 8, 4,
    /* 3X */  12,12, 8, 8,12,12,12, 4,12, 8, 8, 8, 4, 4, 8, 4,
    /* 4X */   4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
    /* 5X */   4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
    /* 6X */   4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
    /* 7X */   8, 8, 8, 8, 8, 8, 4, 8, 4, 4, 4, 4, 4, 4, 8, 4,
    /* 8X */   4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
    /* 9X */   4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
    /* AX */   4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
    /* BX */   4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
    /* CX */  20,12,16,16,24,16, 8,16,20,16,16, 0,24,24, 8,16,
    /* DX */  20,12,16,99,24,16, 8,16,20,16,16,99,24,99, 8,16,
    /* EX */  12,12, 8,99,99,16, 8,16,16, 4,16,99,99,99, 8,16,
    /* FX */  12,12, 8, 4,99,16, 8,16,12, 8,16, 4,99,99, 8,16
        /* opcodes that have 99 cycles are undefined, but don't hang on them */
};

u8 CBopCycles[0x100]
#ifdef DS
DTCM_DATA
#endif
= {
    /* Low nybble -> */
    /* High nybble v */
    /*  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, A, B, C, D, E, F  */
    /* 0X */   8, 8, 8, 8, 8, 8,16, 8, 8, 8, 8, 8, 8, 8,16, 8,
    /* 1X */   8, 8, 8, 8, 8, 8,16, 8, 8, 8, 8, 8, 8, 8,16, 8,
    /* 2X */   8, 8, 8, 8, 8, 8,16, 8, 8, 8, 8, 8, 8, 8,16, 8,
    /* 3X */   8, 8, 8, 8, 8, 8,16, 8, 8, 8, 8, 8, 8, 8,16, 8,
    /* 4X */   8, 8, 8, 8, 8, 8,12, 8, 8, 8, 8, 8, 8, 8,12, 8,
    /* 5X */   8, 8, 8, 8, 8, 8,12, 8, 8, 8, 8, 8, 8, 8,12, 8,
    /* 6X */   8, 8, 8, 8, 8, 8,12, 8, 8, 8, 8, 8, 8, 8,12, 8,
    /* 7X */   8, 8, 8, 8, 8, 8,12, 8, 8, 8, 8, 8, 8, 8,12, 8,
    /* 8X */   8, 8, 8, 8, 8, 8,16, 8, 8, 8, 8, 8, 8, 8,16, 8,
    /* 9X */   8, 8, 8, 8, 8, 8,16, 8, 8, 8, 8, 8, 8, 8,16, 8,
    /* AX */   8, 8, 8, 8, 8, 8,16, 8, 8, 8, 8, 8, 8, 8,16, 8,
    /* BX */   8, 8, 8, 8, 8, 8,16, 8, 8, 8, 8, 8, 8, 8,16, 8,
    /* CX */   8, 8, 8, 8, 8, 8,16, 8, 8, 8, 8, 8, 8, 8,16, 8,
    /* DX */   8, 8, 8, 8, 8, 8,16, 8, 8, 8, 8, 8, 8, 8,16, 8,
    /* EX */   8, 8, 8, 8, 8, 8,16, 8, 8, 8, 8, 8, 8, 8,16, 8,
    /* FX */   8, 8, 8, 8, 8, 8,16, 8, 8, 8, 8, 8, 8, 8,16, 8
};


// IMPORTANT: This variable is unchanging, it DOES NOT change in double speed mode!
const int clockSpeed = 4194304;

int ime;

void initCPU()
{
    gbRegs.sp.w = 0xFFFE;
    ime = 1;			// Correct default value?
    halt = 0;

    if (biosOn)
    {
        gbRegs.pc.w = 0;
        gbMode = CGB;
    }
    else
    {
        gbRegs.pc.w = 0x100;
        initGameboyMode();
    }
}

void enableInterrupts()
{
    ime = 1;
    if (ioRam[0x0f] & ioRam[0xff])
        cyclesToExecute = -1;
}

void disableInterrupts()
{
    ime = 0;
}

int handleInterrupts(unsigned int interruptTriggered)
{
    const u16 isrVectors[] = { 0x40, 0x48, 0x50, 0x58, 0x60 };
    /* Halt state is always reset */
    halt = 0;
    /* Avoid processing irqs */
    if (!ime)
        return 0;

    ime = 0;

    writeMemory(--gbRegs.sp.w, gbRegs.pc.b.h);
    writeMemory(--gbRegs.sp.w, gbRegs.pc.b.l);

    /* __builtin_ffs returns the first bit set plus one */
    int irqNo = __builtin_ffs(interruptTriggered) - 1;
    /* Jump to the vector */
    gbRegs.pc.w = isrVectors[irqNo];
    /* Clear the IF bit */
    ioRam[0x0F] &= ~(1<<irqNo);

    /* The interrupt prologue takes 20 cycles, take it into account */
    return 20;
}

const u8 reg8Offsets[] = {
    offsetof(struct Registers, bc.b.h),
    offsetof(struct Registers, bc.b.l),
    offsetof(struct Registers, de.b.h),
    offsetof(struct Registers, de.b.l),
    offsetof(struct Registers, hl.b.h),
    offsetof(struct Registers, hl.b.l),
    0,
    offsetof(struct Registers, af.b.h)
};

int cyclesToExecute;

#ifdef DS
int runOpcode(int cycles) ITCM_CODE;
#endif

#define setPC(val) { gbRegs.pc.w = (val); pcAddr = &memory[(gbRegs.pc.w)>>12][(gbRegs.pc.w)&0xfff]; firstPcAddr=pcAddr;} 
#define getPC() (gbRegs.pc.w+(pcAddr-firstPcAddr))
#define readPC() *(pcAddr++)
#define readPC_noinc() (*pcAddr)
#define readPC16() ((*pcAddr) | ((*(pcAddr+1))<<8)); pcAddr += 2
#define readPC16_noinc() ((*pcAddr) | ((*(pcAddr+1))<<8))

#define OP_JR(cond)  \
                if (cond) { \
                    setPC((getPC()+(s8)readPC_noinc()+1)&0xffff); \
                } \
                else { \
                    pcAddr++; \
                    totalCycles -= 4; \
                }

u8* firstPcAddr;
int runOpcode(int cycles) {
    cyclesToExecute = cycles;
    // Having these commonly-used registers in local variables should improve speed
    u8* pcAddr;
    pcAddr = &memory[gbRegs.pc.w>>12][gbRegs.pc.w&0xfff];
    firstPcAddr = pcAddr;
    int locSP=gbRegs.sp.w;
    int  locF =gbRegs.af.b.l;

    int totalCycles=0;

    while (totalCycles < cyclesToExecute)
    {
        u8 opcode = *pcAddr;
        pcAddr++;
        totalCycles += opCycles[opcode];

        switch(opcode)
        {
            // 8-bit loads
            case 0x06:		// LD B, n		8
            case 0x0E:		// LD C, n		8
            case 0x16:		// LD D, n		8
            case 0x1E:		// LD E, n		8
            case 0x26:		// LD H, n		8
            case 0x2E:		// LD L, n		8
                (*numberedGbReg(opcode>>3)) = readPC();
                break;
            case 0x3E:		// LD A, n		8
                gbRegs.af.b.h = readPC();
                break;
                /* These are equivalent to NOPs. */
            case 0x7F:		// LD A, A		4
            case 0x40:		// LD B, B		4
            case 0x49:		// LD C, C		4
            case 0x52:		// LD D, D		4
            case 0x5B:		// LD E, E		4
            case 0x64:		// LD H, H		4
            case 0x6D:		// LD L, L		4
                break;
            case 0x78:		// LD A, B		4
                gbRegs.af.b.h = gbRegs.bc.b.h;
                break;
            case 0x79:		// LD A, C		4
                gbRegs.af.b.h = gbRegs.bc.b.l;
                break;
            case 0x7A:		// LD A, D		4
                gbRegs.af.b.h = gbRegs.de.b.h;
                break;
            case 0x7B:		// LD A, E		4
                gbRegs.af.b.h = gbRegs.de.b.l;
                break;
            case 0x7C:		// LD A, H		4
                gbRegs.af.b.h = gbRegs.hl.b.h;
                break;
            case 0x7D:		// LD A, L		4
                gbRegs.af.b.h = gbRegs.hl.b.l;
                break;
            case 0x41:		// LD B, C		4
            case 0x42:		// LD B, D		4
            case 0x43:		// LD B, E		4
            case 0x44:		// LD B, H		4
            case 0x45:		// LD B, L		4
            case 0x48:		// LD C, B		4
            case 0x4A:		// LD C, D		4
            case 0x4B:		// LD C, E		4
            case 0x4C:		// LD C, H		4
            case 0x4D:		// LD C, L		4
            case 0x50:		// LD D, B		4
            case 0x51:		// LD D, C		4
            case 0x53:		// LD D, E		4
            case 0x54:		// LD D, H		4
            case 0x55:		// LD D, L		4
            case 0x58:		// LD E, B		4
            case 0x59:		// LD E, C		4
            case 0x5A:		// LD E, D		4
            case 0x5C:		// LD E, H		4
            case 0x5D:		// LD E, L		4
            case 0x60:		// LD H, B		4
            case 0x61:		// LD H, C		4
            case 0x62:		// LD H, D		4
            case 0x63:		// LD H, E		4
            case 0x65:		// LD H, L		4
            case 0x68:		// LD L, B		4
            case 0x69:		// LD L, C		4
            case 0x6A:		// LD L, D		4
            case 0x6B:		// LD L, E		4
            case 0x6C:		// LD L, H		4
                (*numberedGbReg((opcode>>3)&7)) = *numberedGbReg(opcode&7);
                break;
            case 0x47:		// LD B, A		4
                gbRegs.bc.b.h = gbRegs.af.b.h;
                break;
            case 0x4F:		// LD C, A		4
                gbRegs.bc.b.l = gbRegs.af.b.h;
                break;
            case 0x57:		// LD D, A		4
                gbRegs.de.b.h = gbRegs.af.b.h;
                break;
            case 0x5F:		// LD E, A		4
                gbRegs.de.b.l = gbRegs.af.b.h;
                break;
            case 0x67:		// LD H, A		4
                gbRegs.hl.b.h = gbRegs.af.b.h;
                break;
            case 0x6F:		// LD L, A		4
                gbRegs.hl.b.l = gbRegs.af.b.h;
                break;
            case 0x7E:		// LD A, (hl)	8
                gbRegs.af.b.h = readMemory(gbRegs.hl.w);
                break;
            case 0x46:		// LD B, (hl)	8
            case 0x4E:		// LD C, (hl)	8
            case 0x56:		// LD D, (hl)	8
            case 0x5E:		// LD E, (hl)	8
            case 0x66:		// LD H, (hl)	8
            case 0x6E:		// LD L, (hl)	8
                (*numberedGbReg((opcode>>3)&7)) = readMemory(gbRegs.hl.w);
                break;
            case 0x77:		// LD (hl), A	8
                writeMemory(gbRegs.hl.w, gbRegs.af.b.h);
                break;
            case 0x70:		// LD (hl), B	8
            case 0x71:		// LD (hl), C	8
            case 0x72:		// LD (hl), D	8
            case 0x73:		// LD (hl), E	8
            case 0x74:		// LD (hl), H	8
            case 0x75:		// LD (hl), L	8
                writeMemory(gbRegs.hl.w, *numberedGbReg(opcode&7));
                break;
            case 0x36:		// LD (hl), n	12
                writeMemory(gbRegs.hl.w, readPC_noinc());
                pcAddr++;
                break;
            case 0x0A:		// LD A, (BC)	8
                gbRegs.af.b.h = readMemory(gbRegs.bc.w);
                break;
            case 0x1A:		// LD A, (de)	8
                gbRegs.af.b.h = readMemory(gbRegs.de.w);
                break;
            case 0xFA:		// LD A, (nn)	16
                gbRegs.af.b.h = readMemory(readPC16_noinc());
                pcAddr += 2;
                break;
            case 0x02:		// LD (BC), A	8
                writeMemory(gbRegs.bc.w, gbRegs.af.b.h);
                break;
            case 0x12:		// LD (de), A	8
                writeMemory(gbRegs.de.w, gbRegs.af.b.h);
                break;
            case 0xEA:		// LD (nn), A	16
                writeMemory(readPC16_noinc(), gbRegs.af.b.h);
                pcAddr += 2;
                break;
            case 0xF2:		// LDH A, (C)	8
                gbRegs.af.b.h = readIO(gbRegs.bc.b.l);
                break;
            case 0xE2:		// LDH (C), A	8
                writeIO(gbRegs.bc.b.l, gbRegs.af.b.h);
                break;
            case 0x3A:		// LDD A, (hl)	8
                gbRegs.af.b.h = readMemory(gbRegs.hl.w--);
                break;
            case 0x32:		// LDD (hl), A	8
                writeMemory(gbRegs.hl.w--, gbRegs.af.b.h);
                break;
            case 0x2A:		// LDI A, (hl)	8
                gbRegs.af.b.h = readMemory(gbRegs.hl.w++);
                break;
            case 0x22:		// LDI (hl), A	8
                writeMemory(gbRegs.hl.w++, gbRegs.af.b.h);
                break;
            case 0xE0:		// LDH (n), A   12
                writeIO(readPC_noinc(), gbRegs.af.b.h);
                pcAddr++;
                break;
            case 0xF0:		// LDH A, (n)   12
                gbRegs.af.b.h = readIO(readPC_noinc());
                pcAddr++;
                break;

                // 16-bit loads

            case 0x01:		// LD BC, nn	12
                gbRegs.bc.w = readPC16();
                break;
            case 0x11:		// LD de, nn	12
                gbRegs.de.w = readPC16();
                break;
            case 0x21:		// LD hl, nn	12
                gbRegs.hl.w = readPC16();
                break;
            case 0x31:		// LD SP, nn	12
                locSP = readPC16();
                break;
            case 0xF9:		// LD SP, hl	8
                locSP = gbRegs.hl.w;
                break;
            case 0xF8:		// LDHL SP, n   12
                {
                    int val = readPC();
                    if (((locSP&0xFF)+val) > 0xFF)
                        setCFlag();
                    else
                        clearCFlag();
                    if ((locSP&0xF)+(val&0xF) > 0xF)
                        setHFlag();
                    else
                        clearHFlag();
                    clearNFlag();
                    clearZFlag();
                    gbRegs.hl.w = locSP+(s8)val;
                    break;
                }
            case 0x08:		// LD (nn), SP	20
                {
                    int val = readPC16();
                    writeMemory(val, locSP & 0xFF);
                    writeMemory(val+1, (locSP) >> 8);
                    break;
                }
            case 0xF5:		// PUSH AF
                writeMemory(--locSP, gbRegs.af.b.h);
                writeMemory(--locSP, locF);
                break;
                // Some games use the stack in exotic ways.
                // Better to use writeMemory than writeMemory.
            case 0xC5:		// PUSH BC			16
                writeMemory(--locSP, gbRegs.bc.b.h);
                writeMemory(--locSP, gbRegs.bc.b.l);
                break;
            case 0xD5:		// PUSH de			16
                writeMemory(--locSP, gbRegs.de.b.h);
                writeMemory(--locSP, gbRegs.de.b.l);
                break;
            case 0xE5:		// PUSH hl			16
                writeMemory(--locSP, gbRegs.hl.b.h);
                writeMemory(--locSP, gbRegs.hl.b.l);
                break;
            case 0xF1:		// POP AF				12
                locF = quickRead(locSP++) & 0xF0;
                gbRegs.af.b.h = quickRead(locSP++);
                break;
            case 0xC1:		// POP BC				12
                gbRegs.bc.w = quickRead16(locSP);
                locSP += 2;
                break;
            case 0xD1:		// POP de				12
                gbRegs.de.w = quickRead16(locSP);
                locSP += 2;
                break;
            case 0xE1:		// POP hl				12
                gbRegs.hl.w = quickRead16(locSP);
                locSP += 2;
                break;

                // 8-bit arithmetic
            case 0x87:		// ADD A, A			4
                {
                    u8 r = gbRegs.af.b.h;
                    if (r + r > 0xFF)
                        setCFlag();
                    else
                        clearCFlag();
                    if ((r & 0xF) + (r & 0xF) > 0xF)
                        setHFlag();
                    else
                        clearHFlag();
                    gbRegs.af.b.h += r;
                    if (gbRegs.af.b.h == 0)
                        setZFlag();
                    else
                        clearZFlag();
                    clearNFlag();
                    break;
                }
            case 0x80:		// ADD A, B			4
            case 0x81:		// ADD A, C			4
            case 0x82:		// ADD A, D			4
            case 0x83:		// ADD A, E			4
            case 0x84:		// ADD A, H			4
            case 0x85:		// ADD A, L			4
                {
                    u8 r = *numberedGbReg(opcode&7);
                    if (gbRegs.af.b.h + r > 0xFF)
                        setCFlag();
                    else
                        clearCFlag();
                    if ((gbRegs.af.b.h & 0xF) + (r & 0xF) > 0xF)
                        setHFlag();
                    else
                        clearHFlag();
                    gbRegs.af.b.h += r;
                    if (gbRegs.af.b.h == 0)
                        setZFlag();
                    else
                        clearZFlag();
                    clearNFlag();
                    break;
                }
            case 0x86:		// ADD A, (hl)	8
                {
                    int val = readMemory(gbRegs.hl.w);
                    if (gbRegs.af.b.h + val > 0xFF)
                        setCFlag();
                    else
                        clearCFlag();
                    if ((gbRegs.af.b.h & 0xF) + (val & 0xF) > 0xF)
                        setHFlag();
                    else
                        clearHFlag();
                    gbRegs.af.b.h += val;
                    if (gbRegs.af.b.h == 0)
                        setZFlag();
                    else
                        clearZFlag();
                    clearNFlag();
                    break;
                }
            case 0xC6:		// ADD A, n			8
                {
                    int val = readPC();
                    if (gbRegs.af.b.h + val > 0xFF)
                        setCFlag();
                    else
                        clearCFlag();
                    if ((gbRegs.af.b.h & 0xF) + (val & 0xF) > 0xF)
                        setHFlag();
                    else
                        clearHFlag();
                    gbRegs.af.b.h += val;
                    if (gbRegs.af.b.h == 0)
                        setZFlag();
                    else
                        clearZFlag();
                    clearNFlag();
                    break;
                }


            case 0x8F:		// ADC A, A			4
                {
                    int val = carrySet();
                    u8 r = gbRegs.af.b.h;
                    if (r + r + val > 0xFF)
                        setCFlag();
                    else
                        clearCFlag();
                    if ((r & 0xF) + (r & 0xF) + val > 0xF)
                        setHFlag();
                    else
                        clearHFlag();
                    gbRegs.af.b.h += r + val;
                    if (gbRegs.af.b.h == 0)
                        setZFlag();
                    else
                        clearZFlag();
                    clearNFlag();
                    break;
                }
            case 0x88:		// ADC A, B			4
            case 0x89:		// ADC A, C			4
            case 0x8A:		// ADC A, D			4
            case 0x8B:		// ADC A, E			4
            case 0x8C:		// ADC A, H			4
            case 0x8D:		// ADC A, L			4
                {
                    int val = carrySet();
                    u8 r = *numberedGbReg(opcode&7);
                    if (gbRegs.af.b.h + r + val > 0xFF)
                        setCFlag();
                    else
                        clearCFlag();
                    if ((gbRegs.af.b.h & 0xF) + (r & 0xF) + val > 0xF)
                        setHFlag();
                    else
                        clearHFlag();
                    gbRegs.af.b.h += r + val;
                    if (gbRegs.af.b.h == 0)
                        setZFlag();
                    else
                        clearZFlag();
                    clearNFlag();
                    break;
                }
            case 0x8E:		// ADC A, (hl)	8
                {
                    int val = readMemory(gbRegs.hl.w);
                    int val2 = carrySet();
                    if (gbRegs.af.b.h + val + val2 > 0xFF)
                        setCFlag();
                    else
                        clearCFlag();
                    if ((gbRegs.af.b.h & 0xF) + (val & 0xF) + val2 > 0xF)
                        setHFlag();
                    else
                        clearHFlag();
                    gbRegs.af.b.h += val + val2;
                    if (gbRegs.af.b.h == 0)
                        setZFlag();
                    else
                        clearZFlag();
                    clearNFlag();
                    break;
                }
            case 0xCE:		// ADC A, n			8
                {
                    int val = readPC();
                    int val2 = carrySet();
                    if (gbRegs.af.b.h + val + val2 > 0xFF)
                        setCFlag();
                    else
                        clearCFlag();
                    if ((gbRegs.af.b.h & 0xF) + (val & 0xF) + val2 > 0xF)
                        setHFlag();
                    else
                        clearHFlag();
                    gbRegs.af.b.h += val + val2;
                    if (gbRegs.af.b.h == 0)
                        setZFlag();
                    else
                        clearZFlag();
                    clearNFlag();
                    break;
                }

            case 0x97:		// SUB A, A			4
                {
                    gbRegs.af.b.h = 0;
                    clearCFlag();
                    clearHFlag();
                    setZFlag();
                    setNFlag();
                    break;
                }
            case 0x90:		// SUB A, B			4
            case 0x91:		// SUB A, C			4
            case 0x92:		// SUB A, D			4
            case 0x93:		// SUB A, E			4
            case 0x94:		// SUB A, H			4
            case 0x95:		// SUB A, L			4
                {
                    u8 r = *numberedGbReg(opcode&7);
                    if (gbRegs.af.b.h < r)
                        setCFlag();
                    else
                        clearCFlag();
                    if ((gbRegs.af.b.h & 0xF) < (r & 0xF))
                        setHFlag();
                    else
                        clearHFlag();
                    gbRegs.af.b.h -= r;
                    if (gbRegs.af.b.h == 0)
                        setZFlag();
                    else
                        clearZFlag();
                    setNFlag();
                    break;
                }
            case 0x96:		// SUB A, (hl)	8
                {
                    int val = readMemory(gbRegs.hl.w);
                    if (gbRegs.af.b.h < val)
                        setCFlag();
                    else
                        clearCFlag();
                    if ((gbRegs.af.b.h & 0xF) < (val & 0xF))
                        setHFlag();
                    else
                        clearHFlag();
                    gbRegs.af.b.h -= val;
                    if (gbRegs.af.b.h == 0)
                        setZFlag();
                    else
                        clearZFlag();
                    setNFlag();
                    break;
                }
            case 0xD6:		// SUB A, n			8
                {
                    int val = readPC();
                    if (gbRegs.af.b.h < val)
                        setCFlag();
                    else
                        clearCFlag();
                    if ((gbRegs.af.b.h & 0xF) < (val & 0xF))
                        setHFlag();
                    else
                        clearHFlag();
                    gbRegs.af.b.h -= val;
                    if (gbRegs.af.b.h == 0)
                        setZFlag();
                    else
                        clearZFlag();
                    setNFlag();
                    break;

                }
            case 0x9F:		// SBC A, A			4
                {
                    u8 r = gbRegs.af.b.h;
                    int val2 = carrySet();
                    if (val2 /* != 0 */) {
                        setCFlag();
                        setHFlag();
                    }
                    else {
                        clearCFlag();
                        clearHFlag();
                    }
                    gbRegs.af.b.h -= (r + val2);
                    if (gbRegs.af.b.h == 0)
                        setZFlag();
                    else
                        clearZFlag();
                    setNFlag();
                    break;
                }
            case 0x98:		// SBC A, B			4
            case 0x99:		// SBC A, C			4
            case 0x9A:		// SBC A, D			4
            case 0x9B:		// SBC A, E			4
            case 0x9C:		// SBC A, H			4
            case 0x9D:		// SBC A, L			4
                {
                    u8 r = *numberedGbReg(opcode&7);
                    int val2 = carrySet();
                    if (gbRegs.af.b.h < r + val2)
                        setCFlag();
                    else
                        clearCFlag();
                    if ((gbRegs.af.b.h & 0xF) < (r & 0xF) + val2)
                        setHFlag();
                    else
                        clearHFlag();
                    gbRegs.af.b.h -= (r + val2);
                    if (gbRegs.af.b.h == 0)
                        setZFlag();
                    else
                        clearZFlag();
                    setNFlag();
                    break;
                }
            case 0x9E:		// SBC A, (hl)	8
                {
                    int val2 = carrySet();
                    int val = readMemory(gbRegs.hl.w);
                    if (gbRegs.af.b.h < val + val2)
                        setCFlag();
                    else
                        clearCFlag();
                    if ((gbRegs.af.b.h & 0xF) < (val & 0xF)+val2)
                        setHFlag();
                    else
                        clearHFlag();
                    gbRegs.af.b.h -= val + val2;
                    if (gbRegs.af.b.h == 0)
                        setZFlag();
                    else
                        clearZFlag();
                    setNFlag();
                    break;
                }
            case 0xde:		// SBC A, n			4
                {
                    int val = readPC();
                    int val2 = carrySet();
                    if (gbRegs.af.b.h <val + val2)
                        setCFlag();
                    else
                        clearCFlag();
                    if ((gbRegs.af.b.h & 0xF) < (val & 0xF)+val2)
                        setHFlag();
                    else
                        clearHFlag();
                    gbRegs.af.b.h -= (val + val2);
                    if (gbRegs.af.b.h == 0)
                        setZFlag();
                    else
                        clearZFlag();
                    setNFlag();
                    break;
                }

            case 0xA7:		// AND A, A		4
                if (gbRegs.af.b.h == 0)
                    setZFlag();
                else
                    clearZFlag();
                clearNFlag();
                setHFlag();
                clearCFlag();
                break;
            case 0xA0:		// AND A, B		4
            case 0xA1:		// AND A, C		4
            case 0xA2:		// AND A, D		4
            case 0xA3:		// AND A, E		4
            case 0xA4:		// AND A, H		4
            case 0xA5:		// AND A, L		4
                gbRegs.af.b.h &= *numberedGbReg(opcode&7);
                if (gbRegs.af.b.h == 0)
                    setZFlag();
                else
                    clearZFlag();
                clearNFlag();
                setHFlag();
                clearCFlag();
                break;
            case 0xA6:		// AND A, (hl)	8
                gbRegs.af.b.h &= readMemory(gbRegs.hl.w);
                if (gbRegs.af.b.h == 0)
                    setZFlag();
                else
                    clearZFlag();
                clearNFlag();
                setHFlag();
                clearCFlag();
                break;
            case 0xE6:		// AND A, n			8
                gbRegs.af.b.h &= readPC();
                if (gbRegs.af.b.h == 0)
                    setZFlag();
                else
                    clearZFlag();
                clearNFlag();
                setHFlag();
                clearCFlag();
                break;

            case 0xB7:		// OR A, A			4
                if (gbRegs.af.b.h == 0)
                    setZFlag();
                else
                    clearZFlag();
                clearNFlag();
                clearHFlag();
                clearCFlag();
                break;
            case 0xB0:		// OR A, B			4
            case 0xB1:		// OR A, C			4
            case 0xB2:		// OR A, D			4
            case 0xB3:		// OR A, E			4
            case 0xB4:		// OR A, H			4
            case 0xB5:		// OR A, L			4
                gbRegs.af.b.h |= *numberedGbReg(opcode&7);
                if (gbRegs.af.b.h == 0)
                    setZFlag();
                else
                    clearZFlag();
                clearNFlag();
                clearHFlag();
                clearCFlag();
                break;
            case 0xB6:		// OR A, (hl)		8
                gbRegs.af.b.h |= readMemory(gbRegs.hl.w);
                if (gbRegs.af.b.h == 0)
                    setZFlag();
                else
                    clearZFlag();
                clearNFlag();
                clearHFlag();
                clearCFlag();
                break;
            case 0xF6:		// OR A, n			4
                gbRegs.af.b.h |= readPC();
                if (gbRegs.af.b.h == 0)
                    setZFlag();
                else
                    clearZFlag();
                clearNFlag();
                clearHFlag();
                clearCFlag();
                break;

            case 0xAF:		// XOR A, A			4
                gbRegs.af.b.h = 0;
                setZFlag();
                clearNFlag();
                clearHFlag();
                clearCFlag();
                break;
            case 0xA8:		// XOR A, B			4
            case 0xA9:		// XOR A, C			4
            case 0xAA:		// XOR A, D			4
            case 0xAB:		// XOR A, E			4
            case 0xAC:		// XOR A, H			4
            case 0xAD:		// XOR A, L			4
                gbRegs.af.b.h ^= *numberedGbReg(opcode&7);
                if (gbRegs.af.b.h == 0)
                    setZFlag();
                else
                    clearZFlag();
                clearNFlag();
                clearHFlag();
                clearCFlag();
                break;
            case 0xAE:		// XOR A, (hl)	8
                gbRegs.af.b.h ^= readMemory(gbRegs.hl.w);
                if (gbRegs.af.b.h == 0)
                    setZFlag();
                else
                    clearZFlag();
                clearNFlag();
                clearHFlag();
                clearCFlag();
                break;
            case 0xEE:		// XOR A, n			8
                gbRegs.af.b.h ^= readPC();
                if (gbRegs.af.b.h == 0)
                    setZFlag();
                else
                    clearZFlag();
                clearNFlag();
                clearHFlag();
                clearCFlag();
                break;

            case 0xBF:		// CP A					4
                {
                    clearCFlag();
                    clearHFlag();
                    setZFlag();
                    setNFlag();
                    break;
                }
            case 0xB8:		// CP B					4
            case 0xB9:		// CP C				4
            case 0xBA:		// CP D					4
            case 0xBB:		// CP E					4
            case 0xBC:		// CP H					4
            case 0xBD:		// CP L					4
                {
                    u8 r = *numberedGbReg(opcode&7);
                    if (gbRegs.af.b.h < r)
                        setCFlag();
                    else
                        clearCFlag();
                    if ((gbRegs.af.b.h & 0xF) < (r & 0xF))
                        setHFlag();
                    else
                        clearHFlag();
                    if (gbRegs.af.b.h - r == 0)
                        setZFlag();
                    else
                        clearZFlag();
                    setNFlag();
                    break;
                }
            case 0xBE:		// CP (hl)			8
                {
                    int val = readMemory(gbRegs.hl.w);
                    if (gbRegs.af.b.h < val)
                        setCFlag();
                    else
                        clearCFlag();
                    if ((gbRegs.af.b.h & 0xF) < (val & 0xF))
                        setHFlag();
                    else
                        clearHFlag();
                    if (gbRegs.af.b.h - val == 0)
                        setZFlag();
                    else
                        clearZFlag();
                    setNFlag();
                    break;
                }
            case 0xFE:		// CP n					8
                {
                    int val = readPC();
                    if (gbRegs.af.b.h < val)
                        setCFlag();
                    else
                        clearCFlag();
                    if ((gbRegs.af.b.h & 0xF) < (val & 0xF))
                        setHFlag();
                    else
                        clearHFlag();
                    if (gbRegs.af.b.h - val == 0)
                        setZFlag();
                    else
                        clearZFlag();
                    setNFlag();
                    break;
                }


            case 0x3C:		// INC A				4
                {
                    gbRegs.af.b.h++;
                    if (gbRegs.af.b.h == 0)
                        setZFlag();
                    else
                        clearZFlag();
                    if ((gbRegs.af.b.h & 0xF) == 0)
                        setHFlag();
                    else
                        clearHFlag();
                    clearNFlag();
                    break;
                }
            case 0x04:		// INC B				4
            case 0x0C:		// INC C				4
            case 0x14:		// INC D				4
            case 0x1C:		// INC E				4
            case 0x24:		// INC H				4
            case 0x2C:		// INC L				4
                {
                    u8* reg = numberedGbReg(opcode>>3);
                    (*reg)++;
                    u8 r = *reg;
                    if (r == 0)
                        setZFlag();
                    else
                        clearZFlag();
                    if ((r & 0xF) == 0)
                        setHFlag();
                    else
                        clearHFlag();
                    clearNFlag();
                    break;
                }
            case 0x34:		// INC (hl)		12
                {
                    u8 val = readMemory(gbRegs.hl.w)+1;
                    writeMemory(gbRegs.hl.w, val);
                    if (val == 0)
                        setZFlag();
                    else
                        clearZFlag();
                    if ((val & 0xF) == 0)
                        setHFlag();
                    else
                        clearHFlag();
                    clearNFlag();
                    break;
                }

            case 0x3D:		// DEC A				4
                {
                    gbRegs.af.b.h--;
                    if (gbRegs.af.b.h == 0)
                        setZFlag();
                    else
                        clearZFlag();
                    if ((gbRegs.af.b.h & 0xF) == 0xF)
                        setHFlag();
                    else
                        clearHFlag();
                    setNFlag();
                    break;
                }
            case 0x05:		// DEC B				4
            case 0x0D:		// DEC C				4
            case 0x15:		// DEC D				4
            case 0x1D:		// DEC E				4
            case 0x25:		// DEC H				4
            case 0x2D:		// DEC L				4
                {
                    u8 *reg = numberedGbReg(opcode>>3);
                    (*reg)--;
                    u8 r = *reg;
                    if (r == 0)
                        setZFlag();
                    else
                        clearZFlag();
                    if ((r & 0xF) == 0xF)
                        setHFlag();
                    else
                        clearHFlag();
                    setNFlag();
                    break;
                }
            case 0x35:		// deC (hl)			12
                {
                    u8 val = readMemory(gbRegs.hl.w)-1;
                    writeMemory(gbRegs.hl.w, val);
                    if (val == 0)
                        setZFlag();
                    else
                        clearZFlag();
                    if ((val & 0xF) == 0xF)
                        setHFlag();
                    else
                        clearHFlag();
                    setNFlag();
                    break;
                }

                // 16-bit Arithmetic

            case 0x09:		// ADD hl, BC		8
                if (gbRegs.hl.w + gbRegs.bc.w > 0xFFFF)
                    setCFlag();
                else
                    clearCFlag();
                if ((gbRegs.hl.w & 0xFFF) + (gbRegs.bc.w & 0xFFF) > 0xFFF)
                    setHFlag();
                else
                    clearHFlag();
                clearNFlag();
                gbRegs.hl.w += gbRegs.bc.w;
                break;
            case 0x19:		// ADD hl, de		8
                if (gbRegs.hl.w + gbRegs.de.w > 0xFFFF)
                    setCFlag();
                else
                    clearCFlag();
                if ((gbRegs.hl.w & 0xFFF) + (gbRegs.de.w & 0xFFF) > 0xFFF)
                    setHFlag();
                else
                    clearHFlag();
                clearNFlag();
                gbRegs.hl.w += gbRegs.de.w;
                break;
            case 0x29:		// ADD hl, hl		8
                if (gbRegs.hl.w + gbRegs.hl.w > 0xFFFF)
                    setCFlag();
                else
                    clearCFlag();
                if ((gbRegs.hl.w & 0xFFF) + (gbRegs.hl.w & 0xFFF) > 0xFFF)
                    setHFlag();
                else
                    clearHFlag();
                clearNFlag();
                gbRegs.hl.w += gbRegs.hl.w;
                break;
            case 0x39:		// ADD hl, SP		8
                if (gbRegs.hl.w + locSP > 0xFFFF)
                    setCFlag();
                else
                    clearCFlag();
                if ((gbRegs.hl.w & 0xFFF) + (locSP & 0xFFF) > 0xFFF)
                    setHFlag();
                else
                    clearHFlag();
                clearNFlag();
                gbRegs.hl.w += locSP;
                break;

            case 0xE8:		// ADD SP, n		16
                {
                    int val = readPC();
                    if (((locSP&0xFF)+val) > 0xFF)
                        setCFlag();
                    else
                        clearCFlag();
                    if ((locSP&0xF)+(val&0xF) > 0xF)
                        setHFlag();
                    else
                        clearHFlag();
                    clearNFlag();
                    clearZFlag();
                    locSP += (s8)val;
                    break;
                }
            case 0x03:		// INC BC				8
                gbRegs.bc.w++;
                break;
            case 0x13:		// INC de				8
                gbRegs.de.w++;
                break;
            case 0x23:		// INC hl				8
                gbRegs.hl.w++;
                break;
            case 0x33:		// INC SP				8
                locSP++;
                break;

            case 0x0B:		// DEC BC				8
                gbRegs.bc.w--;
                break;
            case 0x1B:		// DEC de				8
                gbRegs.de.w--;
                break;
            case 0x2B:		// DEC hl				8
                gbRegs.hl.w--;
                break;
            case 0x3B:		// DEC SP				8
                locSP--;
                break;

            case 0x27:		// DAA					4
                {
                    int a = gbRegs.af.b.h;

                    if (!negativeSet())
                    {
                        if (halfSet() || (a & 0xF) > 9)
                            a += 0x06;

                        if (carrySet() || a > 0x9F)
                            a += 0x60;
                    }
                    else
                    {
                        if (halfSet())
                            a = (a - 6) & 0xFF;

                        if (carrySet())
                            a -= 0x60;
                    }

                    locF &= ~(0x20 | 0x80);

                    if ((a & 0x100) == 0x100)
                        setCFlag();

                    a &= 0xFF;

                    if (a == 0)
                        setZFlag();

                    gbRegs.af.b.h = a;

                }
                break;

            case 0x2F:		// CPL					4
                gbRegs.af.b.h = ~gbRegs.af.b.h;
                setNFlag();
                setHFlag();
                break;

            case 0x3F:		// CCF					4
                if (carrySet())
                    clearCFlag();
                else
                    setCFlag();
                clearNFlag();
                clearHFlag();
                break;

            case 0x37:		// SCF					4
                setCFlag();
                clearNFlag();
                clearHFlag();
                break;

            case 0x00:		// NOP					4
                break;

            case 0x76:		// HALT					4
                halt = 1;
                goto end;

            case 0x10:		// STOP					4
                if (ioRam[0x4D] & 1 && gbMode == CGB)
                {
                    if (ioRam[0x4D] & 0x80)
                    {
                        setDoubleSpeed(0);
                    }
                    else
                    {
                        setDoubleSpeed(1);
                    }

                    ioRam[0x4D] &= ~1;
                }
                else
                {
                    halt = 2;
                    goto end;
                }
                pcAddr++;
                break;

            case 0xF3:		// DI   4
                disableInterrupts();
                break;

            case 0xFB:		// EI   4
                enableInterrupts();
                break;

            case 0x07:		// RLCA 4
                {
                    int val = gbRegs.af.b.h;
                    gbRegs.af.b.h <<= 1;
                    if (val & 0x80)
                    {
                        setCFlag();
                        gbRegs.af.b.h |= 1;
                    }
                    else
                        clearCFlag();
                    clearZFlag();
                    clearNFlag();
                    clearHFlag();
                    break;
                }

            case 0x17:		// RLA					4
                {
                    int val = (gbRegs.af.b.h & 0x80);
                    gbRegs.af.b.h <<= 1;
                    gbRegs.af.b.h |= carrySet();
                    if (val)
                        setCFlag();
                    else
                        clearCFlag();
                    clearZFlag();
                    clearNFlag();
                    clearHFlag();
                    break;
                }

            case 0x0F:		// RRCA 4
                {
                    int val = gbRegs.af.b.h;
                    gbRegs.af.b.h >>= 1;
                    if ((val & 1))
                    {
                        setCFlag();
                        gbRegs.af.b.h |= 0x80;
                    }
                    else
                        clearCFlag();
                    clearZFlag();
                    clearNFlag();
                    clearHFlag();
                    break;
                }

            case 0x1F:		// RRA					4
                {
                    int val = gbRegs.af.b.h & 1;
                    gbRegs.af.b.h >>= 1;
                    gbRegs.af.b.h |= (carrySet() << 7);
                    if (val)
                        setCFlag();
                    else
                        clearCFlag();
                    clearZFlag();
                    clearNFlag();
                    clearHFlag();
                    break;
                }

            case 0xC3:		// JP				16
                setPC(readPC16_noinc());
                break;
            case 0xC2:		// JP NZ, nn	16/12
                if (!zeroSet())
                {
                    setPC(readPC16_noinc());
                    break;
                }
                else {
                    pcAddr += 2;
                    totalCycles -= 4;
                    break;
                }
            case 0xCA:		// JP Z, nn		16/12
                if (zeroSet())
                {
                    setPC(readPC16_noinc());
                    break;
                }
                else {
                    pcAddr += 2;
                    totalCycles -= 4;
                    break;
                }
            case 0xD2:		// JP NC, nn	16/12
                if (!carrySet())
                {
                    setPC(readPC16_noinc());
                    break;
                }
                else {
                    pcAddr += 2;
                    totalCycles -= 4;
                    break;
                }
            case 0xDA:		// JP C, nn	12
                if (carrySet())
                {
                    setPC(readPC16_noinc());
                    break;
                }
                else {
                    totalCycles -= 4;
                    pcAddr += 2;
                    break;
                }
            case 0xE9:		// JP (hl)	4
                setPC(gbRegs.hl.w);
                break;
            case 0x18:		// JR n 12
                OP_JR(true);
                break;
            case 0x20:		// JR NZ n  8/12
                OP_JR(!zeroSet());
                break;
            case 0x28:		// JR Z, n  8/12
                OP_JR(zeroSet());
                break;
            case 0x30:		// JR NC, n 8/12
                OP_JR(!carrySet());
                break;
            case 0x38:		// JR C, n  8/12
                OP_JR(carrySet());
                break;

            case 0xCD:		// CALL nn			24
                {
                    int val = getPC() + 2;
                    writeMemory(--locSP, (val) >> 8);
                    writeMemory(--locSP, (val & 0xFF));
                    setPC(readPC16_noinc());
                    break;
                }
            case 0xC4:		// CALL NZ, nn	12/24
                if (!zeroSet())
                {
                    int val = getPC() + 2;
                    writeMemory(--locSP, (val) >> 8);
                    writeMemory(--locSP, (val & 0xFF));
                    setPC(readPC16_noinc());
                    break;
                }
                else {
                    pcAddr += 2;
                    totalCycles -= 12;
                    break;
                }
            case 0xCC:		// CALL Z, nn		12/24
                if (zeroSet())
                {
                    int val = getPC() + 2;
                    writeMemory(--locSP, (val) >> 8);
                    writeMemory(--locSP, (val & 0xFF));
                    setPC(readPC16_noinc());
                    break;
                }
                else {
                    pcAddr += 2;
                    totalCycles -= 12;
                    break;
                }
            case 0xD4:		// CALL NC, nn	12/24
                if (!carrySet())
                {
                    int val = getPC() + 2;
                    writeMemory(--locSP, (val) >> 8);
                    writeMemory(--locSP, (val & 0xFF));
                    setPC(readPC16_noinc());
                    break;
                }
                else {
                    pcAddr += 2;
                    totalCycles -= 12;
                    break;
                }
            case 0xDC:		// CALL C, nn	12/24
                if (carrySet())
                {
                    int val = getPC() + 2;
                    writeMemory(--locSP, (val) >> 8);
                    writeMemory(--locSP, (val & 0xFF));
                    setPC(readPC16_noinc());
                    break;
                }
                else {
                    pcAddr += 2;
                    totalCycles -= 12;
                    break;
                }

            case 0xC7:		// RST 00H			16
                {
                    u16 val = getPC();
                    writeMemory(--locSP, (val) >> 8);
                    writeMemory(--locSP, (val & 0xFF));
                    setPC(0x0);
                }
                break;
            case 0xCF:		// RST 08H			16
                {
                    u16 val = getPC();
                    writeMemory(--locSP, (val) >> 8);
                    writeMemory(--locSP, (val & 0xFF));
                    setPC(0x8);
                    break;
                }
            case 0xD7:		// RST 10H			16
                {
                    u16 val = getPC();
                    writeMemory(--locSP, (val) >> 8);
                    writeMemory(--locSP, (val & 0xFF));
                    setPC(0x10);
                }
                break;
            case 0xDF:		// RST 18H			16
                {
                    u16 val = getPC();
                    writeMemory(--locSP, (val) >> 8);
                    writeMemory(--locSP, (val & 0xFF));
                    setPC(0x18);
                }
                break;
            case 0xE7:		// RST 20H			16
                {
                    u16 val = getPC();
                    writeMemory(--locSP, (val) >> 8);
                    writeMemory(--locSP, (val & 0xFF));
                    setPC(0x20);
                }
                break;
            case 0xEF:		// RST 28H			16
                {
                    u16 val = getPC();
                    writeMemory(--locSP, (val) >> 8);
                    writeMemory(--locSP, (val & 0xFF));
                    setPC(0x28);
                }
                break;
            case 0xF7:		// RST 30H			16
                {
                    u16 val = getPC();
                    writeMemory(--locSP, (val) >> 8);
                    writeMemory(--locSP, (val & 0xFF));
                    setPC(0x30);
                }
                break;
            case 0xFF:		// RST 38H			16
                {
                    u16 val = getPC();
                    writeMemory(--locSP, (val) >> 8);
                    writeMemory(--locSP, (val & 0xFF));
                    setPC(0x38);
                }
                break;

            case 0xC9:		// RET					16
                setPC(quickRead16(locSP));
                locSP += 2;
                break;
            case 0xC0:		// RET NZ				8/20
                if (!zeroSet())
                {
                    setPC(quickRead16(locSP));
                    locSP += 2;
                    break;
                }
                else {
                    totalCycles -= 12;
                    break;
                }
            case 0xC8:		// RET Z				8/20
                if (zeroSet())
                {
                    setPC(quickRead16(locSP));
                    locSP += 2;
                    break;
                }
                else {
                    totalCycles -= 16;
                    break;
                }
            case 0xD0:		// RET NC				8/20
                if (!carrySet())
                {
                    setPC(quickRead16(locSP));
                    locSP += 2;
                    break;
                }
                else {
                    totalCycles -= 16;
                    break;
                }
            case 0xD8:		// RET C				8/20
                if (carrySet())
                {
                    setPC(quickRead16(locSP));
                    locSP += 2;
                    break;
                }
                else {
                    totalCycles -= 16;
                    break;
                }
            case 0xD9:		// RETI					16
                setPC(quickRead16(locSP));
                locSP += 2;
                enableInterrupts();
                break;

            case 0xCB:
                opcode = readPC();
                totalCycles += CBopCycles[opcode];
                switch(opcode)
                {
                    case 0x37:		// SWAP A			8
                        {
                            u8 r = gbRegs.af.b.h;
                            int val = r >> 4;
                            r <<= 4;
                            r |= val;
                            if (r == 0)
                                setZFlag();
                            else
                                clearZFlag();
                            clearNFlag();
                            clearHFlag();
                            clearCFlag();
                            gbRegs.af.b.h = r;
                            break;
                        }
                    case 0x30:		// SWAP B			8
                    case 0x31:		// SWAP C			8
                    case 0x32:		// SWAP D			8
                    case 0x33:		// SWAP E			8
                    case 0x34:		// SWAP H			8
                    case 0x35:		// SWAP L			8
                        {
                            u8 *reg = numberedGbReg(opcode&7);
                            u8 r = *reg;
                            int val = r >> 4;
                            r <<= 4;
                            r |= val;
                            if (r == 0)
                                setZFlag();
                            else
                                clearZFlag();
                            clearNFlag();
                            clearHFlag();
                            clearCFlag();
                            *reg = r;
                            break;
                        }
                    case 0x36:		// SWAP (hl)		16
                        {
                            int val = readMemory(gbRegs.hl.w);
                            int val2 = val >> 4;
                            val <<= 4;
                            val |= val2;
                            writeMemory(gbRegs.hl.w, val);
                            if (val == 0)
                                setZFlag();
                            else
                                clearZFlag();
                            clearNFlag();
                            clearHFlag();
                            clearCFlag();
                            break;
                        }

                    case 0x07:		// RLC A					8
                        {
                            u8 r = gbRegs.af.b.h;
                            r <<= 1;
                            if (((gbRegs.af.b.h) & 0x80) != 0)
                            {
                                setCFlag();
                                r |= 1;
                            }
                            else
                                clearCFlag();
                            if (r == 0)
                                setZFlag();
                            else
                                clearZFlag();
                            clearNFlag();
                            clearHFlag();
                            gbRegs.af.b.h = r;
                            break;
                        }
                    case 0x00:		// RLC B					8
                    case 0x01:		// RLC C					8
                    case 0x02:		// RLC D					8
                    case 0x03:		// RLC E					8
                    case 0x04:		// RLC H					8
                    case 0x05:		// RLC L					8
                        {
                            u8 *reg = numberedGbReg(opcode);
                            u8 r = *reg;
                            r <<= 1;
                            if (((*reg) & 0x80) != 0)
                            {
                                setCFlag();
                                r |= 1;
                            }
                            else
                                clearCFlag();
                            if (r == 0)
                                setZFlag();
                            else
                                clearZFlag();
                            clearNFlag();
                            clearHFlag();
                            *reg = r;
                            break;
                        }

                    case 0x06:		// RLC (hl)				16
                        {
                            int val = readMemory(gbRegs.hl.w);
                            int val2 = val;
                            val2 <<= 1;
                            if ((val & 0x80) != 0)
                            {
                                setCFlag();
                                val2 |= 1;
                            }
                            else
                                clearCFlag();
                            if (val2 == 0)
                                setZFlag();
                            else
                                clearZFlag();
                            clearNFlag();
                            clearHFlag();
                            writeMemory(gbRegs.hl.w, val2);
                            break;

                        }
                    case 0x17:		// RL A				8
                        {
                            u8 r = gbRegs.af.b.h;
                            int val = (r & 0x80);
                            r <<= 1;
                            r |= carrySet();
                            if (val)
                                setCFlag();
                            else
                                clearCFlag();
                            if (r == 0)
                                setZFlag();
                            else
                                clearZFlag();
                            clearNFlag();
                            clearHFlag();
                            gbRegs.af.b.h = r;
                            break;
                        }
                    case 0x10:		// RL B				8
                    case 0x11:		// RL C				8
                    case 0x12:		// RL D				8
                    case 0x13:		// RL E				8
                    case 0x14:		// RL H				8
                    case 0x15:		// RL L				8
                        {
                            u8 *reg = numberedGbReg(opcode&7);
                            u8 r = *reg;
                            int val = (r & 0x80);
                            r <<= 1;
                            r |= carrySet();
                            if (val)
                                setCFlag();
                            else
                                clearCFlag();
                            if (r == 0)
                                setZFlag();
                            else
                                clearZFlag();
                            clearNFlag();
                            clearHFlag();
                            *reg = r;
                            break;
                        }
                    case 0x16:		// RL (hl)			16
                        {
                            u8 val2 = readMemory(gbRegs.hl.w);
                            int val = (val2 & 0x80);
                            val2 <<= 1;
                            val2 |= carrySet();
                            if (val)
                                setCFlag();
                            else
                                clearCFlag();
                            if (val2 == 0)
                                setZFlag();
                            else
                                clearZFlag();
                            clearNFlag();
                            clearHFlag();
                            writeMemory(gbRegs.hl.w, val2);
                            break;
                        }
                    case 0x0F:		// RRC A					8
                        {
                            u8 r = gbRegs.af.b.h;
                            int val = r;
                            r >>= 1;
                            if (val&1)
                            {
                                setCFlag();
                                r |= 0x80;
                            }
                            else
                                clearCFlag();
                            if (r == 0)
                                setZFlag();
                            else
                                clearZFlag();
                            clearNFlag();
                            clearHFlag();
                            gbRegs.af.b.h = r;
                            break;
                        }
                    case 0x08:		// RRC B					8
                    case 0x09:		// RRC C					8
                    case 0x0A:		// RRC D					8
                    case 0x0B:		// RRC E					8
                    case 0x0C:		// RRC H					8
                    case 0x0D:		// RRC L					8
                        {
                            u8 *reg = numberedGbReg(opcode&7);
                            u8 r = *reg;
                            int val = r;
                            r >>= 1;
                            if (val&1)
                            {
                                setCFlag();
                                r |= 0x80;
                            }
                            else
                                clearCFlag();
                            if (r == 0)
                                setZFlag();
                            else
                                clearZFlag();
                            clearNFlag();
                            clearHFlag();
                            *reg = r;
                            break;
                        }
                    case 0x0E:		// RRC (hl)				16
                        {
                            u8 val2 = readMemory(gbRegs.hl.w);
                            int val = val2;
                            val2 >>= 1;
                            if ((val & 1) != 0)
                            {
                                setCFlag();
                                val2 |= 0x80;
                            }
                            else
                                clearCFlag();
                            if (val2 == 0)
                                setZFlag();
                            else
                                clearZFlag();
                            clearNFlag();
                            clearHFlag();
                            writeMemory(gbRegs.hl.w, val2);
                            break;
                        }

                    case 0x1F:		// RR A					8
                        {
                            u8 r = gbRegs.af.b.h;
                            int val = r & 1;
                            r >>= 1;
                            r |= carrySet() << 7;
                            if (val)
                                setCFlag();
                            else
                                clearCFlag();
                            if (r == 0)
                                setZFlag();
                            else
                                clearZFlag();
                            clearNFlag();
                            clearHFlag();
                            gbRegs.af.b.h = r;
                            break;
                        }
                    case 0x18:		// RR B					8
                    case 0x19:		// RR C					8
                    case 0x1A:		// RR D					8
                    case 0x1B:		// RR E					8
                    case 0x1C:		// RR H					8
                    case 0x1D:		// RR L					8
                        {
                            u8 *reg = numberedGbReg(opcode&7);
                            u8 r = *reg;
                            int val = r & 1;
                            r >>= 1;
                            r |= carrySet() << 7;
                            if (val)
                                setCFlag();
                            else
                                clearCFlag();
                            if (r == 0)
                                setZFlag();
                            else
                                clearZFlag();
                            clearNFlag();
                            clearHFlag();
                            *reg = r;
                            break;
                        }
                    case 0x1E:		// RR (hl)			16
                        {
                            u8 val2 = readMemory(gbRegs.hl.w);
                            int val = val2 & 1;
                            val2 >>= 1;
                            val2 |= carrySet() << 7;
                            if (val)
                                setCFlag();
                            else
                                clearCFlag();
                            if (val2 == 0)
                                setZFlag();
                            else
                                clearZFlag();
                            clearNFlag();
                            clearHFlag();
                            writeMemory(gbRegs.hl.w, val2);
                            break;
                        }


                    case 0x27:		// SLA A				8
                        {
                            u8 r = gbRegs.af.b.h;
                            int val = (r & 0x80);
                            r <<= 1;
                            if (val)
                                setCFlag();
                            else
                                clearCFlag();
                            if (r == 0)
                                setZFlag();
                            else
                                clearZFlag();
                            clearNFlag();
                            clearHFlag();
                            gbRegs.af.b.h = r;
                            break;
                        }
                    case 0x20:		// SLA B				8
                    case 0x21:		// SLA C				8
                    case 0x22:		// SLA D				8
                    case 0x23:		// SLA E				8
                    case 0x24:		// SLA H				8
                    case 0x25:		// SLA L				8
                        {
                            u8 *reg = numberedGbReg(opcode&7);
                            u8 r = *reg;
                            int val = (r & 0x80);
                            r <<= 1;
                            if (val)
                                setCFlag();
                            else
                                clearCFlag();
                            if (r == 0)
                                setZFlag();
                            else
                                clearZFlag();
                            clearNFlag();
                            clearHFlag();
                            *reg = r;
                            break;
                        }
                    case 0x26:		// SLA (hl)			16
                        {
                            u8 val2 = readMemory(gbRegs.hl.w);
                            int val = (val2 & 0x80);
                            val2 <<= 1;
                            if (val)
                                setCFlag();
                            else
                                clearCFlag();
                            if (val2 == 0)
                                setZFlag();
                            else
                                clearZFlag();
                            clearNFlag();
                            clearHFlag();
                            writeMemory(gbRegs.hl.w, val2);
                            break;
                        }

                    case 0x2F:		// SRA A				8
                        {
                            u8 r = gbRegs.af.b.h;
                            if (r & 1)
                                setCFlag();
                            else
                                clearCFlag();
                            r >>= 1;
                            if (r & 0x40)
                                r |= 0x80;
                            if (r == 0)
                                setZFlag();
                            else
                                clearZFlag();
                            clearNFlag();
                            clearHFlag();
                            gbRegs.af.b.h = r;
                            break;
                        }
                    case 0x28:		// SRA B				8
                    case 0x29:		// SRA C				8
                    case 0x2A:		// SRA D				8
                    case 0x2B:		// SRA E				8
                    case 0x2C:		// SRA H				8
                    case 0x2D:		// SRA L				8
                        {
                            u8 *reg = numberedGbReg(opcode&7);
                            u8 r = *reg;
                            if (r & 1)
                                setCFlag();
                            else
                                clearCFlag();
                            r >>= 1;
                            if (r & 0x40)
                                r |= 0x80;
                            if (r == 0)
                                setZFlag();
                            else
                                clearZFlag();
                            clearNFlag();
                            clearHFlag();
                            *reg = r;
                            break;
                        }
                    case 0x2E:		// SRA (hl)			16
                        {
                            int val = readMemory(gbRegs.hl.w);
                            if (val & 1)
                                setCFlag();
                            else
                                clearCFlag();
                            val >>= 1;
                            if (val & 0x40)
                                val |= 0x80;
                            if (val == 0)
                                setZFlag();
                            else
                                clearZFlag();
                            clearNFlag();
                            clearHFlag();
                            writeMemory(gbRegs.hl.w, val);
                            break;
                        }

                    case 0x3F:		// SRL A				8
                        {
                            u8 r = gbRegs.af.b.h;
                            if (r & 1)
                                setCFlag();
                            else
                                clearCFlag();
                            r >>= 1;
                            if (r == 0)
                                setZFlag();
                            else
                                clearZFlag();
                            clearNFlag();
                            clearHFlag();
                            gbRegs.af.b.h = r;
                            break;
                        }
                    case 0x38:		// SRL B				8
                    case 0x39:		// SRL C				8
                    case 0x3A:		// SRL D				8
                    case 0x3B:		// SRL E				8
                    case 0x3C:		// SRL H				8
                    case 0x3D:		// SRL L				8
                        {
                            u8 *reg = numberedGbReg(opcode&7);
                            u8 r = *reg;
                            if (r & 1)
                                setCFlag();
                            else
                                clearCFlag();
                            r >>= 1;
                            if (r == 0)
                                setZFlag();
                            else
                                clearZFlag();
                            clearNFlag();
                            clearHFlag();
                            *reg = r;
                            break;
                        }
                    case 0x3E:		// SRL (hl)			16
                        {
                            int val = readMemory(gbRegs.hl.w);
                            if (val & 1)
                                setCFlag();
                            else
                                clearCFlag();
                            val >>= 1;
                            if (val == 0)
                                setZFlag();
                            else
                                clearZFlag();
                            clearNFlag();
                            clearHFlag();
                            writeMemory(gbRegs.hl.w, val);
                            break;
                        }

                    case 0x47:		// BIT 0, A
                    case 0x4F:		// BIT 1, A
                    case 0x57:		// BIT 2, A
                    case 0x5F:		// BIT 3, A
                    case 0x67:		// BIT 4, A
                    case 0x6F:		// BIT 5, A
                    case 0x77:		// BIT 6, A
                    case 0x7F:		// BIT 7, A
                        {
                            if (((gbRegs.af.b.h) & (1<<((opcode>>3)&7))) == 0)
                                setZFlag();
                            else
                                clearZFlag();
                            clearNFlag();
                            setHFlag();
                            break;
                        }
                    case 0x40:		// BIT 0, B     8
                    case 0x41:		// BIT 0, C     8
                    case 0x42:		// BIT 0, D     8
                    case 0x43:		// BIT 0, E     8
                    case 0x44:		// BIT 0, H     8
                    case 0x45:		// BIT 0, L     8
                        if (((*numberedGbReg(opcode&7)) & 1) == 0)
                            setZFlag();
                        else
                            clearZFlag();
                        clearNFlag();
                        setHFlag();
                        break;
                    case 0x48:		// BIT 1, B
                    case 0x49:		// BIT 1, C
                    case 0x4A:		// BIT 1, D
                    case 0x4B:		// BIT 1, E
                    case 0x4C:		// BIT 1, H
                    case 0x4D:		// BIT 1, L
                        if (((*numberedGbReg(opcode&7)) & 2) == 0)
                            setZFlag();
                        else
                            clearZFlag();
                        clearNFlag();
                        setHFlag();
                        break;
                    case 0x50:		// BIT 2, B
                    case 0x51:		// BIT 2, C
                    case 0x52:		// BIT 2, D
                    case 0x53:		// BIT 2, E
                    case 0x54:		// BIT 2, H
                    case 0x55:		// BIT 2, L
                        if (((*numberedGbReg(opcode&7)) & 4) == 0)
                            setZFlag();
                        else
                            clearZFlag();
                        clearNFlag();
                        setHFlag();
                        break;
                    case 0x58:		// BIT 3, B
                    case 0x59:		// BIT 3, C
                    case 0x5A:		// BIT 3, D
                    case 0x5B:		// BIT 3, E
                    case 0x5C:		// BIT 3, H
                    case 0x5D:		// BIT 3, L
                        if (((*numberedGbReg(opcode&7)) & 8) == 0)
                            setZFlag();
                        else
                            clearZFlag();
                        clearNFlag();
                        setHFlag();
                        break;
                    case 0x60:		// BIT 4, B
                    case 0x61:		// BIT 4, C
                    case 0x62:		// BIT 4, D
                    case 0x63:		// BIT 4, E
                    case 0x64:		// BIT 4, H
                    case 0x65:		// BIT 4, L
                        if (((*numberedGbReg(opcode&7)) & 0x10) == 0)
                            setZFlag();
                        else
                            clearZFlag();
                        clearNFlag();
                        setHFlag();
                        break;
                    case 0x68:		// BIT 5, B
                    case 0x69:		// BIT 5, C
                    case 0x6A:		// BIT 5, D
                    case 0x6B:		// BIT 5, E
                    case 0x6C:		// BIT 5, H
                    case 0x6D:		// BIT 5, L
                        if (((*numberedGbReg(opcode&7)) & 0x20) == 0)
                            setZFlag();
                        else
                            clearZFlag();
                        clearNFlag();
                        setHFlag();
                        break;
                    case 0x70:		// BIT 6, B
                    case 0x71:		// BIT 6, C
                    case 0x72:		// BIT 6, D
                    case 0x73:		// BIT 6, E
                    case 0x74:		// BIT 6, H
                    case 0x75:		// BIT 6, L
                        if (((*numberedGbReg(opcode&7)) & 0x40) == 0)
                            setZFlag();
                        else
                            clearZFlag();
                        clearNFlag();
                        setHFlag();
                        break;
                    case 0x78:		// BIT 7, B
                    case 0x79:		// BIT 7, C
                    case 0x7A:		// BIT 7, D
                    case 0x7B:		// BIT 7, E
                    case 0x7C:		// BIT 7, H
                    case 0x7D:		// BIT 7, L
                        if (((*numberedGbReg(opcode&7)) & 0x80) == 0)
                            setZFlag();
                        else
                            clearZFlag();
                        clearNFlag();
                        setHFlag();
                        break;
                    case 0x46:		// BIT 0, (hl)      12
                        if ((readMemory(gbRegs.hl.w) & 0x1) == 0)
                            setZFlag();
                        else
                            clearZFlag();
                        clearNFlag();
                        setHFlag();
                        break;
                    case 0x4E:		// BIT 1, (hl)
                        if ((readMemory(gbRegs.hl.w) & 0x2) == 0)
                            setZFlag();
                        else
                            clearZFlag();
                        clearNFlag();
                        setHFlag();
                        break;
                    case 0x56:		// BIT 2, (hl)
                        if ((readMemory(gbRegs.hl.w) & 0x4) == 0)
                            setZFlag();
                        else
                            clearZFlag();
                        clearNFlag();
                        setHFlag();
                        break;
                    case 0x5E:		// BIT 3, (hl)
                        if ((readMemory(gbRegs.hl.w) & 0x8) == 0)
                            setZFlag();
                        else
                            clearZFlag();
                        clearNFlag();
                        setHFlag();
                        break;
                    case 0x66:		// BIT 4, (hl)
                        if ((readMemory(gbRegs.hl.w) & 0x10) == 0)
                            setZFlag();
                        else
                            clearZFlag();
                        clearNFlag();
                        setHFlag();
                        break;
                    case 0x6E:		// BIT 5, (hl)
                        if ((readMemory(gbRegs.hl.w) & 0x20) == 0)
                            setZFlag();
                        else
                            clearZFlag();
                        clearNFlag();
                        setHFlag();
                        break;
                    case 0x76:		// BIT 6, (hl)
                        if ((readMemory(gbRegs.hl.w) & 0x40) == 0)
                            setZFlag();
                        else
                            clearZFlag();
                        clearNFlag();
                        setHFlag();
                        break;
                    case 0x7E:		// BIT 7, (hl)
                        if ((readMemory(gbRegs.hl.w) & 0x80) == 0)
                            setZFlag();
                        else
                            clearZFlag();
                        clearNFlag();
                        setHFlag();
                        break;
                    case 0xC0:		// SET 0, B
                        gbRegs.bc.b.h |= 1;
                        break;
                    case 0xC1:		// SET 0, C
                        gbRegs.bc.b.l |= 1;
                        break;
                    case 0xC2:		// SET 0, D
                        gbRegs.de.b.h |= 1;
                        break;
                    case 0xC3:		// SET 0, E
                        gbRegs.de.b.l |= 1;
                        break;
                    case 0xC4:		// SET 0, H
                        gbRegs.hl.b.h |= 1;
                        break;
                    case 0xC5:		// SET 0, L
                        gbRegs.hl.b.l |= 1;
                        break;
                    case 0xC6:		// SET 0, (hl)  16
                        {
                            int val = readMemory(gbRegs.hl.w);
                            val |= 1;
                            writeMemory(gbRegs.hl.w, val);
                            break;
                        }
                    case 0xC7:		// SET 0, A
                        gbRegs.af.b.h |= 1;
                        break;
                    case 0xC8:		// SET 1, B
                        gbRegs.bc.b.h |= 2;
                        break;
                    case 0xC9:		// SET 1, C
                        gbRegs.bc.b.l |= 2;
                        break;
                    case 0xCA:		// SET 1, D
                        gbRegs.de.b.h |= 2;
                        break;
                    case 0xCB:		// SET 1, E
                        gbRegs.de.b.l |= 2;
                        break;
                    case 0xCC:		// SET 1, H
                        gbRegs.hl.b.h |= 2;
                        break;
                    case 0xCD:		// SET 1, L
                        gbRegs.hl.b.l |= 2;
                        break;
                    case 0xCE:		// SET 1, (hl)
                        {
                            int val = readMemory(gbRegs.hl.w);
                            val |= 2;
                            writeMemory(gbRegs.hl.w, val);
                            break;
                        }
                    case 0xCF:		// SET 1, A
                        gbRegs.af.b.h |= 2;
                        break;
                    case 0xD0:		// SET 2, B
                        gbRegs.bc.b.h |= 4;
                        break;
                    case 0xD1:		// SET 2, C
                        gbRegs.bc.b.l |= 4;
                        break;
                    case 0xD2:		// SET 2, D
                        gbRegs.de.b.h |= 4;
                        break;
                    case 0xD3:		// SET 2, E
                        gbRegs.de.b.l |= 4;
                        break;
                    case 0xD4:		// SET 2, H
                        gbRegs.hl.b.h |= 4;
                        break;
                    case 0xD5:		// SET 2, L
                        gbRegs.hl.b.l |= 4;
                        break;
                    case 0xD6:		// SET 2, (hl)
                        {
                            int val = readMemory(gbRegs.hl.w);
                            val |= 4;
                            writeMemory(gbRegs.hl.w, val);
                            break;
                        }
                    case 0xD7:		// SET 2, A
                        gbRegs.af.b.h |= 4;
                        break;
                    case 0xD8:		// SET 3, B
                        gbRegs.bc.b.h |= 8;
                        break;
                    case 0xD9:		// SET 3, C
                        gbRegs.bc.b.l |= 8;
                        break;
                    case 0xDA:		// SET 3, D
                        gbRegs.de.b.h |= 8;
                        break;
                    case 0xDB:		// SET 3, E
                        gbRegs.de.b.l |= 8;
                        break;
                    case 0xDC:		// SET 3, H
                        gbRegs.hl.b.h |= 8;
                        break;
                    case 0xDD:		// SET 3, L
                        gbRegs.hl.b.l |= 8;
                        break;
                    case 0xDE:		// SET 3, (hl)
                        {
                            int val = readMemory(gbRegs.hl.w);
                            val |= 8;
                            writeMemory(gbRegs.hl.w, val);
                            break;
                        }
                    case 0xDF:		// SET 3, A
                        gbRegs.af.b.h |= 8;
                        break;
                    case 0xE0:		// SET 4, B
                        gbRegs.bc.b.h |= 0x10;
                        break;
                    case 0xE1:		// SET 4, C
                        gbRegs.bc.b.l |= 0x10;
                        break;
                    case 0xE2:		// SET 4, D
                        gbRegs.de.b.h |= 0x10;
                        break;
                    case 0xE3:		// SET 4, E
                        gbRegs.de.b.l |= 0x10;
                        break;
                    case 0xE4:		// SET 4, H
                        gbRegs.hl.b.h |= 0x10;
                        break;
                    case 0xE5:		// SET 4, L
                        gbRegs.hl.b.l |= 0x10;
                        break;
                    case 0xE6:		// SET 4, (hl)
                        {
                            int val = readMemory(gbRegs.hl.w);
                            val |= 0x10;
                            writeMemory(gbRegs.hl.w, val);
                            break;
                        }
                    case 0xE7:		// SET 4, A
                        gbRegs.af.b.h |= 0x10;
                        break;
                    case 0xE8:		// SET 5, B
                        gbRegs.bc.b.h |= 0x20;
                        break;
                    case 0xE9:		// SET 5, C
                        gbRegs.bc.b.l |= 0x20;
                        break;
                    case 0xEA:		// SET 5, D
                        gbRegs.de.b.h |= 0x20;
                        break;
                    case 0xEB:		// SET 5, E
                        gbRegs.de.b.l |= 0x20;
                        break;
                    case 0xEC:		// SET 5, H
                        gbRegs.hl.b.h |= 0x20;
                        break;
                    case 0xED:		// SET 5, L
                        gbRegs.hl.b.l |= 0x20;
                        break;
                    case 0xEE:		// SET 5, (hl)
                        {
                            int val = readMemory(gbRegs.hl.w);
                            val |= 0x20;
                            writeMemory(gbRegs.hl.w, val);
                            break;
                        }
                    case 0xEF:		// SET 5, A
                        gbRegs.af.b.h |= 0x20;
                        break;
                    case 0xF0:		// SET 6, B
                        gbRegs.bc.b.h |= 0x40;
                        break;
                    case 0xF1:		// SET 6, C
                        gbRegs.bc.b.l |= 0x40;
                        break;
                    case 0xF2:		// SET 6, D
                        gbRegs.de.b.h |= 0x40;
                        break;
                    case 0xF3:		// SET 6, E
                        gbRegs.de.b.l |= 0x40;
                        break;
                    case 0xF4:		// SET 6, H
                        gbRegs.hl.b.h |= 0x40;
                        break;
                    case 0xF5:		// SET 6, L
                        gbRegs.hl.b.l |= 0x40;
                        break;
                    case 0xF6:		// SET 6, (hl)
                        {
                            int val = readMemory(gbRegs.hl.w);
                            val |= 0x40;
                            writeMemory(gbRegs.hl.w, val);
                            break;
                        }
                    case 0xF7:		// SET 6, A
                        gbRegs.af.b.h |= 0x40;
                        break;
                    case 0xF8:		// SET 7, B
                        gbRegs.bc.b.h |= 0x80;
                        break;
                    case 0xF9:		// SET 7, C
                        gbRegs.bc.b.l |= 0x80;
                        break;
                    case 0xFA:		// SET 7, D
                        gbRegs.de.b.h |= 0x80;
                        break;
                    case 0xFB:		// SET 7, E
                        gbRegs.de.b.l |= 0x80;
                        break;
                    case 0xFC:		// SET 7, H
                        gbRegs.hl.b.h |= 0x80;
                        break;
                    case 0xFD:		// SET 7, L
                        gbRegs.hl.b.l |= 0x80;
                        break;
                    case 0xFE:		// SET 7, (hl)
                        {
                            int val = readMemory(gbRegs.hl.w);
                            val |= 0x80;
                            writeMemory(gbRegs.hl.w, val);
                            break;
                        }
                    case 0xFF:		// SET 7, A
                        gbRegs.af.b.h |= 0x80;
                        break;

                    case 0x80:		// RES 0, B
                        gbRegs.bc.b.h &= 0xFE;
                        break;
                    case 0x81:		// RES 0, C
                        gbRegs.bc.b.l &= 0xFE;
                        break;
                    case 0x82:		// RES 0, D
                        gbRegs.de.b.h &= 0xFE;
                        break;
                    case 0x83:		// RES 0, E
                        gbRegs.de.b.l &= 0xFE;
                        break;
                    case 0x84:		// RES 0, H
                        gbRegs.hl.b.h &= 0xFE;
                        break;
                    case 0x85:		// RES 0, L
                        gbRegs.hl.b.l &= 0xFE;
                        break;
                    case 0x86:		// RES 0, (hl)
                        {
                            int val = readMemory(gbRegs.hl.w);
                            val &= 0xFE;
                            writeMemory(gbRegs.hl.w, val);
                            break;
                        }
                    case 0x87:		// RES 0, A
                        gbRegs.af.b.h &= 0xFE;
                        break;
                    case 0x88:		// RES 1, B
                        gbRegs.bc.b.h &= 0xFD;
                        break;
                    case 0x89:		// RES 1, C
                        gbRegs.bc.b.l &= 0xFD;
                        break;
                    case 0x8A:		// RES 1, D
                        gbRegs.de.b.h &= 0xFD;
                        break;
                    case 0x8B:		// RES 1, E
                        gbRegs.de.b.l &= 0xFD;
                        break;
                    case 0x8C:		// RES 1, H
                        gbRegs.hl.b.h &= 0xFD;
                        break;
                    case 0x8D:		// RES 1, L
                        gbRegs.hl.b.l &= 0xFD;
                        break;
                    case 0x8E:		// RES 1, (hl)
                        {
                            int val = readMemory(gbRegs.hl.w);
                            val &= 0xFD;
                            writeMemory(gbRegs.hl.w, val);
                            break;
                        }
                    case 0x8F:		// RES 1, A
                        gbRegs.af.b.h &= 0xFD;
                        break;
                    case 0x90:		// RES 2, B
                        gbRegs.bc.b.h &= 0xFB;
                        break;
                    case 0x91:		// RES 2, C
                        gbRegs.bc.b.l &= 0xFB;
                        break;
                    case 0x92:		// RES 2, D
                        gbRegs.de.b.h &= 0xFB;
                        break;
                    case 0x93:		// RES 2, E
                        gbRegs.de.b.l &= 0xFB;
                        break;
                    case 0x94:		// RES 2, H
                        gbRegs.hl.b.h &= 0xFB;
                        break;
                    case 0x95:		// RES 2, L
                        gbRegs.hl.b.l &= 0xFB;
                        break;
                    case 0x96:		// RES 2, (hl)
                        {
                            int val = readMemory(gbRegs.hl.w);
                            val &= 0xFB;
                            writeMemory(gbRegs.hl.w, val);
                            break;
                        }
                    case 0x97:		// RES 2, A
                        gbRegs.af.b.h &= 0xFB;
                        break;
                    case 0x98:		// RES 3, B
                        gbRegs.bc.b.h &= 0xF7;
                        break;
                    case 0x99:		// RES 3, C
                        gbRegs.bc.b.l &= 0xF7;
                        break;
                    case 0x9A:		// RES 3, D
                        gbRegs.de.b.h &= 0xF7;
                        break;
                    case 0x9B:		// RES 3, E
                        gbRegs.de.b.l &= 0xF7;
                        break;
                    case 0x9C:		// RES 3, H
                        gbRegs.hl.b.h &= 0xF7;
                        break;
                    case 0x9D:		// RES 3, L
                        gbRegs.hl.b.l &= 0xF7;
                        break;
                    case 0x9E:		// RES 3, (hl)
                        {
                            int val = readMemory(gbRegs.hl.w);
                            val &= 0xF7;
                            writeMemory(gbRegs.hl.w, val);
                            break;
                        }
                    case 0x9F:		// RES 3, A
                        gbRegs.af.b.h &= 0xF7;
                        break;
                    case 0xA0:		// RES 4, B
                        gbRegs.bc.b.h &= 0xEF;
                        break;
                    case 0xA1:		// RES 4, C
                        gbRegs.bc.b.l &= 0xEF;
                        break;
                    case 0xA2:		// RES 4, D
                        gbRegs.de.b.h &= 0xEF;
                        break;
                    case 0xA3:		// RES 4, E
                        gbRegs.de.b.l &= 0xEF;
                        break;
                    case 0xA4:		// RES 4, H
                        gbRegs.hl.b.h &= 0xEF;
                        break;
                    case 0xA5:		// RES 4, L
                        gbRegs.hl.b.l &= 0xEF;
                        break;
                    case 0xA6:		// RES 4, (hl)
                        {
                            int val = readMemory(gbRegs.hl.w);
                            val &= 0xEF;
                            writeMemory(gbRegs.hl.w, val);
                            break;
                        }
                    case 0xA7:		// RES 4, A
                        gbRegs.af.b.h &= 0xEF;
                        break;
                    case 0xA8:		// RES 5, B
                        gbRegs.bc.b.h &= 0xDF;
                        break;
                    case 0xA9:		// RES 5, C
                        gbRegs.bc.b.l &= 0xDF;
                        break;
                    case 0xAA:		// RES 5, D
                        gbRegs.de.b.h &= 0xDF;
                        break;
                    case 0xAB:		// RES 5, E
                        gbRegs.de.b.l &= 0xDF;
                        break;
                    case 0xAC:		// RES 5, H
                        gbRegs.hl.b.h &= 0xDF;
                        break;
                    case 0xAD:		// RES 5, L
                        gbRegs.hl.b.l &= 0xDF;
                        break;
                    case 0xAE:		// RES 5, (hl)
                        {
                            int val = readMemory(gbRegs.hl.w);
                            val &= 0xDF;
                            writeMemory(gbRegs.hl.w, val);
                            break;
                        }
                    case 0xAF:		// RES 5, A
                        gbRegs.af.b.h &= 0xDF;
                        break;
                    case 0xB0:		// RES 6, B
                        gbRegs.bc.b.h &= 0xBF;
                        break;
                    case 0xB1:		// RES 6, C
                        gbRegs.bc.b.l &= 0xBF;
                        break;
                    case 0xB2:		// RES 6, D
                        gbRegs.de.b.h &= 0xBF;
                        break;
                    case 0xB3:		// RES 6, E
                        gbRegs.de.b.l &= 0xBF;
                        break;
                    case 0xB4:		// RES 6, H
                        gbRegs.hl.b.h &= 0xBF;
                        break;
                    case 0xB5:		// RES 6, L
                        gbRegs.hl.b.l &= 0xBF;
                        break;
                    case 0xB6:		// RES 6, (hl)
                        {
                            int val = readMemory(gbRegs.hl.w);
                            val &= 0xBF;
                            writeMemory(gbRegs.hl.w, val);
                            break;
                        }
                    case 0xB7:		// RES 6, A
                        gbRegs.af.b.h &= 0xBF;
                        break;
                    case 0xB8:		// RES 7, B
                        gbRegs.bc.b.h &= 0x7F;
                        break;
                    case 0xB9:		// RES 7, C
                        gbRegs.bc.b.l &= 0x7F;
                        break;
                    case 0xBA:		// RES 7, D
                        gbRegs.de.b.h &= 0x7F;
                        break;
                    case 0xBB:		// RES 7, E
                        gbRegs.de.b.l &= 0x7F;
                        break;
                    case 0xBC:		// RES 7, H
                        gbRegs.hl.b.h &= 0x7F;
                        break;
                    case 0xBD:		// RES 7, L
                        gbRegs.hl.b.l &= 0x7F;
                        break;
                    case 0xBE:		// RES 7, (hl)
                        {
                            int val = readMemory(gbRegs.hl.w);
                            val &= 0x7F;
                            writeMemory(gbRegs.hl.w, val);
                            break;
                        }
                    case 0xBF:		// RES 7, A
                        gbRegs.af.b.h &= 0x7F;
                        break;
                    default:
                        break;
                }
                break;
            default:
                break;
        }
    }

end:
    gbRegs.af.b.l = locF;
    gbRegs.pc.w += (pcAddr-firstPcAddr);
    gbRegs.sp.w = locSP;
    return totalCycles;
}

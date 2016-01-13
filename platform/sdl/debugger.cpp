#include <SDL.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include "gameboy.h"
#include "inputhelper.h"

#ifdef CPU_DEBUG

using namespace std;

// public 
int debugMode=0;

int breakpointAddr=-1;
int readWatchAddr=-1;
int readWatchBank=-1;
int writeWatchAddr=-1;
int writeWatchBank=-1;

// private
FILE* logFile;

const char* opcodeList[] = {
"nop", "ld bc,####", "ld (bc),a", "inc bc", "inc b", "dec b", "ld b,##", "rlca", "ld (####),sp", "add hl,bc", "ld a,(bc)", "dec bc", "inc c", "dec c", "ld c,##", "rrca",
"stop ##", "ld de,####", "ld (de),a", "inc de", "inc d", "dec d", "ld d,##", "rla", "jr **", "add hl,de", "ld a,(de)", "dec de", "inc e", "dec e", "ld e,##", "rra",
"jr nz,**", "ld hl,####", "ldi (hl),a", "inc hl", "inc h", "dec h", "ld h,##", "daa", "jr z,**", "add hl,hl", "ldi a,(hl)", "dec hl", "inc l", "dec l", "ld l,##", "cpl",
"jr nc,**", "ld sp,####", "ldd (hl),a", "inc sp", "inc (hl)", "dec (hl)", "ld (hl),##", "scf", "jr c,**", "add hl,sp", "ldd a,(hl)", "dec sp", "inc a", "dec a", "ld a,##", "ccf",
"ld b,b", "ld b,c", "ld b,d", "ld b,e", "ld b,h", "ld b,l", "ld b,(hl)", "ld b,a", "ldc,b", "ld c,c", "ld c,d", "ld c,e", "ld c,h", "ld c,l", "ld c,(hl)", "ld c,a",
"ld d,b", "ld d,c", "ld d,d", "ld d,e", "ld d,h", "ld d,l", "ld d,(hl)", "ld d,a", "ld e,b", "ld e,c", "ld e,d", "ld e,e", "ld e,h", "ld e,l", "ld e,(hl)", "ld e,a",
"ld h,b", "ld h,c", "ld h,d", "ld h,e", "ld h,h", "ld h,l", "ld h,(hl)", "ld h,a", "ld l,b", "ld l,c", "ld l,d", "ld l,e", "ld l,h", "ld l,l", "ld l,(hl)", "ld l,a",
"ld (hl),b", "ld (hl),c", "ld (hl),d", "ld (hl),e", "ld (hl),h", "ld (hl),l", "halt", "ld (hl),a", "ld a,b", "ld a,c", "ld a,d", "ld a,e", "ld a,h", "ld a,l", "ld a,(hl)", "ld a,a",
"add a,b", "add a,c", "add a,d", "add a,e", "add a,h", "add a,l", "add a,(hl)", "add a,a", "adc a,b", "adc a,c", "adc a,d", "adc a,e", "adc a,h", "adc a,l", "adc a,(hl)", "adc a,a",
"sub b", "sub c", "sub d", "sub e", "sub h", "sub l", "sub (hl)", "sub a", "sbc a,b", "sbc a,c", "sbc a,d", "sbc a,e", "sbc a,h", "sbc a,l", "sbc a,(hl)", "sbc a,a",
"and b", "and c", "and d", "and e", "and h", "and l", "and (hl)", "and a", "xor b", "xor c", "xor d", "xor e", "xor h", "xor l", "xor (hl)", "xor a",
"or b", "or c", "or d", "or e", "or h", "or l", "or (hl)", "or a", "cp b", "cp c", "cp d", "cp e", "cp h", "cp l", "cp  (hl)", "cp a",
"ret nz", "pop bc", "jp nz,####", "jp ####", "call nz,####", "push bc", "add a,##", "rst 00", "ret z", "ret", "jp z,####", "CB OPCODE", "call z,####", "call ####", "adc a,##", "rst 08",
"ret nc", "pop de", "jp nc,####", "D3", "call nc,####", "push de", "sub ##", "rst 10", "ret c", "reti", "jp c,####", "DB", "call c,####", "DD", "sbc a,##", "rst 18",
"ldh (ff00+##),a", "pop hl", "ld (ff00+c),a", "E3", "E4", "push hl", "and ##", "rst 20", "add sp,##", "jp (hl)", "ld (####),a", "EB", "EC", "ED", "xor ##", "rst 28",
"ldh a,(ff00+##)", "pop af", "ld a,(ff00+c)", "di", "F4", "push af", "or ##", "rst 30", "ld hl,sp+##", "ld sp,hl", "ld a,(####)", "ei", "FC", "FD", "cp ##", "rst 38",
};
const char* CBopcodeList[] = {
    "",
};

string intToHex(int val, int n) {
    char out[n+1];
    out[n] = 0;
    for (int i=0;i<n;i++) {
        int v = val&0xf;
        out[n-i-1] = (v < 10 ? '0'+v : 'a'+v-10);
        val >>= 4;
    }
    return string(out);
}

int printOp(Gameboy* gameboy, int addr, FILE* file=stdout)
{
    int gbPC = addr;
    int opcode = gameboy->readMemory(gbPC);
    gbPC++;

//     if (opcode == 0xCB)
//         str = CBopcodeList[gameboy->readMemory(gbPC++)];
//     else
    string str(opcodeList[opcode]);

    int numStart=-1;
    int numEnd=-2;
    int numChars=0;

    for (int i=0; ; i++)
    {
        if (str[i] == '\0')
            break;
        if (str[i] == '#' || str[i] == '*') {
            if (numStart == -1)
                numStart = i;
            numEnd = i;
        }
    }
    numChars = numEnd-numStart+1;
    if (numChars == 2) {
        string val = intToHex(gameboy->quickRead(gbPC), 2);
        if (str[numStart] == '*') {
            val = intToHex((gbPC + 1 + (s8)gameboy->quickRead(gbPC))&0xffff, 4);
        }
        str.replace(numStart, numChars, val);
    }
    else if (numChars == 4) {
        string val = intToHex(gameboy->readMemory16(gbPC), 4);
        str.replace(numStart, numChars, val);
    }
    numChars /= 2;

    int bank = gameboy->getBank(gbPC-1);
    if (bank == -1)
        bank = 0;

    fprintf(file, "%.2X:%.4X: %s\n", bank, gbPC-1, str.c_str());

    gbPC++;

    return numChars+1;
}

void parseCommand(Gameboy* g, const Registers& regs)
{
    printOp(g, regs.pc.w);
    if (debugMode == 2) {
        debugMode = 1;
    }
    while(true)
    {
        printf("> ");
        string input;
        string word;

        getline(cin, input);
        stringstream stream(input);
        stream >> word;

        if (word.compare("n") == 0) {
            debugMode = 2;
            return;
        }
        else if (word.compare("c") == 0)
        {
            debugMode = 0;
            return;
        }
        else if (word.compare("q") == 0)
        {
#ifdef CPU_LOG
            fclose(logFile);
#endif
            exit(0);
        }
        else if (word.compare("b") == 0) {
            stream >> hex >> breakpointAddr;
            printf("Set breakpoint at %x\n", breakpointAddr);
        }
        else if (word.compare("ww") == 0)
        {
            stream >> writeWatchBank;
            stream >> hex >> writeWatchAddr;
            printf("Watching %d:%x for writes\n", writeWatchBank, writeWatchAddr);
        }
        else if (word.compare("rw") == 0)
        {
            stream >> readWatchBank;
            stream >> hex >> readWatchAddr;
            printf("Watching %d:%x for reads\n", readWatchBank, readWatchAddr);
        }
        else if (word.compare("p") == 0)
        {
            string word;
            stream >> word;
            if (word.compare("banks") == 0)
                printf("ROM: %.2x\nWRAM: %d\nSRAM: %d\n", gameboy->romBank, gameboy->wramBank, gameboy->currentRamBank);
            else
                printf("af: %.4x  bc: %.4x\nde: %.4x  hl: %.4x\n", regs.af.w, regs.bc.w, regs.de.w, regs.hl.w);
        }
        else if (word.compare("l") == 0) {
            int address = regs.pc.w;
            int numLines = 10;
            stream >> hex >> address;
            stream >> numLines;

            int pos = address;
            if (pos < 0)
                pos += regs.pc.w;
            if (pos < 0)
                pos = 0;
            for (int i=0;i<numLines;i++) {
                if (pos == regs.pc.w)
                    printf("* ");
                else
                    printf("  ");
                pos += printOp(gameboy, pos);
            }
        }
        else {
            printf("Unrecognized command.\n");
        }
    }
}

void startDebugger() {
#ifdef CPU_LOG
    logFile = fopen("LOG", "w");
#endif
}

int runDebugger(Gameboy* gameboy, const Registers& regs)
{
    if (keyJustPressed(SDLK_d))
        debugMode = 1;
    else if (breakpointAddr == regs.pc.w)
        debugMode = 1;
    system_checkPolls();

#ifdef CPU_LOG
    printOp(gameboy, regs.pc.w, logFile);
#endif

    if (debugMode)
        parseCommand(gameboy, regs);

    return 0;
}

/*
void saveLog()
{
	fclose(logFile);
	logFile = fopen("LOG", "a");
}
*/

#endif

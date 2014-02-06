// GBS files contain music ripped from a game.
#include <nds.h>
#include "gbs.h"
#include "common.h"
#include "inputhelper.h"
#include "mmu.h"
#include "gameboy.h"
#include "main.h"
#include "gbsnd.h"
#include "console.h"
#include "romfile.h"

#define READ16(src) (*(src) | *(src+1)<<8)

bool gbsMode;
u8 gbsHeader[0x70];

u8 gbsNumSongs;
u16 gbsLoadAddress;
u16 gbsInitAddress;
u16 gbsPlayAddress;

u8 gbsSelectedSong;
int gbsPlayingSong;

PrintConsole* gbsConsole = 0;
extern PrintConsole defaultConsole; // Defined in libnds

// private

void gbsRedraw() {
    //consoleClear();
    
    PrintConsole* oldPrintConsole = getPrintConsole();
    setPrintConsole(gbsConsole);
    iprintf("\33[0;0H"); // Cursor to upper-left corner

    iprintf("Song %d of %d\33[0K\n", gbsSelectedSong+1, gbsNumSongs);
    if (gbsPlayingSong == -1)
        iprintf("(Not playing)\33[0K\n\n");
    else
        iprintf("(Playing %d)\33[0K\n\n", gbsPlayingSong+1);

    // Print music information
    for (int i=0; i<3; i++) {
        for (int j=0; j<32; j++) {
            char c = gbsHeader[0x10+i*0x20+j];
            if (c == 0)
                iprintf(" ");
            else
                iprintf("%c", c);
        }
        iprintf("\n");
    }
    setPrintConsole(oldPrintConsole);
}

void gbsLoadSong() {
    u8* romSlot0 = gameboy->getRomFile()->getRomBank(0);
    gameboy->initMMU();
    gameboy->ime = 0;

    gbRegs.sp.w = READ16(gbsHeader+0x0c);
    u8 tma =        gbsHeader[0x0e];
    u8 tac =        gbsHeader[0x0f];

    if (tac&0x80)
        gameboy->setDoubleSpeed(1);
    tac &= ~0x80;
    if (tma == 0 && tac == 0) {
        // Vblank interrupt handler
        romSlot0[0x40] = 0xcd; // call
        romSlot0[0x41] = gbsPlayAddress&0xff;
        romSlot0[0x42] = gbsPlayAddress>>8;
        romSlot0[0x43] = 0xd9; // reti
        gameboy->writeIO(0xff, VBLANK);
    }
    else {
        // Timer interrupt handler
        romSlot0[0x50] = 0xcd; // call
        romSlot0[0x51] = gbsPlayAddress&0xff;
        romSlot0[0x52] = gbsPlayAddress>>8;
        romSlot0[0x53] = 0xd9; // reti
        gameboy->writeIO(0xff, TIMER);
    }

    gameboy->writeIO(0x05, 0x00);
    gameboy->writeIO(0x06, tma);
    gameboy->writeIO(0x07, tac);

    gbsPlayingSong = gbsSelectedSong;
    gbRegs.af.b.h = gbsPlayingSong;
    gameboy->writeMemory(--gbRegs.sp.w, 0x01);
    gameboy->writeMemory(--gbRegs.sp.w, 0x00); // Will return to beginning
    gbRegs.pc.w = gbsInitAddress;
}

// public

void gbsReadHeader() {
    gbsNumSongs    =    gbsHeader[0x04];
    gbsLoadAddress =    READ16(gbsHeader+0x06);
    gbsInitAddress =    READ16(gbsHeader+0x08);
    gbsPlayAddress =    READ16(gbsHeader+0x0a);
}

void gbsInit() {
    u8* romSlot0 = gameboy->getRomFile()->getRomBank(0);

    if (gbsConsole == 0) {
        gbsConsole = (PrintConsole*)malloc(sizeof(PrintConsole));
        memcpy(gbsConsole, &defaultConsole, sizeof(PrintConsole));
    }
    videoSetMode(MODE_0_2D);
    consoleInit(gbsConsole, gbsConsole->bgLayer, BgType_Text4bpp, BgSize_T_256x256, gbsConsole->mapBase, gbsConsole->gfxBase, true, true);
    setPrintConsole(gbsConsole);
    videoBgEnable(0);

    u8 firstSong=   gbsHeader[0x05]-1;

    // RST vectors
    for (int i=0; i<8; i++) {
        u16 dest = gbsLoadAddress + i*8;
        romSlot0[i*8] = 0xc3; // jp
        romSlot0[i*8+1] = dest&0xff;
        romSlot0[i*8+2] = dest>>8;
    }

    // Interrupt handlers
    for (int i=0; i<5; i++) {
        romSlot0[0x40+i*8] = 0xd9; // reti
    }

    // Infinite loop
    romSlot0[0x100] = 0xfb; // ime
    romSlot0[0x101] = 0x76; // halt
    romSlot0[0x102] = 0x18; // jr -3
    romSlot0[0x103] = -3;

    gbsSelectedSong = firstSong;
    gbsLoadSong();
}

// Called at vblank each frame
void gbsCheckInput() {
    if (keyPressedAutoRepeat(mapGbKey(KEY_GB_LEFT))) {
        if (gbsSelectedSong == 0)
            gbsSelectedSong = gbsNumSongs-1;
        else
            gbsSelectedSong--;
    }
    if (keyPressedAutoRepeat(mapGbKey(KEY_GB_RIGHT))) {
        gbsSelectedSong++;
        if (gbsSelectedSong == gbsNumSongs)
            gbsSelectedSong = 0;
    }
    if (keyJustPressed(mapGbKey(KEY_GB_A))) {
        gbsLoadSong();
    }
    if (keyJustPressed(mapGbKey(KEY_GB_B))) { // Stop playing music
        gbsPlayingSong = -1;
        gameboy->ime = 0;
        gameboy->writeIO(0xff, 0);
        gameboy->initSND();
    }
    gbsRedraw();

    if (gbsPlayingSong != -1)
        disableSleepMode();
    else
        enableSleepMode();
}

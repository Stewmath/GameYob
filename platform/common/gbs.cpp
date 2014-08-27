// GBS files contain music ripped from a game.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gbs.h"
#include "inputhelper.h"
#include "gameboy.h"
#include "main.h"
#include "soundengine.h"
#include "console.h"
#include "menu.h"
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
    printf("\33[0;0H"); // Cursor to upper-left corner

    printf("Song %d of %d\33[0K\n", gbsSelectedSong+1, gbsNumSongs);
    if (gbsPlayingSong == -1)
        printf("(Not playing)\33[0K\n\n");
    else
        printf("(Playing %d)\33[0K\n\n", gbsPlayingSong+1);

    // Print music information
    for (int i=0; i<3; i++) {
        for (int j=0; j<32; j++) {
            char c = gbsHeader[0x10+i*0x20+j];
            if (c == 0)
                printf(" ");
            else
                printf("%c", c);
        }
        printf("\n");
    }
    setPrintConsole(oldPrintConsole);
}

void gbsLoadSong() {
    u8* romSlot0 = gameboy->getRomFile()->getRomBank(0);
    gameboy->initMMU();
    gameboy->ime = 0;

    gameboy->gbRegs.sp.w = READ16(gbsHeader+0x0c);
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
    gameboy->gbRegs.af.b.h = gbsPlayingSong;
    gameboy->writeMemory(--gameboy->gbRegs.sp.w, 0x01);
    gameboy->writeMemory(--gameboy->gbRegs.sp.w, 0x00); // Will return to beginning
    gameboy->gbRegs.pc.w = gbsInitAddress;
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
#ifdef DS
    videoSetMode(MODE_0_2D);
    consoleInit(gbsConsole, gbsConsole->bgLayer, BgType_Text4bpp, BgSize_T_256x256, gbsConsole->mapBase, gbsConsole->gfxBase, true, true);
    setPrintConsole(gbsConsole);
    videoBgEnable(0);
#endif

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
    if (keyPressedAutoRepeat(mapMenuKey(MENU_KEY_LEFT))) {
        if (gbsSelectedSong == 0)
            gbsSelectedSong = gbsNumSongs-1;
        else
            gbsSelectedSong--;
    }
    if (keyPressedAutoRepeat(mapMenuKey(MENU_KEY_RIGHT))) {
        gbsSelectedSong++;
        if (gbsSelectedSong == gbsNumSongs)
            gbsSelectedSong = 0;
    }
    if (keyJustPressed(mapMenuKey(MENU_KEY_A))) {
        gbsLoadSong();
    }
    if (keyJustPressed(mapMenuKey(MENU_KEY_B))) { // Stop playing music
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

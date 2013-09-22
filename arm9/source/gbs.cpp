// GBS files contain music ripped from a game.
#include <nds.h>
#include "gbs.h"
#include "inputhelper.h"
#include "mmu.h"
#include "gbcpu.h"
#include "gameboy.h"
#include "main.h"
#include "gbsnd.h"

#define READ16(src) (*(src) | *(src+1)<<8)

bool gbsMode;
u8 gbsHeader[0x70];

u8 gbsNumSongs;
u16 gbsLoadAddress;
u16 gbsInitAddress;
u16 gbsPlayAddress;

u8 gbsSelectedSong;
u8 gbsPlayingSong;

// private

void gbsRedraw() {
    consoleClear();

    printf("Song %d of %d\n", gbsSelectedSong+1, gbsNumSongs);
    printf("(Playing %d)\n\n", gbsPlayingSong+1);

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
}

void gbsLoadSong() {
    initMMU();
    ime = 0;

    gbRegs.sp.w = READ16(gbsHeader+0x0c);
    u8 tma =        gbsHeader[0x0e];
    u8 tac =        gbsHeader[0x0f];

    if (tac&0x80)
        setDoubleSpeed(1);
    tac &= ~0x80;
    if (tma == 0 && tac == 0) {
        // Vblank interrupt handler
        romSlot0[0x40] = 0xcd; // call
        romSlot0[0x41] = gbsPlayAddress&0xff;
        romSlot0[0x42] = gbsPlayAddress>>8;
        romSlot0[0x43] = 0xd9; // reti
        writeIO(0xff, VBLANK);
    }
    else {
        // Timer interrupt handler
        romSlot0[0x50] = 0xcd; // call
        romSlot0[0x51] = gbsPlayAddress&0xff;
        romSlot0[0x52] = gbsPlayAddress>>8;
        romSlot0[0x53] = 0xd9; // reti
        writeIO(0xff, TIMER);
    }

    writeIO(0x05, 0x00);
    writeIO(0x06, tma);
    writeIO(0x07, tac);

    gbsPlayingSong = gbsSelectedSong;
    gbRegs.af.b.h = gbsPlayingSong;
    writeMemory(--gbRegs.sp.w, 0x01);
    writeMemory(--gbRegs.sp.w, 0x00); // Will return to beginning
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
    u8 firstSong=   gbsHeader[0x05]-1;
    printLog("INIT\n");

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
void gbsUpdateInput() {
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
        ime = 0;
        writeIO(0xff, 0);
        initSND();
    }
    gbsRedraw();
}

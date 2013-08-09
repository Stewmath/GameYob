// GBS files contain music ripped from a game.
#include <nds.h>
#include "gbs.h"
#include "inputhelper.h"
#include "mmu.h"
#include "gbcpu.h"
#include "gameboy.h"
#include "main.h"

#define READ16(src) (*(src) | *(src+1)<<8)

bool gbsMode;
u8 gbsHeader[0x70];

u8 gbsNumSongs;
u16 gbsLoadAddress;
u16 gbsInitAddress;
u16 gbsPlayAddress;

int gbsCurrentSong;

// private

void gbsLoadSong(int song) {
    gbsCurrentSong = song;
    gbRegs.af.b.h = song;
    gbRegs.sp.w = READ16(gbsHeader+0x0c); // Reset SP
    writeMemory(--gbRegs.sp.w, 0x01);
    writeMemory(--gbRegs.sp.w, 0x00); // Will return to beginning
    gbRegs.pc.w = gbsInitAddress;
    ime = 0;
}

// public

void readGBSHeader() {
    gbsNumSongs    =    gbsHeader[0x04];
    gbsLoadAddress =    READ16(gbsHeader+0x06);
    gbsInitAddress =    READ16(gbsHeader+0x08);
    gbsPlayAddress =    READ16(gbsHeader+0x0a);
}

void initGBS() {
    u8 firstSong=   gbsHeader[0x05]-1;
    u8 tma =        gbsHeader[0x0e];
    u8 tac =        gbsHeader[0x0f];

    if (tac&0x80)
        setDoubleSpeed(1);
    tac &= ~0x80;
    if (tma == 0 && tac == 0) {
        tma = 188;
        tac = 84;
    }

    writeIO(0x05, 0x00);
    writeIO(0x06, tma);
    writeIO(0x07, tac);

    // Timer interrupt handler
    romSlot0[0x50] = 0xcd; // call
    romSlot0[0x51] = gbsPlayAddress&0xff;
    romSlot0[0x52] = gbsPlayAddress>>8;
    romSlot0[0x53] = 0xd9; // reti

    // RST vectors
    for (int i=0; i<8; i++) {
        u16 dest = gbsLoadAddress + i*8;
        romSlot0[i*8] = 0xc3; // jp
        romSlot0[i*8+1] = dest&0xff;
        romSlot0[i*8+2] = dest>>8;
    }

    // Infinite loop
    romSlot0[0x100] = 0xfb; // ime
    romSlot0[0x101] = 0x76; // halt
    romSlot0[0x102] = 0x18; // jr -3
    romSlot0[0x103] = -3;

    gbsLoadSong(firstSong);

    writeIO(0xff, TIMER); // Enable timer interrupt
}

void updateGBSInput() {
    updateInput(); // As normal

    consoleClear();

    printf("Song %d of %d\n", gbsCurrentSong+1, gbsNumSongs);

    if (keyPressedAutoRepeat(KEY_LEFT)) {
        if (gbsCurrentSong == 0)
            gbsCurrentSong = gbsNumSongs;
        gbsLoadSong(gbsCurrentSong-1);
    }
    if (keyPressedAutoRepeat(KEY_RIGHT)) {
        if (gbsCurrentSong == gbsNumSongs-1)
            gbsCurrentSong = -1;
        gbsLoadSong(gbsCurrentSong+1);
    }
}

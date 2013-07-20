#pragma once
#include "global.h"

extern int interruptWaitMode;
extern int scaleMode;
extern int scaleFilter;
extern u8 gfxMask;
extern volatile int loadedBorderType;

extern u8 bgPaletteData[0x40];
extern u8 sprPaletteData[0x40];

void drawScanline(int scanline);
void drawScanline_P2(int scanline);
void drawScreen();

void initGFX();
void initGFXPalette();
void refreshGFX();
void refreshSgbPalette();

void checkBorder();
void refreshScaleMode();
void setGFXMask(int mask);
void setSgbTiles(u8* src, u8 flags);
void setSgbMap(u8* src);

void writeVram(u16 addr, u8 val);
void writeVram16(u16 addr, u16 src);
void writeHram(u16 addr, u8 val);
void handleVideoRegister(u8 ioReg, u8 val);

enum {
    BORDER_NONE=0,
    BORDER_SGB,
    BORDER_CUSTOM
};  // Use with loadedBorderType

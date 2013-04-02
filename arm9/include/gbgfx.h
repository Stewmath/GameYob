#pragma once
#include "global.h"

extern int interruptWaitMode;

extern u8 bgPaletteData[0x40];
extern u8 sprPaletteData[0x40];

void drawScanline(int scanline);
void drawScreen();

void setScaleMode(int mode);

void enableScaleFilter(int enabled);
void enableBackground(int enabled);
void enableSprites(int enabled);
void enableWindow(int enabled);

void initGFX();
void refreshGFX();
void disableScreen();
void enableScreen();

void writeVram(u16 addr, u8 val);
void writeVram16(u16 addr, u16 src);
void writeHram(u16 addr, u8 val);
void handleVideoRegister(u8 ioReg, u8 val);

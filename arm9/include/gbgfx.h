#pragma once

enum Icons {
    ICON_NULL=0,
    ICON_PRINTER
};

extern volatile int dsFrameCounter;

extern int interruptWaitMode;
extern int scaleMode;
extern int scaleFilter;
extern u8 gfxMask;
extern volatile int loadedBorderType;
extern bool customBorderExists;

extern u8 bgPaletteData[0x40];
extern u8 sprPaletteData[0x40];

void doAtVBlank(void (*func)(void));

void drawScanline(int scanline);
void drawScanline_P2(int scanline);
void drawScreen();

void initGFX();
void initGFXPalette();
void refreshGFX();
void clearGFX();
void refreshSgbPalette();

void displayIcon(int iconid);

void selectBorder(); // Starts the file chooser for a border
int loadBorder(const char* filename); // Loads the border to vram
void checkBorder(); // Decides what kind of border to use, invokes loadBorder if necessary

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

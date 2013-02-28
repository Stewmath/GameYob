#pragma once
#include "global.h"

extern int scale;

extern bool changedTile[2][0x180];
extern int changedTileQueueLength;
extern u16 changedTileQueue[];

extern bool changedTileInFrame[2][0x180];
extern int changedTileInFrameQueueLength;
extern u16 changedTileInFrameQueue[];

extern u32 gbColors[4];
extern u8 bgPaletteData[0x40];		// The raw palette data, used by the gameboy
extern u8 sprPaletteData[0x40];		// The raw palette data, used by the gameboy

extern bool bgPaletteChanged[];
extern bool sprPaletteChanged[];

extern bool lineModified;
extern int winPosY;

extern int interruptWaitMode;

/*extern int tileAddr;
extern int BGMapAddr;
extern int winMapAddr;
extern int winOn;
extern int scale;*/

// Render scanline to pixels[]
void drawScanline(int scanline);
// Copy pixels[] to the screen
void drawScreen();

void initGFX();
void disableScreen();
void enableScreen();
void drawTile(int tile, int bank);
void drawBgTile(int tile, int bank);
void drawSprTile(int tile, int bank);

void updateTiles();
void updateTileMap(int map, int i, u8 val);
void updateLCDC(u8 val);
void updateHofs(u8 val);
void updateVofs(u8 val);
void updateSprPalette(int paletteid);
void updateBgPalette(int paletteid);
void updateClassicBgPalette();
void updateClassicSprPalette(int paletteid);
void drawClassicBgPalette(int val);
void drawClassicSprPalette(int paletteid, int val);
void drawBgPalette(int paletteid, int val);
void drawSprPalette(int paletteid, int val);

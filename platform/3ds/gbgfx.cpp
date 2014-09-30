/* Known graphical issues:
 * DMG sprite order
 * Horizontal window split behavior
 */

#include <3ds.h>
#include <string.h>
#include "gbgfx.h"
#include "gameboy.h"
#include "3dsgfx.h"
#include <math.h>

#define OFFSET_X TOP_SCREEN_WIDTH/2 - 160/2
#define OFFSET_Y TOP_SCREEN_HEIGHT/2 - 144/2

// public variables

bool probingForBorder;

int interruptWaitMode;
int scaleMode;
int scaleFilter;
u8 gfxMask;
volatile int loadedBorderType;
bool customBorderExists;
bool sgbBorderLoaded;



// private variables

u32 gbColors[4];
u32 pixels[32*32*64];
u8 tileCache[2][0x1800][8][8];

int tileSize;
int tileSigned = 0;
int tileAddr = 0x8000;
int BGMapAddr = 0x800;
int winMapAddr = 0x800;
int BGOn = 1;
int winOn = 0;
int scale = 3;

u32 bgPalettes[8][4];
u32* bgPalettesRef[8][4];
u32 sprPalettes[8][4];
u32* sprPalettesRef[8][4];

int dmaLine;
bool lineModified;
    

// For drawScanline / drawSprite

u8 spritePixels[256];
u32 spritePixelsTrue[256]; // Holds the palettized colors

u8 spritePixelsLow[256];
u32 spritePixelsTrueLow[256];

u8 bgPixels[256];
u32 bgPixelsTrue[256];
u8 bgPixelsLow[256];
u32 bgPixelsTrueLow[256];


bool bgPalettesModified[8];
bool sprPalettesModified[8];


// Private functions
void drawSprite(int scanline, int spriteNum);

void updateBgPalette(int paletteid);
void updateBgPaletteDMG();
void updateSprPalette(int paletteid);
void updateSprPaletteDMG(int paletteid);


// Function definitions

void doAtVBlank(void (*func)(void)) {
    func();
}
void initGFX()
{
	bgPalettes[0][0] = RGB24(255, 255, 255);
	bgPalettes[0][1] = RGB24(192, 192, 192);
	bgPalettes[0][2] = RGB24(94, 94, 94);
	bgPalettes[0][3] = RGB24(0, 0, 0);
	sprPalettes[0][0] = RGB24(255, 255, 255);
	sprPalettes[0][1] = RGB24(192, 192, 192);
	sprPalettes[0][2] = RGB24(94, 94, 94);
	sprPalettes[0][3] = RGB24(0, 0, 0);
	sprPalettes[1][0] = RGB24(255, 255, 255);
	sprPalettes[1][1] = RGB24(192, 192, 192);
	sprPalettes[1][2] = RGB24(94, 94, 94);
	sprPalettes[1][3] = RGB24(0, 0, 0);

	for (int i=0; i<8; i++)
	{
		sprPalettesRef[i][0] = &sprPalettes[i][0];
		sprPalettesRef[i][1] = &sprPalettes[i][1];
		sprPalettesRef[i][2] = &sprPalettes[i][2];
		sprPalettesRef[i][3] = &sprPalettes[i][3];

		bgPalettesRef[i][0] = &bgPalettes[i][0];
		bgPalettesRef[i][1] = &bgPalettes[i][1];
		bgPalettesRef[i][2] = &bgPalettes[i][2];
		bgPalettesRef[i][3] = &bgPalettes[i][3];
	}

    memset(bgPalettesModified, 0, sizeof(bgPalettesModified));
    memset(sprPalettesModified, 0, sizeof(sprPalettesModified));
}

void refreshGFX() {

}

void clearGFX() {

}

void drawScanline(int scanline)
{
    for (int i=0; i<8; i++) {
        if (bgPalettesModified[i]) {
            if (gameboy->gbMode == GB)
                updateBgPaletteDMG();
            else
                updateBgPalette(i);
            bgPalettesModified[i] = false;
        }
        if (sprPalettesModified[i]) {
            if (gameboy->gbMode == GB)
                updateSprPaletteDMG(i);
            else
                updateSprPalette(i);
            sprPalettesModified[i] = false;
        }
    }

	if (gameboy->ioRam[0x40] & 0x10)	// Tile Data location
	{
		tileAddr = 0x8000;
		tileSigned = 0;
	}
	else
	{
		tileAddr = 0x8800;
		tileSigned = 1;
	}

	if (gameboy->ioRam[0x40] & 0x8)		// Tile Map location
	{
		BGMapAddr = 0x1C00;
	}
	else
	{
		BGMapAddr = 0x1800;
	}
	if (gameboy->ioRam[0x40] & 0x40)
		winMapAddr = 0x1C00;
	else
		winMapAddr = 0x1800;
	if (gameboy->ioRam[0x40] & 0x20)
		winOn = 1;
	else
		winOn = 0;

	for (int i=0; i<256; i++)
	{
	//	pixels[i+(scanline*256)] = 0;
		bgPixels[i] = 5;
		bgPixelsLow[i] = 5;
		spritePixels[i] = 0;
		spritePixelsLow[i] = 0;
	}
	if (gameboy->ioRam[0x40] & 0x2)
	{
		for (int i=39; i>=0; i--)
		{
			drawSprite(scanline, i);
		}
	}

	if (BGOn)
	{
		u8 scrollX = gameboy->ioRam[0x43];
		int scrollY = gameboy->ioRam[0x42];
		// The y position (measured in tiles)
		int tileY = ((scanline+scrollY)&0xFF)/8;
		for (int i=0; i<32; i++)
		{
			int mapAddr = BGMapAddr+i+(tileY*32);		// The address (from beginning of gameboy->vram) of the tile's mapping
			// This is the tile id.
			int tileNum = gameboy->vram[0][mapAddr];
			if (tileSigned)
				tileNum = ((s8)tileNum)+128+0x80;

			// This is the tile's Y position to be read (0-7)
			int pixelY = (scanline+scrollY)%8;
			int flipX = 0, flipY = 0;
			int bank = 0;
			int paletteid = 0;
			int priority = 0;

			if (gameboy->gbMode == CGB)
			{
				flipX = !!(gameboy->vram[1][mapAddr] & 0x20);
				flipY = !!(gameboy->vram[1][mapAddr] & 0x40);
				bank = !!(gameboy->vram[1][mapAddr] & 0x8);
				paletteid = gameboy->vram[1][mapAddr] & 0x7;
				priority = !!(gameboy->vram[1][mapAddr] & 0x80);
			}

			if (flipY)
			{
				pixelY = 7-pixelY;
			}

			for (int x=0; x<8; x++)
			{
				int colorid;
				u32 color;

				if (flipX)
				{
					colorid = !!(gameboy->vram[bank][(tileNum<<4)+(pixelY<<1)] & (0x80>>(7-x)));
					colorid |= !!(gameboy->vram[bank][(tileNum<<4)+(pixelY<<1)+1] & (0x80>>(7-x)))<<1;
				}
				else
				{
					colorid = !!(gameboy->vram[bank][(tileNum<<4)+(pixelY<<1)] & (0x80>>x));
					colorid |= !!(gameboy->vram[bank][(tileNum<<4)+(pixelY<<1)+1] & (0x80>>x))<<1;
				}
				// The x position to write to pixels[].
				u32 writeX = ((i*8)+x-scrollX)&0xFF;

				color = *bgPalettesRef[paletteid][colorid];
				if (priority)
				{
					bgPixels[writeX] = colorid;
					bgPixelsTrue[writeX] = color;
				}
				else
				{
					bgPixelsLow[writeX] = colorid;
					bgPixelsTrueLow[writeX] = color;
				}
				//spritePixels[writeX] = 0;
			}
		}
	}
	// Draw window
	int winX = gameboy->ioRam[0x4B]-7;
	int winY = gameboy->ioRam[0x4A];
	if (scanline >= winY && winOn)
	{
		int tileY = (scanline-winY)/8;
		for (int i=0; i<32; i++)
		{
			int mapAddr = winMapAddr+i+(tileY*32);
			// This is the tile id.
			int tileNum = gameboy->vram[0][mapAddr];
			if (tileSigned)
				tileNum = ((s8)tileNum)+128+0x80;

			int pixelY = (scanline-winY)%8;
			int flipX = 0, flipY = 0;
			int bank = 0;
			int paletteid = 0;
			int priority = 0;

			if (gameboy->gbMode == CGB)
			{
				flipX = !!(gameboy->vram[1][mapAddr] & 0x20);
				flipY = !!(gameboy->vram[1][mapAddr] & 0x40);
				bank = !!(gameboy->vram[1][mapAddr]&0x8);
				paletteid = gameboy->vram[1][mapAddr]&0x7;
				priority = !!(gameboy->vram[1][mapAddr] & 0x80);
			}

			if (flipY)
				pixelY = 7-pixelY;
			for (int x=0; x<8; x++)
			{
				int colorid;
				u32 color;

				if (flipX)
				{
					colorid = !!(gameboy->vram[bank][(tileNum<<4)+(pixelY<<1)] & (0x80>>(7-x)));
					colorid |= !!(gameboy->vram[bank][(tileNum<<4)+(pixelY<<1)+1] & (0x80>>(7-x)))<<1;
				}
				else
				{
					colorid = !!(gameboy->vram[bank][(tileNum<<4)+(pixelY<<1)] & (0x80>>x));
					colorid |= !!(gameboy->vram[bank][(tileNum<<4)+(pixelY<<1)+1] & (0x80>>x))<<1;
				}

				int writeX = (i*8)+x+winX;
				if (writeX >= 168)
					break;

				color = *bgPalettesRef[paletteid][colorid];
				if (priority)
				{
					bgPixels[writeX] = colorid;
					bgPixelsTrue[writeX] = color;
					bgPixelsLow[writeX] = 5;
				}
				else
				{
					bgPixelsLow[writeX] = colorid;
					bgPixelsTrueLow[writeX] = color;
					bgPixels[writeX] = 5;
				}
			}
		}
	}
	//if (gameboy->ioRam[0x40] & 0x2)
	{
		int dest = scanline*256;
		if (gameboy->ioRam[0x40] & 0x1)
		{
			for (int i=0; i<256; i++, dest++)
			{
				pixels[dest] = spritePixelsTrueLow[i];
				if ((bgPixelsLow[i] > 0 || spritePixelsLow[i] == 0) && bgPixelsLow[i] != 5)
					pixels[dest] = bgPixelsTrueLow[i];
				if (spritePixels[i] != 0)
					pixels[dest] = spritePixelsTrue[i];
				if ((bgPixels[i] != 0 || (spritePixels[i] == 0 && spritePixelsLow[i] == 0)) && bgPixels[i] != 5)
					pixels[dest] = bgPixelsTrue[i];
				/*if (spritePixels[i] != 0)
				  {
				  pixels[i+(scanline*256)] = spritePixelsTrue[i];
				  spritePixels[i] = 0;
				  }
				 */
			}
		}
		else
		{
			//printf("kewl");
			for (int i=0; i<256; i++, dest++)
			{
				if ((bgPixelsLow[i] > 0) && bgPixelsLow[i] != 5)
					pixels[dest] = bgPixelsTrueLow[i];
				if ((bgPixels[i] != 0) && bgPixels[i] != 5)
					pixels[dest] = bgPixelsTrue[i];

				if (spritePixelsLow[i] != 0)
					pixels[dest] = spritePixelsTrueLow[i];
				if (spritePixels[i] != 0)
					pixels[dest] = spritePixelsTrue[i];
				/*if (spritePixels[i] != 0)
				  {
				  pixels[i+(scanline*256)] = spritePixelsTrue[i];
				  spritePixels[i] = 0;
				  }
				 */
			}
		}
	}
}

void drawScanline_P2(int scanline) {

}

void drawScreen()
{
    u8* framebuffer = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
    for (int y=0; y<144; y++) {
        for (int x=0; x<160; x++) {
            drawPixel(framebuffer, x+OFFSET_X, y+OFFSET_Y, pixels[x+y*256]);
        }
    }
}


void displayIcon(int iconid) {

}


void selectBorder() {

}

int loadBorder(const char* filename) {

}

void checkBorder() {

}

void refreshScaleMode() {

}


// SGB stub functions
void refreshSgbPalette() {

}
void setSgbMask(int mask) {

}
void setSgbTiles(u8* src, u8 flags) {

}
void setSgbMap(u8* src) {

}


void writeVram(u16 addr, u8 val) {
}

void writeVram16(u16 addr, u16 src) {
}

void writeHram(u16 addr, u8 val) {
    gameboy->hram[addr&0x1ff] = val;
}

void handleVideoRegister(u8 ioReg, u8 val) {
    switch(ioReg) {
        case 0x47:
            if (gameboy->gbMode == GB)
                bgPalettesModified[0] = true;
            break;
        case 0x48:
            if (gameboy->gbMode == GB)
                sprPalettesModified[0] = true;
            break;
        case 0x49:
            if (gameboy->gbMode == GB)
                sprPalettesModified[1] = true;
            break;
        case 0x69:
            if (gameboy->gbMode == CGB)
                bgPalettesModified[(gameboy->ioRam[0x68]/8)&7] = true;
            break;
        case 0x6B:
            if (gameboy->gbMode == CGB)
                sprPalettesModified[(gameboy->ioRam[0x6A]/8)&7] = true;
            break;
        default:
            break;
    }
}

void updateBgPalette(int paletteid)
{
    double multiplier = 8;
    int i;
    for (i=0; i<4; i++)
    {
        int red = (gameboy->bgPaletteData[(paletteid*8)+(i*2)]&0x1F)*multiplier;
        int green = (((gameboy->bgPaletteData[(paletteid*8)+(i*2)]&0xE0) >> 5) |
                ((gameboy->bgPaletteData[(paletteid*8)+(i*2)+1]) & 0x3) << 3)*multiplier;
        int blue = ((gameboy->bgPaletteData[(paletteid*8)+(i*2)+1] >> 2) & 0x1F)*multiplier;
        bgPalettes[paletteid][i] = RGB24(red, green, blue);
    }
}

void updateBgPaletteDMG()
{
	u8 val = gameboy->ioRam[0x47];
	u8 palette[] = {val&3, (val>>2)&3, (val>>4)&3, (val>>6)};

	for (int i=0; i<4; i++)
		bgPalettesRef[0][i] = &bgPalettes[0][palette[i]];
}

void updateSprPalette(int paletteid)
{
    double multiplier = 8;
    int i;
    for (i=0; i<4; i++)
    {
        int red = (gameboy->sprPaletteData[(paletteid*8)+(i*2)]&0x1F)*multiplier;
        int green = (((gameboy->sprPaletteData[(paletteid*8)+(i*2)]&0xE0) >> 5) |
                ((gameboy->sprPaletteData[(paletteid*8)+(i*2)+1]) & 0x3) << 3)*multiplier;
        int blue = ((gameboy->sprPaletteData[(paletteid*8)+(i*2)+1] >> 2) & 0x1F)*multiplier;
        sprPalettes[paletteid][i] = RGB24(red, green, blue);
    }
}

void updateSprPaletteDMG(int paletteid)
{
	int val = gameboy->ioRam[0x48+paletteid];
	u8 palette[] = {val&3, (val>>2)&3, (val>>4)&3, (val>>6)};

	for (int i=0; i<4; i++)
		sprPalettesRef[paletteid][i] = &sprPalettes[paletteid][palette[i]];
}

void drawSprite(int scanline, int spriteNum)
{
	// The sprite's number, times 4 (each uses 4 bytes)
	spriteNum *= 4;
	int tileNum = gameboy->hram[spriteNum+2];
	int x = (gameboy->hram[spriteNum+1]-8);
	int y = (gameboy->hram[spriteNum]-16);
	int height;
	if (gameboy->ioRam[0x40] & 0x4)
		height = 16;
	else
		height = 8;
	int bank = 0;
	int flipX = (gameboy->hram[spriteNum+3] & 0x20);
	int flipY = (gameboy->hram[spriteNum+3] & 0x40);
	int priority = !(gameboy->hram[spriteNum+3] & 0x80);
	int paletteid;

	if (gameboy->gbMode == CGB)
	{
		bank = !!(gameboy->hram[spriteNum+3]&0x8);
		paletteid = gameboy->hram[spriteNum+3] & 0x7;
	}
	else
	{
		//paletteid = gameboy->hram[spriteNum+3] & 0x7;
		paletteid = !!(gameboy->hram[spriteNum+3] & 0x10);
	}

	if (height == 16)
		tileNum &= ~1;
	if (scanline >= y && scanline < y+height)
	{
		if (scanline-y >= 8)
			tileNum++;

		//u8* tile = &memory[0]+((tileNum)*16)+0x8000;//tileAddr;
		int pixelY = (scanline-y)%8;
		int j;

		if (flipY)
		{
			pixelY = 7-pixelY;
			if (height == 16)
				tileNum = tileNum^1;
		}
		for (j=0; j<8; j++)
		{
			int color;
			int trueColor;

			color = !!(gameboy->vram[bank][(tileNum<<4)+(pixelY<<1)] & (0x80>>j));
			color |= !!(gameboy->vram[bank][(tileNum<<4)+(pixelY<<1)+1] & (0x80>>j))<<1;
			if (color != 0)
			{
				trueColor = *sprPalettesRef[paletteid][color];
				u32* trueDest = (priority ? spritePixelsTrue : spritePixelsTrueLow);
				u8* idDest = (priority ? spritePixels : spritePixelsLow);

				if (flipX)
				{
					idDest[(x+(7-j))&0xFF] = color;
					trueDest[(x+(7-j))&0xFF] = trueColor;
				}
				else
				{
					idDest[(x+j)&0xFF] = color;
					trueDest[(x+j)&0xFF] = trueColor;
				}
			}
		}
	}
}


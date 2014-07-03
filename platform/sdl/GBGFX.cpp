#include "GBGFX.h"
#include "MMU.h"
#include "GBCPU.h"
#include <math.h>
#include <SDL/SDL.h>
#include <GL/gl.h>

u32 gbColors[4];
Uint32 pixels[32*32*64];
u8 tileCache[2][0x1800][8][8];

int tileSize;
int tileSigned = 0;
int tileAddr = 0x8000;
int BGMapAddr = 0x800;
int winMapAddr = 0x800;
int BGOn = 1;
int winOn = 0;
int scale = 3;

int drawVram = 0;

u32 bgPalettes[8][4];
u32* bgPalettesRef[8][4];
u8 bgPaletteData[0x40];
u32 sprPalettes[8][4];
u32* sprPalettesRef[8][4];
u8 sprPaletteData[0x40];

int changedTileQueueLength;
u16 changedTileQueue[0x300];
bool changedTile[2][0x180];

int changedTileInFrameQueueLength;
u16 changedTileInFrameQueue[0x300];
bool changedTileInFrame[2][0x180];
int dmaLine;
bool lineModified;

SDL_PixelFormat* format;
    
void initGFX()
{
	gbColors[0] = 255;
	gbColors[1] = 192;
	gbColors[2] = 94;
	gbColors[3] = 0;

	//Set Clear Color
	glClearColor(0, 0, 0, 0);

	glOrtho(0, 160, 144, 0, -1, 1); //Sets orthographic (2D) projection

	glRasterPos2f(0, 0);
	glPixelZoom(scale, -scale);

	SDL_Surface* gbScreen = SDL_CreateRGBSurface(SDL_SWSURFACE, 256*scale, 256*scale, 32, 0, 0, 0, 0);
	format = gbScreen->format;

	bgPalettes[0][0] = SDL_MapRGB(gbScreen->format, 255, 255, 255);
	bgPalettes[0][1] = SDL_MapRGB(gbScreen->format, 192, 192, 192);
	bgPalettes[0][2] = SDL_MapRGB(gbScreen->format, 94, 94, 94);
	bgPalettes[0][3] = SDL_MapRGB(gbScreen->format, 0, 0, 0);
	sprPalettes[0][0] = SDL_MapRGB(gbScreen->format, 255, 255, 255);
	sprPalettes[0][1] = SDL_MapRGB(gbScreen->format, 192, 192, 192);
	sprPalettes[0][2] = SDL_MapRGB(gbScreen->format, 94, 94, 94);
	sprPalettes[0][3] = SDL_MapRGB(gbScreen->format, 0, 0, 0);
	sprPalettes[1][0] = SDL_MapRGB(gbScreen->format, 255, 255, 255);
	sprPalettes[1][1] = SDL_MapRGB(gbScreen->format, 192, 192, 192);
	sprPalettes[1][2] = SDL_MapRGB(gbScreen->format, 94, 94, 94);
	sprPalettes[1][3] = SDL_MapRGB(gbScreen->format, 0, 0, 0);

	int i;

	for (i=0; i<8; i++)
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
	//drawPatternTable();
}

// These functions are used on the DS only.
void drawBgTile(int tile, int bank)
{
}
void drawSprTile(int tile, int bank)
{
}

// Used only in drawSprite and drawScanline
u8 spritePixels[256];
// This holds the palettized colours.
Uint32 spritePixelsTrue[256];

// Low-priority.
u8 spritePixelsLow[256];
Uint32 spritePixelsTrueLow[256];

u8 bgPixels[256];
Uint32 bgPixelsTrue[256];
u8 bgPixelsLow[256];
Uint32 bgPixelsTrueLow[256];
void drawSprite(int scanline, int spriteNum)
{
	// The sprite's number, times 4 (each uses 4 bytes)
	spriteNum *= 4;
	int tileNum = hram[spriteNum+2];
	int x = (hram[spriteNum+1]-8);
	int y = (hram[spriteNum]-16);
	int height;
	if (ioRam[0x40] & 0x4)
		height = 16;
	else
		height = 8;
	int bank = 0;
	int flipX = (hram[spriteNum+3] & 0x20);
	int flipY = (hram[spriteNum+3] & 0x40);
	int priority = !(hram[spriteNum+3] & 0x80);
	int paletteid;

	if (gbMode == CGB)
	{
		bank = !!(hram[spriteNum+3]&0x8);
		paletteid = hram[spriteNum+3] & 0x7;
	}
	else
	{
		//paletteid = hram[spriteNum+3] & 0x7;
		paletteid = !!(hram[spriteNum+3] & 0x10);
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

			color = !!(vram[bank][(tileNum<<4)+(pixelY<<1)] & (0x80>>j));
			color |= !!(vram[bank][(tileNum<<4)+(pixelY<<1)+1] & (0x80>>j))<<1;
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

void drawScanline(int scanline)
{
	if (drawVram)
		return;
	if (ioRam[0x40] & 0x10)	// Tile Data location
	{
		tileAddr = 0x8000;
		tileSigned = 0;
	}
	else
	{
		tileAddr = 0x8800;
		tileSigned = 1;
	}

	if (ioRam[0x40] & 0x8)		// Tile Map location
	{
		BGMapAddr = 0x1C00;
	}
	else
	{
		BGMapAddr = 0x1800;
	}
	if (ioRam[0x40] & 0x40)
		winMapAddr = 0x1C00;
	else
		winMapAddr = 0x1800;
	if (ioRam[0x40] & 0x20)
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
	if (ioRam[0x40] & 0x2)
	{
		for (int i=39; i>=0; i--)
		{
			drawSprite(scanline, i);
		}
	}

	if (BGOn)
	{
		u8 scrollX = ioRam[0x43];
		int scrollY = ioRam[0x42];
		// The y position (measured in tiles)
		int tileY = ((scanline+scrollY)&0xFF)/8;
		for (int i=0; i<32; i++)
		{
			int mapAddr = BGMapAddr+i+(tileY*32);		// The address (from beginning of vram) of the tile's mapping
			// This is the tile id.
			int tileNum = vram[0][mapAddr];
			if (tileSigned)
				tileNum = ((s8)tileNum)+128+0x80;

			// This is the tile's Y position to be read (0-7)
			int pixelY = (scanline+scrollY)%8;
			int flipX = 0, flipY = 0;
			int bank = 0;
			int paletteid = 0;
			int priority = 0;

			if (gbMode == CGB)
			{
				flipX = !!(vram[1][mapAddr] & 0x20);
				flipY = !!(vram[1][mapAddr] & 0x40);
				bank = !!(vram[1][mapAddr] & 0x8);
				paletteid = vram[1][mapAddr] & 0x7;
				priority = !!(vram[1][mapAddr] & 0x80);
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
					colorid = !!(vram[bank][(tileNum<<4)+(pixelY<<1)] & (0x80>>(7-x)));
					colorid |= !!(vram[bank][(tileNum<<4)+(pixelY<<1)+1] & (0x80>>(7-x)))<<1;
				}
				else
				{
					colorid = !!(vram[bank][(tileNum<<4)+(pixelY<<1)] & (0x80>>x));
					colorid |= !!(vram[bank][(tileNum<<4)+(pixelY<<1)+1] & (0x80>>x))<<1;
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
	int winX = ioRam[0x4B]-7;
	int winY = ioRam[0x4A];
	if (scanline >= winY && winOn)
	{
		int tileY = (scanline-winY)/8;
		for (int i=0; i<32; i++)
		{
			int mapAddr = winMapAddr+i+(tileY*32);
			// This is the tile id.
			int tileNum = vram[0][mapAddr];
			if (tileSigned)
				tileNum = ((s8)tileNum)+128+0x80;

			int pixelY = (scanline-winY)%8;
			int flipX = 0, flipY = 0;
			int bank = 0;
			int paletteid = 0;
			int priority = 0;

			if (gbMode == CGB)
			{
				flipX = !!(vram[1][mapAddr] & 0x20);
				flipY = !!(vram[1][mapAddr] & 0x40);
				bank = !!(vram[1][mapAddr]&0x8);
				paletteid = vram[1][mapAddr]&0x7;
				priority = !!(vram[1][mapAddr] & 0x80);
			}

			if (flipY)
				pixelY = 7-pixelY;
			for (int x=0; x<8; x++)
			{
				int colorid;
				u32 color;

				if (flipX)
				{
					colorid = !!(vram[bank][(tileNum<<4)+(pixelY<<1)] & (0x80>>(7-x)));
					colorid |= !!(vram[bank][(tileNum<<4)+(pixelY<<1)+1] & (0x80>>(7-x)))<<1;
				}
				else
				{
					colorid = !!(vram[bank][(tileNum<<4)+(pixelY<<1)] & (0x80>>x));
					colorid |= !!(vram[bank][(tileNum<<4)+(pixelY<<1)+1] & (0x80>>x))<<1;
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
	//if (ioRam[0x40] & 0x2)
	{
		int dest = scanline*256;
		if (ioRam[0x40] & 0x1)
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

void drawScreen()
{
	if (!drawVram)
		glDrawPixels(256, 144, GL_BGRA, GL_UNSIGNED_BYTE, pixels);
	else
	{
		int bank = drawVram-1;
		for (int t=0; t<32*32; t++)
		{
			int tileX = (t%32)*8;
			int tileY = (t/32)*256*8;

			for (int y=0; y<8; y++)
			{
				int colors1 = vram[bank][(t*16)+(y*2)];
				int colors2 = vram[bank][(t*16)+(y*2)+1];
				for (int x=0; x<8; x++)
				{
					int colorid = !!(colors1 & (0x80>>x));
					colorid |= !!(colors2 & (0x80>>x))<<1;
					int color = gbColors[colorid];
					pixels[tileX+tileY+(y*256)+x] = SDL_MapRGB(format, color, color, color);
				}
			}
		}
		glDrawPixels(256, 256, GL_BGRA, GL_UNSIGNED_BYTE, pixels);
	}
	SDL_GL_SwapBuffers();

	/*
	Uint32* screenPixels = gbScreen->pixels;
	int t,x,y;
	int var;
	for (t=0; t<256*144; t++)
	{
		screenPixels[t] = pixels[t];
	}
	SDL_Rect rect;
	rect.x = 0;
	rect.y = 0;
	rect.w = 160*scale;
	rect.h = 144*scale;
	SDL_BlitSurface(gbScreen, &rect, screen, NULL);
	SDL_Flip(screen);*/
}

void updateBgPalette(int paletteid)
{
	//if (gbMode == CGB)
	{
		double multiplier = 8;
		int i;
		for (i=0; i<4; i++)
		{
			int red = (bgPaletteData[(paletteid*8)+(i*2)]&0x1F)*multiplier;
			int green = (((bgPaletteData[(paletteid*8)+(i*2)]&0xE0) >> 5) |
					((bgPaletteData[(paletteid*8)+(i*2)+1]) & 0x3) << 3)*multiplier;
			int blue = ((bgPaletteData[(paletteid*8)+(i*2)+1] >> 2) & 0x1F)*multiplier;
			bgPalettes[paletteid][i] = SDL_MapRGB(format, red, green, blue);
		}
	}
	/*else
	{
		int val = ioRam[0x47];
		bgPalettes[paletteid][0] = gbColors[(val)&3];
		bgPalettes[paletteid][1] = gbColors[(val>>2)&3];
		bgPalettes[paletteid][2] = gbColors[(val>>4)&3];
		bgPalettes[paletteid][3] = gbColors[(val>>6)];
	}*/
}

void updateSprPalette(int paletteid)
{
	//if (gbMode == CGB)
	{
		double multiplier = 8;
		int i;
		for (i=0; i<4; i++)
		{
			int red = (sprPaletteData[(paletteid*8)+(i*2)]&0x1F)*multiplier;
			int green = (((sprPaletteData[(paletteid*8)+(i*2)]&0xE0) >> 5) |
					((sprPaletteData[(paletteid*8)+(i*2)+1]) & 0x3) << 3)*multiplier;
			int blue = ((sprPaletteData[(paletteid*8)+(i*2)+1] >> 2) & 0x1F)*multiplier;
			sprPalettes[paletteid][i] = SDL_MapRGB(format, red, green, blue);
		}
	}
	/*else
	{
		int val = ioRam[0x48+paletteid];
		sprPalettes[paletteid][0] = gbColors[(val)&3];
		sprPalettes[paletteid][1] = gbColors[(val>>2)&3];
		sprPalettes[paletteid][2] = gbColors[(val>>4)&3];
		sprPalettes[paletteid][3] = gbColors[(val>>6)];
	}*/
}

void updateClassicBgPalette()
{
	int val = ioRam[0x47];
	u8 palette[] = {val&3, (val>>2)&3, (val>>4)&3, (val>>6)};

	for (int i=0; i<4; i++)
		bgPalettesRef[0][i] = &bgPalettes[0][palette[i]];
}

void updateClassicSprPalette(int paletteid)
{
	int val = ioRam[0x48+paletteid];
	u8 palette[] = {val&3, (val>>2)&3, (val>>4)&3, (val>>6)};

	for (int i=0; i<4; i++)
		sprPalettesRef[paletteid][i] = &sprPalettes[paletteid][palette[i]];
}

void enableScreen() {
}
void disableScreen() {
}
void updateTiles() {
}
void updateTileMap(int m, int i, u8 val) {
}

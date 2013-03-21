#include <nds.h>
#include <stdio.h>
#include <math.h>
#include "gbgfx.h"
#include "mmu.h"
#include "gbcpu.h"
#include "main.h"
#include "gameboy.h"

u8 bgPaletteData[0x40];
u8 sprPaletteData[0x40];

const int off_map_base = 25;

int screenOffsX = 48;
int screenOffsY = 24;

u8* pixels = (u8*)BG_GFX;
int screenBg;
//int colors[4];
u8 mapImage[2][256*256];
bool renderingBankA=true;

u16 bgPalettes[8][4];
u16 sprPalettes[8][4];

bool tileModified[2][0x180];
int tileModifiedQueue[0x300];
int tileModifiedQueueLength=0;
u8 tileImages[2][0x180][8][8];

// Frame counter. Incremented each vblank.
u8 frame=0;

// Whether to wait for vblank if emulation is running behind schedule
int interruptWaitMode=0;

bool windowDisabled = false;
bool hblankDisabled = false;

void drawTile(int bank, int tile);
void updateTiles();
void updateTileMap(int map, int i, u8 val);
void drawScanline(int scanline) ITCM_CODE;
void setScaleMode(int m);

void hblankHandler()
{
}

void vblankHandler()
{
    frame++;
}

void initGFX()
{
    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankB(VRAM_B_MAIN_BG);
    vramSetBankE(VRAM_E_MAIN_SPRITE);

    // Backdrop
    BG_PALETTE[0] = RGB8(0,0,0);

    // Tile for "Blank maps", which contain only the gameboy's color 0.
    for (int i=0; i<16; i++) {
        BG_GFX[i] = 1 | (1<<4) | (1<<8) | (1<<12);
    }

    sprPaletteData[0] = 0xff;
    sprPaletteData[1] = 0xff;
    sprPaletteData[2] = 0x15|((0x15&7)<<5);
    sprPaletteData[3] = (0x15>>3)|(0x15<<2);
    sprPaletteData[4] = 0xa|((0xa&7)<<5);
    sprPaletteData[5] = (0xa>>3)|(0xa<<2);
    sprPaletteData[6] = 0;
    sprPaletteData[7] = 0;
    sprPaletteData[8] = 0xff;
    sprPaletteData[9] = 0xff;
    sprPaletteData[10] = 0x15|((0x15&7)<<5);
    sprPaletteData[11] = (0x15>>3)|(0x15<<2);
    sprPaletteData[12] = 0xa|((0xa&7)<<5);
    sprPaletteData[13] = (0xa>>3)|(0xa<<2);
    sprPaletteData[14] = 0;
    sprPaletteData[15] = 0;
    bgPaletteData[0] = 0xff;
    bgPaletteData[1] = 0xff;
    bgPaletteData[2] = 0x15|((0x15&7)<<5);
    bgPaletteData[3] = (0x15>>3)|(0x15<<2);
    bgPaletteData[4] = 0xa|((0xa&7)<<5);
    bgPaletteData[5] = (0xa>>3)|(0xa<<2);
    bgPaletteData[6] = 0;
    bgPaletteData[7] = 0;

    videoSetMode(MODE_5_2D | DISPLAY_WIN0_ON | DISPLAY_BG2_ACTIVE);
    REG_BG2CNT = BG_BMP8_256x256;

    WIN_IN = 1<<2;
    WIN_OUT = 0;
    setScaleMode(0);

    REG_DISPSTAT &= 0xFF;
    REG_DISPSTAT |= (144+screenOffsY)<<8;

    irqEnable(IRQ_VCOUNT);
    //irqEnable(IRQ_HBLANK);
    irqEnable(IRQ_VBLANK);
    irqSet(IRQ_VBLANK, &vblankHandler);
    //irqSet(IRQ_HBLANK, &hblankHandler);

    for (int i=0; i<0x180; i++)
    {
        drawTile(0, i);
        drawTile(1, i);
    }
}

void refreshGFX() {
}

void updateTileMap(int m, int tile, int y) {
    u8 lcdc = ioRam[0x40];
    int tileSigned = !(lcdc&0x10);
    int bgMap = (m ? 0x1c00 : 0x1800);
    int addr = bgMap+tile;
    int tileNum;
    if (tileSigned)
        tileNum = 0x100+(s8)vram[0][addr];
    else
        tileNum = vram[0][addr];

    int paletteid=0, bank=0;
    if (gbMode == CGB) {
        u8 flags = vram[1][addr];
        paletteid = flags&7;
        bank = (flags>>3)&1;
    }
    u8* dest = &mapImage[m][tile/32*8*256+(tile%32)*8];
    if (y == -1) {
        for (int y=0; y<8; y++) {
            u8 b1 = vram[bank][tileNum*0x10+y*2];
            u8 b2 = vram[bank][tileNum*0x10+y*2+1];
            for (int x=0; x<8; x++) {
                u8 color = (b1>>7)&1;
                b1 <<= 1;
                color |= (b2>>6)&2;
                b2 <<= 1;
                *(dest++) = paletteid*16+color+1;
            }
            dest += 256-8;
        }
    }
    else {
        u8 b1 = vram[bank][tileNum*0x10+y*2];
        u8 b2 = vram[bank][tileNum*0x10+y*2+1];
        for (int x=0; x<8; x++) {
            u8 color = (b1>>7)&1;
            b1 <<= 1;
            color |= (b2>>6)&2;
            b2 <<= 1;
            *(dest++) = paletteid*16+color+1;
        }
        dest += 256-8;
    }
}

void setScaleMode(int mode) {
    switch(mode) {
        case 0:
            WIN0_X0 = screenOffsX;
            WIN0_Y0 = screenOffsY;
            WIN0_X1 = screenOffsX+160;
            WIN0_Y1 = screenOffsY+144;

            REG_BG2PA = 1<<8;
            REG_BG2PB = 0;
            REG_BG2PC = 0;
            REG_BG2PD = 1<<8;
            REG_BG2X = -screenOffsX<<8;
            REG_BG2Y = -screenOffsY<<8;
            break;
        case 1:
            {
                WIN0_X0 = 0;
                WIN0_Y0 = 0;
                WIN0_X1 = 255;
                WIN0_Y1 = 191;
                REG_BG2PA = (1<<8)/((double)192/144);
                REG_BG2PB = 0;
                REG_BG2PC = 0;
                REG_BG2PD = (1<<8)/((double)192/144);
                REG_BG2X = -((256-((double)192/144)*160)/2)*256;
                REG_BG2Y = 0;
                break;
            }
        case 2:
            WIN0_X0 = 0;
            WIN0_Y0 = 0;
            WIN0_X1 = 255;
            WIN0_Y1 = 191;
            REG_BG2PA = (1<<8)/((double)256/160);
            REG_BG2PB = 0;
            REG_BG2PC = 0;
            REG_BG2PD = (1<<8)/((double)192/144);
            REG_BG2X = 0;
            REG_BG2Y = 0;
            break;
    }
}

void disableScreen() {
}
void enableScreen() {
}

void drawTile(int bank, int tileNum) {
    tileModified[bank][tileNum] = false;
    return;
    u8 lcdc = ioRam[0x40];
    int tileSigned = !(lcdc&0x10);
    for (int i=0; i<2; i++) {
        for (int t=0; t<32*32; t++) {
            bool cond;
           if (tileSigned)
              cond = 0x100+(s8)vram[0][0x1800+i*0x400+t] == tileNum;
           else
              cond = vram[0][0x1800+i*0x400+t] == tileNum;
           if (cond)
               updateTileMap(i, t, -1);
        }
    }
    /*
    for (int y=0; y<8; y++) {
        u8 b1=vram[bank][(tileNum<<4)+(y<<1)];
        u8 b2=vram[bank][(tileNum<<4)+(y<<1)+1];
        for (int x=0; x<8; x++) {
            int colorid;
            colorid = !!(b1&0x80);
            colorid |= !!(b2&0x80)<<1;
            b1 <<= 1;
            b2 <<= 1;

            tileImages[bank][tileNum][y][x] = colorid+1;
        }
    }
    */
}

void drawScreen() {
    swiIntrWait(interruptWaitMode, IRQ_VBLANK);
    if (renderingBankA) {
        REG_BG2CNT = BG_BMP8_256x256;
        pixels = ((u8*)BG_GFX)+0x10000;
        renderingBankA = false;
    }
    else {
        REG_BG2CNT = BG_BMP8_256x256 | BG_MAP_BASE(4);
        pixels = ((u8*)BG_GFX);
        renderingBankA = true;
    }
    /*
    u8* src = pixels;
    u8* dest = (u8*)BG_GFX;
    for (int i=0; i<144; i++) {
        dmaCopyAsynch(src, dest, 160);
        src += 256;
        dest += 256;
    }
    */
}

void drawScanline(int scanline) {
    while (tileModifiedQueueLength != 0) {
        tileModifiedQueueLength--;
        int val = tileModifiedQueue[tileModifiedQueueLength];
        int bank = val>>9;
        int tileNum = val&0x1ff;
        drawTile(bank, tileNum);
    }

    u8 lcdc = ioRam[0x40];
    int bgMap = (lcdc&8 ? 1 : 0);
    int winMap = (lcdc&0x40 ? 1 : 0);
    int tileSigned = !(lcdc&0x10);
    int tallSprites = lcdc&4;
    int winOn = lcdc&0x20;
    int spritesOn = lcdc&2;
    u8 hofs = ioRam[0x43];
    u8 vofs = ioRam[0x42];
    int winX = ioRam[0x4b]-7;
    int winY = ioRam[0x4a];

    // Background
    int realy = (scanline+vofs)%256;
    int ypix = realy%8;
    int tile = (realy/8)*32+hofs/8;
    int x=hofs%8;
    int end = 21;

    if (winOn && winY <= scanline)
        end = winX/8+1;
    int loopStart = 256-hofs;
    if (loopStart >= 160)
        dmaCopy(mapImage[bgMap]+realy*256+hofs, pixels+scanline*256, 160);
    else {
        dmaCopy(mapImage[bgMap]+realy*256+hofs, pixels+scanline*256, loopStart+1);
        dmaCopy(mapImage[bgMap]+realy*256, pixels+scanline*256+loopStart, 160-loopStart+1);
    }

    // Window
    /*
    if (winOn && winY <= scanline) {
        dest = pixels+scanline*256+winX;
        u16 *destStart=dest;
        if (winX < 0)
            destStart = dest-winX;
        realy = (scanline-winY)%256;
        ypix = realy%8;
        tile = (realy/8)*32-winX/8;
        int x=7-winX%8;
        end = 20-winX/8+1;
        for (int t=0; t<end; t++) {
            int tileNum;
            if (tileSigned)
                tileNum = 0x100+(s8)vram[0][winMap+tile];
            else
                tileNum = vram[0][winMap+tile];
            u8 *src = tileImages[0][tileNum][ypix];

            while (x < 8) {
                if (dest >= destStart) {
                    *dest = src[x];
                }
                dest++;
                x++;
            }
            x = 0;
            tile++;
        }
    }
    */

    if (spritesOn) {
        u16* dest = (u16*)(pixels+scanline*256);
        for (int s=39; s>=0; s--) {
            int index = s*4;
            int posY = hram[index];
            bool cond;
            if (tallSprites)
                cond = posY-16 > scanline-16;
            else
                cond = posY-16 > scanline-8;
            if (cond && posY-16 <= scanline) {
                int ypix = -((posY-16)-scanline);
                int tileNum = hram[index+2];
                if (tallSprites)
                    tileNum &= ~1;
                int posX = hram[index+1]-8;
                u8 flags = hram[index+3];
                int flipX = flags&0x20;
                int flipY = flags&0x40;
                int paletteid, bank;
                if (gbMode == CGB) {
                    paletteid = flags&7;
                    bank = (flags>>3)&1;
                }
                else {
                    paletteid = (flags>>4)&1;
                    bank = 0;
                }
                int dir=1;
                int dx = posX;
                if (flipX) {
                    dir = -1;
                    dx += 7;
                }
                u8 b1,b2;
                if (flipY) {
                    int ind = tileNum*0x10+((tallSprites?15:7)-ypix)*2;
                    b1 = vram[bank][ind];
                    b2 = vram[bank][ind+1];
                }
                else {
                    int ind = tileNum*0x10+ypix*2;
                    b1 = vram[bank][ind];
                    b2 = vram[bank][ind+1];
                }
                for (x=0; x<8; x++) {
                    u8 color = (b1>>7)&1;
                    b1 <<= 1;
                    color |= (b2>>6)&2;
                    b2 <<= 1;
                    if (dx >= 0 && color != 0) {
                        if (dx%2 == 0) {
                            dest[dx/2] &= 0xff00;
                            dest[dx/2] |= paletteid*16+color+5;
                        }
                        else {
                            dest[dx/2] &= 0x00ff;
                            dest[dx/2] |= (paletteid*16+color+5)<<8;
                        }
                    }
                    //dest[dx] = src[x]+8*4+paletteid*4;
                    dx += dir;
                }
            }
        }
    }
}

void updateBgPalette(int paletteid) {
    if (gbMode == GB) {
        for (int i=0; i<4; i++) {
            u8 id = (ioRam[0x47]>>(i*2))&3;
            BG_PALETTE[paletteid*16+i+1] = bgPaletteData[paletteid*8+id*2] | (bgPaletteData[paletteid*8+id*2+1]<<8) | BIT(15);
        }
    }
    else {
        for (int i=0; i<4; i++) {
            BG_PALETTE[paletteid*16+i+1] = bgPaletteData[paletteid*8+i*2] | (bgPaletteData[paletteid*8+i*2+1]<<8) | BIT(15);
        }
    }
}
void updateSprPalette(int paletteid) {
    if (gbMode == GB) {
        for (int i=0; i<4; i++) {
            u8 id = (ioRam[0x48+paletteid]>>(i*2))&3;
            BG_PALETTE[paletteid*16+i+5] = sprPaletteData[paletteid*8+id*2] | (sprPaletteData[paletteid*8+id*2+1]<<8) | BIT(15);
        }
    }
    else {
        for (int i=0; i<4; i++) {
            BG_PALETTE[paletteid*16+i+5] = sprPaletteData[paletteid*8+i*2] | (sprPaletteData[paletteid*8+i*2+1]<<8) | BIT(15);
        }
    }
}

void writeVram(u16 addr, u8 val) {
    vram[vramBank][addr] = val;

    if (addr >= 0x1800) {
        if (addr < 0x1c00)
            updateTileMap(0, addr-0x1800, -1);
        else
            updateTileMap(1, addr-0x1c00, -1);
    }
    else {
        int tileNum = addr/16;
        if (!tileModified[vramBank][tileNum]) {
            tileModified[vramBank][tileNum] = true;
            tileModifiedQueue[tileModifiedQueueLength++] = (vramBank<<9)|tileNum;
        }
    }
}
void writeVram16(u16 dest, u16 src) {
    for (int i=0; i<16; i++) {
        vram[vramBank][dest++] = readMemory(src++);
    }
    dest -= 16;
    src -= 16;
    if (dest >= 0x1800) {
        for (int i=dest; i<dest+16; i++) {
            if (dest < 0x1c00)
                updateTileMap(0, i-0x1800, -1);
            else
                updateTileMap(1, i-0x1c00, -1);
        }
    }
    else {
        int tileNum = dest/16;
        drawTile(vramBank, tileNum);
    }
}


void handleVideoRegister(u8 ioReg, u8 val) {
    switch(ioReg) {
        case 0x40:    // LCDC
            ioRam[0x40] = val;
            return;
        case 0x41:
            ioRam[0x41] &= 0x7;
            ioRam[0x41] |= val&0xF8;
            return;
        case 0x46:				// DMA
            {
                u16 src = val << 8;
                int i;
                for (i=0; i<0xA0; i++) {
                    u8 val = readMemory(src+i);
                    hram[i] = val;
                    if (ioRam[0x44] >= 144 || ioRam[0x44] <= 1)
                        spriteData[i] = val;
                }

                extraCycles += 50;
                //printLog("dma write %d\n", ioRam[0x44]);
                return;
            }
        case 0x42:
        case 0x43:
        case 0x4B:
            ioRam[ioReg] = val;
            return;
            // winY
        case 0x4A:
            ioRam[0x4a] = val;
            return;
        case 0x44:
            //ioRam[0x44] = 0;
            return;
        case 0x47:				// BG Palette (GB classic only)
            ioRam[0x47] = val;
            updateBgPalette(0);
            return;
        case 0x48:				// Spr Palette (GB classic only)
            ioRam[0x48] = val;
            updateSprPalette(0);
            return;
        case 0x49:				// Spr Palette (GB classic only)
            ioRam[0x49] = val;
            updateSprPalette(1);
            return;
        case 0x68:				// BG Palette Index (GBC only)
            ioRam[0x68] = val;
            return;
        case 0x69:				// BG Palette Data (GBC only)
            {
                int index = ioRam[0x68] & 0x3F;
                bgPaletteData[index] = val;
                if (index%8 == 7)
                    updateBgPalette(index/8);

                if (ioRam[0x68] & 0x80)
                    ioRam[0x68]++;
                return;
            }
        case 0x6B:				// Sprite Palette Data (GBC only)
            {
                int index = ioRam[0x6A] & 0x3F;
                sprPaletteData[index] = val;
                if (index%8 == 7)
                    updateSprPalette(index/8);

                if (ioRam[0x6A] & 0x80)
                    ioRam[0x6A]++;
                return;
            }
        default:
            ioRam[ioReg] = val;
    }
}


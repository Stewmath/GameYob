#include <nds.h>
#include <stdio.h>
#include <math.h>
#include <algorithm>
#include <set>
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
u8 mapImageBuf[2][256*256];
u8 mapImageShiftBuf[2][256*256+1];
u8* mapImage = (u8*)memUncached(mapImageBuf);
u8* mapImageShift = (u8*)memUncached(mapImageShiftBuf);

bool renderingBankA=true;

bool tileModified[2][32*32];

std::set<int> mapTiles[0x300];
std::set<int> usedSprites;

bool lastTileSigned[144];
bool rowDrawn[2][32];

// Frame counter. Incremented each vblank.
u8 frame=0;

// Whether to wait for vblank if emulation is running behind schedule
int interruptWaitMode=0;

int scaleMode=0;
int scaleFilter;

bool windowEnabled = true;
bool bgEnabled = true;
bool spritesEnabled = true;

bool hblankDisabled = false;

bool bgPaletteModified[8];

void drawTile(int tile);
void updateTiles();
void updateTileMap(int map, int i, u8 val);
void drawScanline(int scanline) ITCM_CODE;
void setScaleMode(int m);
void updateBgPalette(int paletteid, u8* data);
void drawSprites();

struct ScanlineStruct {
    bool modified;
    u8 bgPalettes[0x40];
    bool bgPaletteModified[8];
};

ScanlineStruct scanlineState[144];

void hblankHandler()
{
    int gbLine = REG_VCOUNT+1-screenOffsY;
    if (gbLine >= 0 && gbLine < 144) {
        ScanlineStruct* state = &scanlineState[gbLine];
        if (state->modified || (gbLine != 0 && scanlineState[gbLine-1].modified)) {
            for (int i=0; i<8; i++) {
                if (state->bgPaletteModified[i])
                    updateBgPalette(i, state->bgPalettes);
                else if (gbLine != 0 && scanlineState[gbLine-1].bgPaletteModified[i])
                    updateBgPalette(i, scanlineState[gbLine-1].bgPalettes);
            }
        }
    }
}

void vblankHandler()
{
    frame++;
    static bool vShift=true;

    if (scaleMode != 0) {
        if (vShift && scaleFilter != 0) {
            if (scaleFilter == 1) {
                REG_BG2Y = 1<<6;
                REG_BG3Y = 1<<6;
            }
            else {
                REG_BG2Y = 1<<7;
                REG_BG3Y = 1<<7;
            }
        }
        else {
            REG_BG2Y = 0;
            REG_BG3Y = 0;
        }
        vShift = !vShift;
    }
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

    videoSetMode(MODE_5_2D | DISPLAY_BG2_ACTIVE);

    REG_BG2CNT = BG_BMP8_256x256;
    REG_BG3CNT = BG_BMP8_256x256;

    WIN_OUT = 0;

    REG_DISPSTAT &= 0xFF;
    REG_DISPSTAT |= (144+screenOffsY)<<8;

    REG_BLDCNT = 1<<2 | 1<<11 | 1<<6;
    REG_BLDALPHA = 8 | 8<<8;

    irqEnable(IRQ_VCOUNT);
    irqEnable(IRQ_HBLANK);
    irqEnable(IRQ_VBLANK);
    irqSet(IRQ_VBLANK, &vblankHandler);
    irqSet(IRQ_HBLANK, &hblankHandler);

    setScaleFilter(scaleFilter);

    refreshGFX();
}

void refreshGFX() {
    for (int i=0; i<32*32; i++) {
        tileModified[0][i] = true;
        tileModified[1][i] = true;
    }
    for (int i=0; i<144; i++)
        lastTileSigned[i] = -1;
    for (int i=0; i<0x300; i++)
        mapTiles[i] = std::set<int>();
}

void updateTileMap(int m, int tile, int y) {
    int tileSigned = !(ioRam[0x40]&0x10);
    int bgMap = (m ? 0x1c00 : 0x1800);
    int addr = bgMap+tile;
    int tileNum;
    if (tileSigned)
        tileNum = 0x100+(s8)vram[0][addr];
    else
        tileNum = vram[0][addr];

    int paletteid=0, bank=0;
    int flipX=0, flipY=0;
    if (gbMode == CGB) {
        u8 flags = vram[1][addr];
        paletteid = flags&7;
        bank = (flags>>3)&1;
        flipX = flags&0x20;
        flipY = flags&0x40;
    }
    int offset = tile/32*8*256+(tile%32)*8;
    u8* dest = &mapImage[m*0x10000+offset];
    if (y == -1) {
        for (int y=0; y<8; y++) {
            u8* ind;
            if (flipY) {
                ind = &vram[bank][tileNum*0x10+(7-y)*2];
            }
            else {
                ind = &vram[bank][tileNum*0x10+y*2];
            }
            u8 b1=*(ind++);
            u8 b2=*(ind);
            int dir=1;
            if (flipX) {
                dir = -1;
                dest += 7;
            }
            for (int x=0; x<8; x++) {
                u8 color = (b1>>7)&1;
                b1 <<= 1;
                color |= (b2>>6)&2;
                b2 <<= 1;
                *dest = paletteid*16+color+1;
                dest += dir;
            }
            dest += 257;
            if (!flipX)
                dest -= 9;
            memcpy(&mapImageShift[m*0x10000+offset+y*256+1], &mapImage[m*0x10000+offset+y*256], 8);
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
    int BG2X, BG2Y, BG2PA,BG2PB,BG2PC,BG2PD;

    scaleMode = mode;

    switch(mode) {
        case 0:
            BG2PA = 1<<8;
            BG2PB = 0;
            BG2PC = 0;
            BG2PD = 1<<8;
            BG2X = -screenOffsX<<8;
            BG2Y = -screenOffsY<<8;
            break;
        case 1:
            BG2PA = (1<<8)/((double)192/144);
            BG2PB = 0;
            BG2PC = 0;
            BG2PD = (1<<8)/((double)192/144);
            BG2X = -(256/2-((((double)192)/144)*160)/2)*256;
            BG2Y = 0;
            break;
        case 2:
            BG2PA = (1<<8)/((double)255/160);
            BG2PB = 0;
            BG2PC = 0;
            BG2PD = (1<<8)/((double)192/144);
            BG2X = 0;
            BG2Y = 0;
            break;
        default:
            return;
    }

    REG_BG2X = BG2X;
    REG_BG2Y = BG2Y;
    REG_BG2PA = BG2PA;
    REG_BG2PB = BG2PB;
    REG_BG2PC = BG2PC;
    REG_BG2PD = BG2PD;

    if (scaleFilter == 1) {
        REG_BG3X = BG2X + (1<<6);
        REG_BG3Y = BG2Y;
    }
    else if (scaleFilter == 2) {
        REG_BG3X = BG2X + (1<<7);
        REG_BG3Y = BG2Y;
    }
    else {
        REG_BG3X = BG2X;
        REG_BG3Y = BG2Y;
    }
    REG_BG3PA = BG2PA;
    REG_BG3PB = BG2PB;
    REG_BG3PC = BG2PC;
    REG_BG3PD = BG2PD;
}
void setScaleFilter(int filter) {
    scaleFilter = filter;
    if (scaleFilter != 0)
        videoBgEnable(3);
    else
        videoBgDisable(3);
    setScaleMode(scaleMode);
}

void enableBackground(int enabled) {
    bgEnabled = enabled;
}
void enableSprites(int enabled) {
    spritesEnabled = enabled;
}
void enableWindow(int enabled) {
    windowEnabled = enabled;
}


void disableScreen() {
}
void enableScreen() {
}

void drawTile(int tileNum) {
    return;
    /*
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
    */
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

    if (!(fastForwardMode || fastForwardKey))
        swiIntrWait(interruptWaitMode, IRQ_VBLANK);
    if (renderingBankA) {
        REG_BG2CNT = BG_BMP8_256x256;
        REG_BG3CNT = BG_BMP8_256x256;
        pixels = ((u8*)BG_GFX)+0x10000;
        renderingBankA = false;
    }
    else {
        REG_BG2CNT = BG_BMP8_256x256 | BG_MAP_BASE(4);
        REG_BG3CNT = BG_BMP8_256x256 | BG_MAP_BASE(4);
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
    scanlineState[scanline].modified = false;
    for (int i=0; i<8; i++) {
        scanlineState[scanline].bgPaletteModified[i] = bgPaletteModified[i];
        if (bgPaletteModified[i]) {
            scanlineState[scanline].modified = true;
            bgPaletteModified[i] = false;
            for (int j=i*8; j<i*8+8; j++) {
                scanlineState[scanline].bgPalettes[j] = bgPaletteData[j];
            }
        }
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

    int realy = (scanline+vofs)%256;
    int ypix = realy%8;

    int tile = (realy/8)*32;
    if (tileSigned != lastTileSigned[scanline]) {
        for (int i=ypix; i<8; i++) {
            if (scanline+i-ypix < 144)
                lastTileSigned[scanline+i-ypix] = tileSigned;
        }
        for (int i=0; i<32; i++) {
            tileModified[0][tile] = false;
            updateTileMap(0, tile, -1);
            tileModified[1][tile] = false;
            updateTileMap(1, tile, -1);
            tile++;
        }
        rowDrawn[bgMap][realy/8] = true;
    }

    if (bgEnabled) {
        if (lcdc&1) {
            tile = realy/8*32;
            if (!rowDrawn[bgMap][realy/8]) {
                rowDrawn[bgMap][realy/8] = true;
                for (int i=0; i<32; i++) {
                    if (tileModified[bgMap][tile]) {
                        tileModified[bgMap][tile] = false;
                        updateTileMap(bgMap, tile, -1);
                    }
                    tile++;
                }
            }
            int end = 160;
            if (winOn && winY <= scanline)
                end = winX;
            if (end > 160)
                end = 160;
            if (end > 0) {
                int loopStart = 256-hofs;
                if (loopStart >= end) {
                    if (hofs%2 == 0)
                        dmaCopy(&mapImage[bgMap*0x10000+realy*256+hofs], pixels+scanline*256, end+1);
                    else
                        dmaCopy(&mapImageShift[bgMap*0x10000+realy*256+hofs+1], pixels+scanline*256, end+1);
                }
                else {
                    if (loopStart%2)
                        pixels[scanline*256] = mapImage[bgMap*0x10000+realy*256+hofs];
                    if (loopStart > 1) {
                        if (hofs%2 == 0)
                            dmaCopy(&mapImage[bgMap*0x10000+realy*256+hofs+loopStart%2], pixels+scanline*256, loopStart);
                        else
                            dmaCopy(&mapImageShift[bgMap*0x10000+realy*256+hofs+1+loopStart%2], pixels+scanline*256, loopStart);
                    }
                    if (loopStart%2 == 0)
                        dmaCopy(&mapImage[bgMap*0x10000+realy*256], pixels+scanline*256+loopStart, end-loopStart+1);
                    else {
                        dmaCopy(&mapImageShift[bgMap*0x10000+realy*256+1], pixels+scanline*256+loopStart, end-loopStart+1);
                    }
                }
            }
        }
        else {
            for (int i=0; i<160/2; i++)
                ((u16*)pixels)[scanline*128+i] = 0;
        }
    }

    if (windowEnabled && winOn && winY <= scanline && winX < 160) {
        tile = (scanline-winY)/8*32;
        if (!rowDrawn[winMap][(scanline-winY)/8]) {
            rowDrawn[bgMap][(scanline-winY)/8] = true;
            for (int i=0; i<32; i++) {
                if (tileModified[winMap][tile]) {
                    tileModified[winMap][tile] = false;
                    updateTileMap(winMap, tile, -1);
                }
                tile++;
            }
        }

        int len,dest;
        if (winX < 0) {
            len = 160;
            dest = scanline*256;
        }
        else {
            len=160-winX;
            dest=scanline*256+winX;
        }
        if (winX%2 == 0) {
            dmaCopy(&mapImage[winMap*0x10000+(scanline-winY)*256], pixels+dest, len);
        }
        else {
            pixels[scanline*256+winX] = mapImage[winMap*0x10000];
            if (len > 2)
                dmaCopy(&mapImage[winMap*0x10000+(scanline-winY)*256+1], pixels+dest+1, len);
        }
    }

    if (spritesOn && spritesEnabled && !usedSprites.empty()) {
        u16* dest = (u16*)(pixels+scanline*256);
        std::set<int>::iterator it=usedSprites.end();
        do {
            it--;
            int s = *it;

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
                for (int x=0; x<8; x++) {
                    u8 color = (b1>>7)&1;
                    b1 <<= 1;
                    color |= (b2>>6)&2;
                    b2 <<= 1;
                    if (color != 0 && dx >= 0 && dx < 160) {
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
        while (it != usedSprites.begin());
    }
}

void drawSprites() {
    int spritesOn = ioRam[0x40]&2;
    int tallSprites = ioRam[0x40]&4;
    if (spritesOn && spritesEnabled && !usedSprites.empty()) {
        std::set<int>::iterator it=usedSprites.end();
        do {
            it--;
            int s = *it;

            int index = s*4;
            int posY = hram[index]-16;
            if (posY > -16 && posY < 144) {
                u16* dest = (u16*)(pixels+posY*256);
                bool cond;
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
                int yPixels = (tallSprites?16:8);
                for (int y=0; y<yPixels; y++) {
                    if (posY+y >= 144)
                        break;
                    int dx = posX;
                    if (flipX) {
                        dir = -1;
                        dx += 7;
                    }
                    u8 b1,b2;
                    if (flipY) {
                        int ind = tileNum*0x10+((tallSprites?15:7)-y)*2;
                        b1 = vram[bank][ind];
                        b2 = vram[bank][ind+1];
                    }
                    else {
                        int ind = tileNum*0x10+y*2;
                        b1 = vram[bank][ind];
                        b2 = vram[bank][ind+1];
                    }
                    for (int x=0; x<8; x++) {
                        u8 color = (b1>>7)&1;
                        b1 <<= 1;
                        color |= (b2>>6)&2;
                        b2 <<= 1;
                        if (color != 0 && dx >= 0 && dx < 160) {
                            if (dx%2 == 0) {
                                dest[dx/2+y*128] &= 0xff00;
                                dest[dx/2+y*128] |= paletteid*16+color+5;
                            }
                            else {
                                dest[dx/2+y*128] &= 0x00ff;
                                dest[dx/2+y*128] |= (paletteid*16+color+5)<<8;
                            }
                        }
                        //dest[dx] = src[x]+8*4+paletteid*4;
                        dx += dir;
                    }
                }
            }
        }
        while (it != usedSprites.begin());
    }
}

void updateBgPalette(int paletteid, u8* data) {
    if (gbMode == GB) {
        for (int i=0; i<4; i++) {
            u8 id = (ioRam[0x47]>>(i*2))&3;
            BG_PALETTE[paletteid*16+i+1] = data[paletteid*8+id*2] | (data[paletteid*8+id*2+1]<<8) | BIT(15);
        }
    }
    else {
        for (int i=0; i<4; i++) {
            BG_PALETTE[paletteid*16+i+1] = data[paletteid*8+i*2] | (data[paletteid*8+i*2+1]<<8) | BIT(15);
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
    u8 old = vram[vramBank][addr];
    if (old == val)
        return;
    vram[vramBank][addr] = val;

    if (addr >= 0x1800) {
        int map = (addr-0x1800)/0x400;
        int tile = addr-0x1800;

        tileModified[map][tile&0x3ff] = true;
        rowDrawn[map][(tile&0x3ff)/32] = false;
        int oldVal = old+(vramBank?0x180:0);
        std::set<int>::iterator it = mapTiles[oldVal].find(tile);
        if (it != mapTiles[oldVal].end())
            mapTiles[oldVal].erase(it);
        oldVal = (s8)old+0x100+(vramBank?0x180:0);
        it = mapTiles[oldVal].find(tile);
        if (it != mapTiles[oldVal].end())
            mapTiles[oldVal].erase(it);

        int newVal;
        if (ioRam[0x40]&0x10)
            newVal = val+(vramBank?0x180:0);
        else
            newVal = (s8)val+0x100+(vramBank?0x180:0);
        mapTiles[newVal].insert(tile);
    }
    else {
        int tile = addr/16;
        for (std::set<int>::iterator it=mapTiles[tile].begin(); it!=mapTiles[tile].end(); it++) {
            int map = (*it)/0x400;
            tileModified[map][*it&0x3ff] = true;
            rowDrawn[map][(*it&0x3ff)/32] = false;
        }
    }
}
void writeVram16(u16 dest, u16 src) {
    if (dest >= 0x1800) {
        for (int i=dest; i<dest+16; i++) {
            writeVram(i, readMemory(src++));
        }
    }
    else {
        bool changed=false;
        for (int i=0; i<16; i++) {
            u8 val = readMemory(src++);
            if (vram[vramBank][dest] != val) {
                changed = true;
                vram[vramBank][dest] = val;
            }
            dest++;
        }
        if (!changed)
            return;
        dest -= 16;
        src -= 16;

        int tile = dest/16;
        for (std::set<int>::iterator it=mapTiles[tile].begin(); it!=mapTiles[tile].end(); it++) {
            int map = *it/0x400;
            tileModified[map][*it&0x3ff] = true;
            rowDrawn[map][(*it&0x3ff)/32] = false;
        }
    }
}

void writeHram(u16 addr, u8 val) {
    hram[addr] = val;

    int sprAddr = addr-addr%4;
    if (hram[sprAddr] != 0 && hram[sprAddr] < 144+16 && hram[sprAddr+1] != 0 && hram[sprAddr+1] < 160+8)
        usedSprites.insert(sprAddr/4);
    else {
        std::set<int>::iterator it = find(usedSprites.begin(), usedSprites.end(), sprAddr/4);
        if (it != usedSprites.end())
            usedSprites.erase(it);
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
                usedSprites = std::set<int>();
                std::set<int>::iterator endIt = usedSprites.begin();
                u16 src = val << 8;
                u8 dest = 0;
                for (int i=0; i<40; i++) {
                    for (int j=0; j<4; j++) {
                        u8 val = readMemory(src++);
                        hram[dest++] = val;
                    }
                    int sprAddr = dest-4;
                    if (hram[sprAddr] != 0 && hram[sprAddr] < 144+16 && hram[sprAddr+1] != 0 && hram[sprAddr+1] < 160+8)
                        endIt = usedSprites.insert(endIt, sprAddr/4);
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
            updateBgPalette(0, bgPaletteData);
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
                bgPaletteModified[index/8] = true;

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


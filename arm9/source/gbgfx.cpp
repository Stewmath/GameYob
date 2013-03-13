#include <nds.h>
#include <stdio.h>
#include <math.h>
#include "gbgfx.h"
#include "mmu.h"
#include "gbcpu.h"
#include "main.h"
#include "gameboy.h"

const int spr_priority = 2;
const int spr_priority_low = 3;

const int map_base[] = {28, 29};
const int blank_map_base[] = {30, 31};
const int overlay_map_base[] = {26,27};
const int off_map_base = 25;

const int win_blank_priority = 3;
const int win_priority = 2;

const int bg_blank_priority = 3;
const int bg_priority = 2;

const int bg_overlay_priority = 1;
const int win_overlay_priority = 1;

u16* map[2];
u16* blankMap[2];
u16* overlayMap[2];
u16* offMap;

u16 mapBuf[2][0x400];
u16 blankMapBuf[2][0x400];
u16 overlayMapBuf[2][0x400];

int screenOffsX = 48;
int screenOffsY = 24;

int colors[4];
int tileSize;
u16 pixels[256*144];

int tileSigned = 0;
int tileAddr = 0x8000;
int BGMapAddr = 0x9800;
int winMapAddr = 0x9800;
int BGOn = 1;
int winOn = 0;

int frameSkip = 2;
int frameSkipCnt;

// Frame counter. Incremented each vblank.
u8 frame=0;

bool changedTile[2][0x180];
int changedTileQueueLength=0;
u16 changedTileQueue[0x300];

bool changedTileInFrame[2][0x180];
int changedTileInFrameQueueLength=0;
u16 changedTileInFrameQueue[0x300];

bool bgPaletteModified[8];
bool spritePaletteModified[8];

u8 bgPaletteData[0x40];
u8 sprPaletteData[0x40];

int winPosY=0;

bool lineModified = false;

// Whether to wait for vblank if emulation is running behind schedule
int interruptWaitMode=0;
bool windowDisabled = false;
bool hblankDisabled = false;

int dmaLine;


void drawSprites();
void drawTile(int tile, int bank);
void updateTiles();
void updateTileMap(int map, int i, u8 val);

typedef struct {
    bool modified;
    u8 hofs;
    u8 vofs;
    u8 winX;
    u8 winY;
    u8 winPosY;
    u8 winOn;
    u16 bgBlankCnt;
    u16 bgCnt;
    u16 bgOverlayCnt;
    u16 winBlankCnt;
    u16 winCnt;
    u16 winOverlayCnt;
    bool spritesOn;
    bool map0;
    bool tileSigned;
    int spriteMaps[40];
} ScanlineStruct;

ScanlineStruct scanlineBuffers[2][144];

ScanlineStruct *drawingState = scanlineBuffers[0];
ScanlineStruct *renderingState = scanlineBuffers[1];

typedef struct
{
    u16 attr0;
    u16 attr1;
    u16 attr2;
    u16 affine_data;
} spriteEntry;

#define sprites ((spriteEntry*)OAM)
#define spriteEntries ((SpriteEntry*)OAM);

// The graphics are drawn with the DS's native hardware.
// Games tend to modify the graphics in the middle of being drawn.
// These changes are recorded and applied during DS hblank.

inline void drawLine(int gbLine) {
    ScanlineStruct *state = &drawingState[gbLine];

    if (state->spritesOn)
        REG_DISPCNT |= DISPLAY_SPR_ACTIVE;
    else
        REG_DISPCNT &= ~DISPLAY_SPR_ACTIVE;

    int hofs = state->hofs-screenOffsX;
    int vofs = state->vofs-screenOffsY;

    REG_BG3HOFS = hofs;
    REG_BG3VOFS = vofs;
    REG_BG3CNT = state->bgCnt;

    REG_BG2CNT = state->bgBlankCnt;
    REG_BG2HOFS = hofs;
    REG_BG2VOFS = vofs;

    if (!state->winOn || windowDisabled) {
        REG_DISPCNT &= ~DISPLAY_WIN0_ON;
        WIN_IN = (WIN_IN | (1<<8)) & ~4; // Set bit 8, unset bit 2

        REG_BG0CNT = state->bgOverlayCnt;
        REG_BG0HOFS = hofs;
        REG_BG0VOFS = vofs;
    }
    else {
        REG_DISPCNT |= DISPLAY_WIN0_ON;
        WIN_IN &= ~(1<<8);

        int winX = state->winX;

        if (winX <= 7)
            WIN0_X0 = screenOffsX;
        else
            WIN0_X0 = winX-7+screenOffsX;
        WIN0_Y0 = state->winY+screenOffsY;

        int whofs = -(winX-7)-screenOffsX;
        int wvofs = -(gbLine-state->winPosY)-screenOffsY;

        REG_BG0HOFS = whofs;
        REG_BG0VOFS = wvofs;
        REG_BG0CNT = state->winBlankCnt;

        REG_BG1HOFS = whofs;
        REG_BG1VOFS = wvofs;
        REG_BG1CNT = state->winCnt;

        // If window fills whole scanline, give it one of the background layers 
        // for "tile priority" (overlay).
        if (winX <= 7) {
            WIN_IN |= 4;

            REG_BG2HOFS = whofs;
            REG_BG2VOFS = wvofs;
            REG_BG2CNT = state->winOverlayCnt;
        }
    }
}

void hblankHandler() ITCM_CODE;

void hblankHandler()
{
    int line = REG_VCOUNT+1;
    int gbLine = line-screenOffsY;
    if (!(gbLine >= 0 && gbLine < 144))
        return;

    if (!drawingState[gbLine].modified)
        return;

    drawLine(gbLine);
}

void vblankHandler()
{
    frame++;
}

void initGFX()
{
    tileSize = 8;

    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankB(VRAM_B_MAIN_BG);
    vramSetBankE(VRAM_E_MAIN_SPRITE);


    // Backdrop
    BG_PALETTE[0] = RGB8(0,0,0);

    map[0] = BG_MAP_RAM(map_base[0]);
    map[1] = BG_MAP_RAM(map_base[1]);
    blankMap[0] = BG_MAP_RAM(blank_map_base[0]);
    blankMap[1] = BG_MAP_RAM(blank_map_base[1]);
    overlayMap[0] = BG_MAP_RAM(overlay_map_base[0]);
    overlayMap[1] = BG_MAP_RAM(overlay_map_base[1]);
    offMap = BG_MAP_RAM(off_map_base);

    // Tile for "Blank maps", which contain only the gameboy's color 0.
    for (int i=0; i<16; i++) {
        BG_GFX[i] = 1 | (1<<4) | (1<<8) | (1<<12);
    }

    // Initialize the "off map", for when the gameboy screen is disabled.
    // Uses the "Blank map" tile because, why not.
    for (int i=0; i<32*32; i++) {
        offMap[i] = 8<<12;
    }
    // Off map palette
    BG_PALETTE[8*16+1] = RGB8(255,255,255);

    int i=0;
    for (i=40; i<128; i++)
        sprites[i].attr0 = ATTR0_DISABLED;

    colors[0] = 255;
    colors[1] = 192;
    colors[2] =  94;
    colors[3] = 0;


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

    WIN_IN = (0x7) | (1<<4) | (0xc<<8) | (1<<12); // enable backgrounds and sprites
    WIN_OUT = 0;
    WIN1_X0 = screenOffsX;
    WIN1_X1 = screenOffsX+160;
    WIN1_Y0 = screenOffsY;
    WIN1_Y1 = screenOffsY+144;
    WIN0_X0 = screenOffsX;
    WIN0_Y0 = screenOffsY;
    WIN0_X1 = screenOffsX+160;
    WIN0_Y1 = screenOffsY+144;

    videoSetMode(MODE_0_2D | DISPLAY_BG0_ACTIVE | DISPLAY_BG1_ACTIVE | DISPLAY_BG2_ACTIVE | DISPLAY_BG3_ACTIVE | DISPLAY_WIN0_ON | DISPLAY_WIN1_ON | DISPLAY_SPR_ACTIVE | DISPLAY_SPR_1D);

    REG_DISPSTAT &= 0xFF;
    REG_DISPSTAT |= (144+screenOffsY)<<8;

    irqEnable(IRQ_VCOUNT);
    irqEnable(IRQ_HBLANK);
    irqEnable(IRQ_VBLANK);
    irqSet(IRQ_VBLANK, &vblankHandler);
    irqSet(IRQ_HBLANK, &hblankHandler);

    for (i=0; i<0x180; i++)
    {
        drawTile(i, 0);
        drawTile(i, 1);
    }
}

void disableScreen() {
    videoBgDisable(0);
    videoBgDisable(1);
    REG_BG2CNT = BG_MAP_BASE(off_map_base);
    videoBgDisable(3);
    REG_DISPCNT &= ~(DISPLAY_SPR_ACTIVE | DISPLAY_WIN0_ON);
    irqClear(IRQ_HBLANK);
}
void enableScreen() {
    videoBgEnable(0);
    videoBgEnable(1);
    videoBgEnable(2);
    videoBgEnable(3);
    REG_DISPCNT |= DISPLAY_SPR_ACTIVE;
    if (!hblankDisabled) {
        irqEnable(IRQ_HBLANK);
    }
    irqSet(IRQ_HBLANK, hblankHandler);
}

void updateTileMap(int m, int i, u8 val) {
    int mapAddr = (m ? 0x1c00+i : 0x1800+i);
    int tileNum = vram[0][mapAddr];

    int bank=0;
    int flipX = 0, flipY = 0;
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
    if (priority)
        overlayMapBuf[m][i] = (tileNum+(bank*0x100)) | (paletteid<<12) | (flipX<<10) | (flipY<<11);
    else {
        overlayMapBuf[m][i] = 0x300;
    }
    mapBuf[m][i] = (tileNum+(bank*0x100)) | (paletteid<<12) | (flipX<<10) | (flipY<<11);
    blankMapBuf[m][i] = paletteid<<12;
}

void drawTile(int tileNum, int bank) {
    int x,y;
    int index = (tileNum<<4)+(bank*0x100*16);
    int signedIndex;
    if (tileNum < 0x100)
        signedIndex = (tileNum<<4)+(bank*0x100*16);
    else
        signedIndex = ((tileNum-0x100)<<4)+(bank*0x100*16);

    bool unsign = tileNum < 0x100;
    bool sign = tileNum >= 0x80;
    for (y=0; y<8; y++) {
        u8 b1=vram[bank][(tileNum<<4)+(y<<1)];
        u8 b2=vram[bank][(tileNum<<4)+(y<<1)+1];
        int bb[] = {0,0};
        int sb[] = {0,0};
        for (x=0; x<8; x++) {
            int colorid;
            colorid = !!(b1&0x80);
            colorid |= !!(b2&0x80)<<1;
            b1 <<= 1;
            b2 <<= 1;

            if (colorid != 0)
                bb[x/4] |= ((colorid+1)<<((x%4)*4));
            if (unsign)
                sb[x/4] |= (colorid<<((x%4)*4));
        }
        if (unsign) {
            BG_GFX[0x8000+index] = bb[0];
            BG_GFX[0x8000+index+1] = bb[1];
            SPRITE_GFX[index] = sb[0];
            SPRITE_GFX[index+1] = sb[1];
        }
        if (sign) {
            BG_GFX[0x10000+signedIndex] = bb[0];
            BG_GFX[0x10000+signedIndex+1] = bb[1];
        }
        index += 2;
        signedIndex += 2;
    }
}

// Currently not actually used
void copyTile(u8 *src,u16 *dest) {
    for (int y=0; y<8; y++) {
        dest[y*2] = 0;
        dest[y*2+1] = 0;
        int index = y*2;
        for (int x=0; x<8; x++) {
            int colorid;
            colorid = !!(src[index] & (0x80>>x));
            colorid |= !!(src[index+1] & (0x80>>x))<<1;
            int orValue = (colorid<<((x%4)*4));
            dest[y*2+x/4] |= orValue;
        }
    }
}

void drawScreen()
{
    DC_FlushRange(mapBuf[0], 0x400*2);
    DC_FlushRange(mapBuf[1], 0x400*2);
    DC_FlushRange(blankMapBuf[0], 0x400*2);
    DC_FlushRange(blankMapBuf[1], 0x400*2);
    DC_FlushRange(overlayMapBuf[0], 0x400*2);
    DC_FlushRange(overlayMapBuf[1], 0x400*2);

    swiIntrWait(interruptWaitMode, IRQ_VBLANK);

    dmaCopy(mapBuf[0], map[0], 0x400*2);
    dmaCopy(mapBuf[1], map[1], 0x400*2);
    dmaCopy(blankMapBuf[0], blankMap[0], 0x400*2);
    dmaCopy(blankMapBuf[1], blankMap[1], 0x400*2);
    dmaCopy(overlayMapBuf[0], overlayMap[0], 0x400*2);
    dmaCopy(overlayMapBuf[1], overlayMap[1], 0x400*2);

    int currentFrame = frame;

    ScanlineStruct* tmp = renderingState;
    renderingState = drawingState;
    drawingState = tmp;

    winPosY = -1;

    /*
       if (drawingState[0].spritesOn)
       REG_DISPCNT |= DISPLAY_SPR_ACTIVE;
       */

    // Check we're actually done drawing the screen.
    if (REG_VCOUNT >= screenOffsY+144) {
        REG_DISPCNT |= DISPLAY_SPR_ACTIVE;
    }

    // Palettes
    for (int paletteid=0; paletteid<8; paletteid++) {
        if (spritePaletteModified[paletteid]) {
            spritePaletteModified[paletteid] = false;
            for (int i=0; i<4; i++) {
                int id;
                if (gbMode == GB)
                    id = (ioRam[0x48+paletteid]>>(i*2))&3;
                else
                    id = i;

                SPRITE_PALETTE[((paletteid)*16)+i] = sprPaletteData[(paletteid*8)+(id*2)] | sprPaletteData[(paletteid*8)+(id*2)+1]<<8;
            }
        }
        if (bgPaletteModified[paletteid]) {
            bgPaletteModified[paletteid] = false;
            for (int i=0; i<4; i++) {
                int id;
                if (gbMode == GB)
                    id = (ioRam[0x47]>>(i*2))&3;
                else
                    id = i;

				BG_PALETTE[((paletteid)*16)+i+1] = bgPaletteData[(paletteid*8)+(id*2)] | bgPaletteData[(paletteid*8)+(id*2)+1]<<8;
            }
        }
    }

    updateTiles();
    drawSprites();

    if (interruptWaitMode == 1 && !(currentFrame+1 == frame && REG_VCOUNT >= 192) && (currentFrame != frame || REG_VCOUNT < 144+screenOffsY))
        printLog("badv %d-%d, %d\n", currentFrame, frame, REG_VCOUNT);
}

void drawSprites() {
    for (int i=0; i<40; i++)
    {
        int spriteNum = i*4;
        if (spriteData[spriteNum] == 0)
            sprites[i].attr0 = ATTR0_DISABLED;
        else
        {
            int y = spriteData[spriteNum]-16;
            int tall = !!(ioRam[0x40]&0x4);
            int tileNum = spriteData[spriteNum+2];
            if (tall)
                tileNum &= ~1;
            int x = (spriteData[spriteNum+1]-8)&0x1FF;
            int bank = 0;
            int flipX = !!(spriteData[spriteNum+3] & 0x20);
            int flipY = !!(spriteData[spriteNum+3] & 0x40);
            int priority = !!(spriteData[spriteNum+3] & 0x80);
            int paletteid;

            if (gbMode == CGB)
            {
                bank = !!(spriteData[spriteNum+3]&0x8);
                paletteid = spriteData[spriteNum+3] & 0x7;
            }
            else
            {
                paletteid = !!(spriteData[spriteNum+3] & 0x10);
            }

            int priorityVal = (priority ? spr_priority_low : spr_priority);
            sprites[i].attr0 = (y+screenOffsY) | (tall<<15);
            sprites[i].attr1 = (x+screenOffsX) | (flipX<<12) | (flipY<<13);
            sprites[i].attr2 = (tileNum+(bank*0x100)) | (priorityVal<<10) | (paletteid<<12);
        }
    }

    for (int i=0; i<0xa0; i++)
        spriteData[i] = hram[i];
}

void drawScanline(int scanline) ITCM_CODE;

void drawScanline(int scanline)
{
    if (hblankDisabled || scanline >= 144)
        return;
    int winX = ioRam[0x4b];
    if (winPosY == -2)
        winPosY = ioRam[0x44]-ioRam[0x4a];
    else if (winX < 167 && ioRam[0x4a] <= scanline)
        winPosY++;
    if (scanline == 0 || (scanline == 1 || (renderingState[scanline-1].modified && !renderingState[scanline-2].modified)) || scanline == ioRam[0x4a])
        lineModified = true;
    if (!lineModified) {
        renderingState[scanline].modified = false;
        return;
    } 
    bool winOn = (ioRam[0x40] & 0x20) && winX < 167 && ioRam[0x4a] < 144 && ioRam[0x4a] <= scanline;
    renderingState[scanline].modified = true;
    lineModified = false;
    renderingState[scanline].hofs = ioRam[0x43];
    renderingState[scanline].vofs = ioRam[0x42];
    renderingState[scanline].winX = winX;
    renderingState[scanline].winPosY = winPosY;
    renderingState[scanline].winY = ioRam[0x4a];

    if (ioRam[0x40] & 0x10)
        tileSigned = 0;
    else
        tileSigned = 1;
    if (ioRam[0x40] & 0x40)
        winMapAddr = 1;
    else
        winMapAddr = 0;
    if (ioRam[0x40] & 0x8)
        BGMapAddr = 1;
    else
        BGMapAddr = 0;

    renderingState[scanline].winOn = winOn;
    renderingState[scanline].spritesOn = ioRam[0x40] & 0x2;

    int winMapBase = map_base[winMapAddr];
    int bgMapBase = map_base[BGMapAddr];

    renderingState[scanline].map0 = !BGMapAddr;
    renderingState[scanline].tileSigned = tileSigned;

    if (tileSigned) {
        renderingState[scanline].winBlankCnt = (BG_MAP_BASE(blank_map_base[winMapAddr]) | BG_TILE_BASE(0) | win_blank_priority);
        renderingState[scanline].winCnt = (BG_MAP_BASE(winMapBase) | BG_TILE_BASE(8) | win_priority);
        renderingState[scanline].winOverlayCnt = (BG_MAP_BASE(overlay_map_base[winMapAddr]) | BG_TILE_BASE(8) | win_overlay_priority);

        renderingState[scanline].bgBlankCnt = (BG_MAP_BASE(blank_map_base[BGMapAddr]) | BG_TILE_BASE(0) | bg_blank_priority);
        renderingState[scanline].bgCnt = (BG_MAP_BASE(bgMapBase) | BG_TILE_BASE(8) | bg_priority);
        renderingState[scanline].bgOverlayCnt = (BG_MAP_BASE(overlay_map_base[BGMapAddr]) | BG_TILE_BASE(8) | bg_overlay_priority);
    }
    else {
        renderingState[scanline].winBlankCnt = (BG_MAP_BASE(blank_map_base[winMapAddr]) | BG_TILE_BASE(0) | win_blank_priority);
        renderingState[scanline].winCnt = (BG_MAP_BASE(winMapBase) | BG_TILE_BASE(4) | win_priority);
        renderingState[scanline].winOverlayCnt = (BG_MAP_BASE(overlay_map_base[winMapAddr]) | BG_TILE_BASE(4) | win_overlay_priority);

        renderingState[scanline].bgBlankCnt = (BG_MAP_BASE(blank_map_base[BGMapAddr]) | BG_TILE_BASE(0) | bg_blank_priority);
        renderingState[scanline].bgCnt = (BG_MAP_BASE(bgMapBase) | BG_TILE_BASE(4) | bg_priority);
        renderingState[scanline].bgOverlayCnt = (BG_MAP_BASE(overlay_map_base[BGMapAddr]) | BG_TILE_BASE(4) | bg_overlay_priority);
    }

    return;
}

void writeVram(u16 addr, u8 val) {
    vram[vramBank][addr] = val;

    if (addr < 0x1800) {
        int tileNum = addr/16;
        int scanline = ioRam[0x44];
        if (scanline >= 128 && scanline < 144) {
            if (!changedTileInFrame[vramBank][tileNum]) {
                changedTileInFrame[vramBank][tileNum] = true;
                changedTileInFrameQueue[changedTileInFrameQueueLength++] = tileNum|(vramBank<<9);
            }
        }
        else {
            if (!changedTile[vramBank][tileNum]) {
                changedTile[vramBank][tileNum] = true;
                changedTileQueue[changedTileQueueLength++] = tileNum|(vramBank<<9);
            }
        }
    }
    else {
        int map = (addr-0x1800)/0x400;
        if (map)
            updateTileMap(map, addr-0x1c00, val);
        else
            updateTileMap(map, addr-0x1800, val);
    }
}
void writeVram16(u16 dest, u16 src) {
    for (int i=0; i<16; i++) {
        vram[vramBank][dest++] = readMemory(src++);
    }
    dest -= 16;
    src -= 16;
    if (dest < 0x1800) {
        int tileNum = dest/16;
        if (ioRam[0x44] < 144) {
            if (!changedTileInFrame[vramBank][tileNum]) {
                changedTileInFrame[vramBank][tileNum] = true;
                changedTileInFrameQueue[changedTileInFrameQueueLength++] = tileNum|(vramBank<<9);
            }
        }
        else {
            if (!changedTile[vramBank][tileNum]) {
                changedTile[vramBank][tileNum] = true;
                changedTileQueue[changedTileQueueLength++] = tileNum|(vramBank<<9);
            }
        }
    }
    else {
        for (int i=0; i<16; i++) {
            int addr = dest+i;
            int map = (addr-0x1800)/0x400;
            if (map)
                updateTileMap(map, addr-0x1c00, vram[vramBank][src+i]);
            else
                updateTileMap(map, addr-0x1800, vram[vramBank][src+i]);
        }
    }
}


void updateTiles() {
    while (changedTileQueueLength > 0) {
        int val = changedTileQueue[--changedTileQueueLength];
        int bank = val>>9,tile=val&0x1ff;
        drawTile(tile, bank);
        changedTile[bank][tile] = false;
    }
    // copy "changedTileInFrame" to "changedTile", where they'll be applied next 
    // frame.
    while (changedTileInFrameQueueLength > 0) {
        int val = changedTileInFrameQueue[--changedTileInFrameQueueLength];
        int bank = val>>9, tile = val&0x1ff;
        changedTileInFrame[bank][tile] = false;
        changedTile[bank][tile] = true;
        changedTileQueue[changedTileQueueLength++] = val;
    }
}

void handleVideoRegister(u8 ioReg, u8 val) {
    switch(ioReg) {
        case 0x40:    // LCDC
            if ((val & 0x7F) != (ioRam[0x40] & 0x7F))
                lineModified = true;
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

                totalCycles += 50;
                dmaLine = ioRam[0x44];
                //printLog("dma write %d\n", ioRam[0x44]);
                return;
            }
        case 0x42:
        case 0x43:
        case 0x4B:
            {
                if (val != ioRam[ioReg]) {
                    ioRam[ioReg] = val;
                    lineModified = true;
                }
            }
            break;
            // winY
        case 0x4A:
            if (ioRam[0x44] >= 144 || val > ioRam[0x44])
                winPosY = -1;
            else {
                // Signal that winPosY must be reset according to winY
                winPosY = -2;
            }
            lineModified = true;
            ioRam[0x4a] = val;
            break;
        case 0x44:
            //ioRam[0x44] = 0;
            return;
        case 0x47:				// BG Palette (GB classic only)
            ioRam[0x47] = val;
            if (gbMode == GB)
            {
                bgPaletteModified[0] = true;
            }
            return;
        case 0x48:				// Spr Palette (GB classic only)
            ioRam[0x48] = val;
            if (gbMode == GB)
            {
                spritePaletteModified[0] = true;
            }
            return;
        case 0x49:				// Spr Palette (GB classic only)
            ioRam[0x49] = val;
            if (gbMode == GB)
            {
                spritePaletteModified[1] = true;
            }
            return;
        case 0x69:				// BG Palette Data (GBC only)
            {
                int index = ioRam[0x68] & 0x3F;
                bgPaletteData[index] = val;
                if (index%8 == 7)
                    bgPaletteModified[index/8] = true;

                if (ioRam[0x68] & 0x80)
                    ioRam[0x68]++;
                ioRam[0x69] = bgPaletteData[ioRam[0x68]&0x3F];
                return;
            }
        case 0x6B:				// Sprite Palette Data (GBC only)
            {
                int index = ioRam[0x6A] & 0x3F;
                sprPaletteData[index] = val;
                if (index%8 == 7)
                    spritePaletteModified[index/8] = true;

                if (ioRam[0x6A] & 0x80)
                    ioRam[0x6A]++;
                ioRam[0x6B] = sprPaletteData[ioRam[0x6A]&0x3F];
                return;
            }
        default:
            ioRam[ioReg] = val;
    }
}

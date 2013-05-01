#include <nds.h>
#include <stdio.h>
#include <math.h>
#include "gbgfx.h"
#include "mmu.h"
#include "gbcpu.h"
#include "main.h"
#include "gameboy.h"
#include "sgb.h"
#include "console.h"
#include "common.h"

enum {
    BORDER_NONE=0,
    BORDER_SGB,
    BORDER_CUSTOM
};

const int spr_priority = 2;
const int spr_priority_low = 3;

const int map_base[] = {1, 2};
const int color0_map_base[] = {3, 4};
const int overlay_map_base[] = {5, 6};
const int off_map_base = 7;
const int border_map_base = 8;

const int win_color0_priority = 3;
const int win_priority = 2;

const int bg_color0_priority = 3;
const int bg_priority = 2;

const int bg_overlay_priority = 1;
const int win_overlay_priority = 1;

const int win_all_priority = 2;

u16* map[2];
u16* color0Map[2];
u16* overlayMap[2];
u16* offMap;
u16* borderMap;

u16 mapBuf[2][0x400];
u16 color0MapBuf[2][0x400];
u16 overlayMapBuf[2][0x400];

int screenOffsX = 48;
int screenOffsY = 24;

int tileSize;

// Frame counter. Incremented each vblank.
u16 frame=0;

bool changedTile[2][0x180];
int changedTileQueueLength=0;
u16 changedTileQueue[0x300];

bool changedTileInFrame[2][0x180];
int changedTileInFrameQueueLength=0;
u16 changedTileInFrameQueue[0x300];

bool bgPaletteModified[8];
bool spritePaletteModified[8];
bool bgPalettesModified;
bool sprPalettesModified;
bool spritesModified;

u8 bgPaletteData[0x40];
u8 sprPaletteData[0x40];

int winPosY=0;

bool lineModified = false;

// Whether to wait for vblank if emulation is running behind schedule
int interruptWaitMode=0;
bool windowDisabled = false;
bool hblankDisabled = false;

int gfxMask;
int loadedBorderType;

int scaleMode;
int scaleFilter=1;
int SCALE_BGX, SCALE_BGY;

bool lastScreenDisabled;
bool screenDisabled;

void drawSprites(u8* data, int tallSprites);
void drawTile(int tile, int bank);
void updateTiles();
void updateTileMap(int map, int i, u8 val);
void updateBgPalette(int paletteid, u8* data, u8 dmgPal);
void updateSprPalette(int paletteid, u8* data, u8 dmgPal);

bool lineCompleted[144];
typedef struct {
    bool modified;
    bool mapsModified;
    u8 hofs;
    u8 vofs;
    u8 winX;
    u8 winY;
    u8 winPosY;
    u8 winOn;
    u16 bgColor0Cnt;
    u16 bgCnt;
    u16 bgOverlayCnt;
    u16 winColor0Cnt;   // Contains color 0 only
    u16 winCnt;         // Contains colors 1-3
    u16 winOverlayCnt;  // Contains colors 1-3, only for 'priority' tiles
    u16 winAllCnt;      // Contains colors 0-3. Used when available layers are limited.
    u8 bgMap,winMap;
    bool spritesOn;
    bool tileSigned;
    u8 bgPaletteData[0x40];
    u8 sprPaletteData[0x40];
    u8 bgPal;
    u8 sprPal[2];
    bool bgPalettesModified;
    bool sprPalettesModified;
    bool spritesModified;
    u8 spriteData[0xa0];
    int tallSprites;
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

#define BG_CNT(x) (*((vu16*)0x4000008+(x)))
#define BG_HOFS(x) (*((vu16*)0x4000010+(x)*2))
#define BG_VOFS(x) (*((vu16*)0x4000012+(x)*2))

// The graphics are drawn with the DS's native hardware.
// Games tend to modify the graphics in the middle of being drawn.
// These changes are recorded and applied during DS hblank.

inline void drawLine(int gbLine) {
    ScanlineStruct *state = &drawingState[gbLine];

    if (screenDisabled) {
        REG_DISPCNT &= ~DISPLAY_SPR_ACTIVE;
        REG_BG0CNT = BG_MAP_BASE(off_map_base);
        WIN_IN |= 7<<8;
        return;
    }
    if (state->mapsModified) {
        if (state->spritesOn)
            REG_DISPCNT |= DISPLAY_SPR_ACTIVE;
        else
            REG_DISPCNT &= ~DISPLAY_SPR_ACTIVE;

        bool useWin = state->winOn && !windowDisabled;
        int layer = 0;
        int bgLayers,winLayers;
        if (useWin) {
            if (state->winX > 7) {
                winLayers = 1;
                bgLayers = 2;
            }
            else {
                winLayers = 3;
                bgLayers = 0;
            }
        }
        else {
            winLayers = 0;
            bgLayers = 3;
            WIN_IN |= 7;
            WIN0_X0 = screenOffsX;
            WIN0_Y0 = screenOffsY;
        }

        if (winLayers != 0) {
            int whofs = -(state->winX-7)-screenOffsX;
            int wvofs = -(gbLine-state->winPosY)-screenOffsY;
            if (winLayers == 1) {
                BG_CNT(layer) = state->winAllCnt;
                BG_HOFS(layer) = whofs;
                BG_VOFS(layer++) = wvofs;
            }
            else {
                BG_CNT(layer) = state->winColor0Cnt;
                BG_HOFS(layer) = whofs;
                BG_VOFS(layer++) = wvofs;
                BG_CNT(layer) = state->winCnt;
                BG_HOFS(layer) = whofs;
                BG_VOFS(layer++) = wvofs;
                if (winLayers >= 3) {
                    BG_CNT(layer) = state->winOverlayCnt;
                    BG_HOFS(layer) = whofs;
                    BG_VOFS(layer++) = wvofs;
                }
            }
            if (state->winX <= 7)
                WIN0_X0 = screenOffsX;
            else
                WIN0_X0 = state->winX-7+screenOffsX;
            WIN_IN |= 7<<8;
            for (int i=0; i<layer; i++)
                WIN_IN &= ~(1<<(8+i));
        }
        if (bgLayers > 1) {
            int hofs = state->hofs-screenOffsX;
            int vofs = state->vofs-screenOffsY;
            BG_CNT(layer) = state->bgColor0Cnt;
            BG_HOFS(layer) = hofs;
            BG_VOFS(layer++) = vofs;
            BG_CNT(layer) = state->bgCnt;
            BG_HOFS(layer) = hofs;
            BG_VOFS(layer++) = vofs;
            if (bgLayers >= 3) {
                BG_CNT(layer) = state->bgOverlayCnt;
                BG_HOFS(layer) = hofs;
                BG_VOFS(layer++) = vofs;
            }
        }
    }

    if (state->bgPalettesModified) {
        if (gbMode == GB) {
            for (int i=0; i<4; i++)
                updateBgPalette(i, state->bgPaletteData, state->bgPal);
        }
        else {
            for (int i=0; i<8; i++) {
                updateBgPalette(i, state->bgPaletteData, 0);
            }
        }
    }
    if (state->sprPalettesModified) {
        if (gbMode == GB) {
            for (int i=0; i<4; i++) {
                updateSprPalette(i, state->sprPaletteData, state->sprPal[0]);
                updateSprPalette(i+4, state->sprPaletteData, state->sprPal[1]);
            }
        }
        else {
            for (int i=0; i<8; i++) {
                updateSprPalette(i, state->sprPaletteData, 0);
            }
        }
    }
    if (state->spritesModified)
        drawSprites(state->spriteData, state->tallSprites);
}

void hblankHandler() ITCM_CODE;

void hblankHandler()
{
    int line = REG_VCOUNT+1;
    int gbLine = line-screenOffsY;
    if ((REG_DISPSTAT&3) != 2)
        gbLine--;

    if (gbLine >= 144)
        return;
    if (gbLine <= 0) {
        gbLine = 0;
        if (lineCompleted[0])
            return;

        // These vram banks may have been allocated to arm7 for scaling stuff.
        vramSetBankC(VRAM_C_SUB_BG);
        if (sharedData->scalingOn)
            vramSetBankD(VRAM_D_LCD);
    }

    if (gbLine != 0 && !lineCompleted[gbLine-1] && drawingState[gbLine-1].modified)
        drawLine(gbLine-1);

    lineCompleted[gbLine] = true;

    if (!drawingState[gbLine].modified)
        return;

    drawLine(gbLine);
}

bool shift=false;
void vblankHandler()
{
    if (!consoleOn) {
        if (sharedData->scalingOn) {
            // Capture the main display into bank D
            REG_DISPCAPCNT = 15 | 3<<16 | 0<<18 | 3<<20 | 0<<29 | 1<<31;

            // Leave the DMA copying for arm7
            //dmaCopyWordsAsynch(0, (u8*)0x06860000, (u8*)0x06200000, 192*256*2);
            vramSetBankD(VRAM_D_ARM7_0x06000000);
            vramSetBankC(VRAM_C_ARM7_0x06020000);
            sharedData->scaleTransferReady = true;
        }

        frame++;
        if (frame == 150 && probingForBorder) {
            // Give up on finding a sgb border.
            probingForBorder = false;
            nukeBorder = true;
            resetGameboy();
        }
    }

    memset(lineCompleted, 0, sizeof(lineCompleted));
    if (scaleFilter == 1) {
        if (shift) {
            REG_BG2X_SUB = SCALE_BGX - (1<<5);
            REG_BG2Y_SUB = SCALE_BGY - (1<<5);
            REG_BG3X_SUB = SCALE_BGX + (1<<5);
            REG_BG3Y_SUB = SCALE_BGY + (1<<5);
        }
        else {
            REG_BG2X_SUB = SCALE_BGX - (1<<5);
            REG_BG2Y_SUB = SCALE_BGY + (1<<5);
            REG_BG3X_SUB = SCALE_BGX + (1<<5);
            REG_BG3Y_SUB = SCALE_BGY - (1<<5);
        }
        shift = !shift;
    }
}

int loadBorder(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (file == NULL) {
        printLog("Error opening border.\n");
        return 1;
    }

    fseek(file, 0xe, SEEK_SET);
    u8 pixelStart = (u8)fgetc(file) + 0xe;
    fseek(file, pixelStart, SEEK_SET);
    for (int y=191; y>=0; y--) {
        u16 buffer[256];
        fread(buffer, 2, 0x100, file);
        /*
        DC_FlushRange(buffer, 0x100*2);
        dmaCopy(buffer, BG_GFX+0x20000+y*256, 0x100*2);
        */
        for (int i=0; i<256; i++) {
            BG_GFX[0x20000+y*256+i] = ((buffer[i]>>10)&0x1f) | ((buffer[i])&(0x1f<<5)) | (buffer[i]&0x1f)<<10 | BIT(15);
        }
    }

    fclose(file);

    loadedBorderType = BORDER_CUSTOM;
    return 0;
}


void initGFX()
{
    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankB(VRAM_B_MAIN_BG);
    vramSetBankE(VRAM_E_MAIN_SPRITE);

    map[0] = BG_MAP_RAM(map_base[0]);
    map[1] = BG_MAP_RAM(map_base[1]);
    color0Map[0] = BG_MAP_RAM(color0_map_base[0]);
    color0Map[1] = BG_MAP_RAM(color0_map_base[1]);
    overlayMap[0] = BG_MAP_RAM(overlay_map_base[0]);
    overlayMap[1] = BG_MAP_RAM(overlay_map_base[1]);
    offMap = BG_MAP_RAM(off_map_base);
    borderMap = BG_MAP_RAM(border_map_base);

    // Tile for "color0 maps", which contain only the gameboy's color 0.
    for (int i=0; i<16; i++) {
        BG_GFX[i] = 1 | (1<<4) | (1<<8) | (1<<12);
    }

    // Initialize the "off map", for when the gameboy screen is disabled.
    // Uses the "color0 map" tile because, why not.
    for (int i=0; i<32*32; i++) {
        offMap[i] = 15<<12;
    }
    // Off map palette
    BG_PALETTE[15*16+1] = RGB8(255,255,255);

    for (int i=40; i<128; i++)
        sprites[i].attr0 = ATTR0_DISABLED;

    initGFXPalette();

    WIN_IN = 7 | (1<<4) | (1<<12);
    WIN_OUT = 1<<3;
    WIN1_X0 = screenOffsX;
    WIN1_X1 = screenOffsX+160;
    WIN1_Y0 = screenOffsY;
    WIN1_Y1 = screenOffsY+144;
    WIN0_X0 = screenOffsX;
    WIN0_Y0 = screenOffsY;
    WIN0_X1 = screenOffsX+160;
    WIN0_Y1 = screenOffsY+144;

    videoSetMode(MODE_3_2D | DISPLAY_BG0_ACTIVE | DISPLAY_BG1_ACTIVE | DISPLAY_BG2_ACTIVE | DISPLAY_BG3_ACTIVE |
            DISPLAY_WIN0_ON | DISPLAY_WIN1_ON | DISPLAY_SPR_ACTIVE | DISPLAY_SPR_1D);

    setCustomBorder(customBordersEnabled);
    
    REG_DISPSTAT &= 0xFF;
    REG_DISPSTAT |= (144+screenOffsY)<<8;

    //irqEnable(IRQ_VCOUNT);
    irqEnable(IRQ_HBLANK);
    irqEnable(IRQ_VBLANK);
    irqSet(IRQ_VBLANK, &vblankHandler);
    irqSet(IRQ_HBLANK, &hblankHandler);

    memset(vram[0], 0, 0x2000);
    memset(vram[1], 0, 0x2000);

    gfxMask = 0;
    frame = 0;

    refreshGFX();
}

void initGFXPalette() {
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
}

void refreshGFX() {
    for (int i=0; i<0x180; i++) {
        drawTile(i, 0);
        drawTile(i, 1);
    }
    for (int i=0; i<0x400; i++) {
        updateTileMap(0, i, vram[0][0x1800+i]);
        updateTileMap(1, i, vram[0][0x1c00+i]);
    }
    for (int i=0; i<8; i++) {
        bgPaletteModified[i] = true;
        spritePaletteModified[i] = true;
    }
    bgPalettesModified = true;
    sprPalettesModified = true;
    lastScreenDisabled = !(ioRam[0x40] & 0x80);
    screenDisabled = lastScreenDisabled;
    /*
    for (int i=0; i<0xa0; i++)
        spriteData[i] = hram[i];
    */
}

void refreshSgbPalette() {
    int winMap=0,bgMap=0;
    bool winOn=0;
    int hofs=0,vofs=0,winX=0,winY=0;

    for (int y=0; y<18; y++) {
        for (int yPix=y*8-7; yPix<=y*8; yPix++) {
            if (yPix >= 0 && renderingState[yPix].modified && renderingState[yPix].mapsModified) {
                winOn = renderingState[yPix].winOn;
                winX = renderingState[yPix].winX;
                winY = renderingState[yPix].winY;
                hofs = renderingState[yPix].hofs;
                vofs = renderingState[yPix].vofs;
                winMap = renderingState[yPix].winMap;
                bgMap = renderingState[yPix].bgMap;
            }
        }
        for (int x=0; x<20; x++) {
            int palette = sgbMap[y*20+x]&3;

            int realx = (x*8-(winX-7))/8;
            int realy = (y*8-winY)/8;

            if (winOn) {
                if (realx >= 0 && realy >= 0 && realx < 32 && realy < 32) {
                    int i = realy*32+realx;
                    mapBuf[winMap][i] &= ~(7<<12);
                    mapBuf[winMap][i] |= (palette<<12);
                }
            }

            int loop = (x == 19 ? 2 : 1); // Give tile 20 tile 19's palette, as it's scrolling in
            for (int j=0; j<loop; j++) {
                realx = (((x+j)*8+hofs)&0xff)/8;
                realy = ((y*8+vofs+7)&0xff)/8;
                int i = realy*32+realx;
                mapBuf[bgMap][i] &= ~(7<<12);
                mapBuf[bgMap][i] |= (palette<<12);
            }
        }
    }
}

void setCustomBorder(bool enabled) {
    if (loadedBorderType == BORDER_CUSTOM && !customBordersEnabled)
        loadedBorderType = BORDER_NONE;
    if (loadedBorderType == BORDER_SGB && !sgbBordersEnabled)
        loadedBorderType = BORDER_NONE;

    if (!nukeBorder && loadedBorderType != BORDER_NONE) {
        goto end; // Don't overwrite the border when "reset" is selected.
    }

    if (enabled) {
        if (loadedBorderType == BORDER_CUSTOM)
            return;
        if (loadBorder("/border.bmp") == 1)
            loadedBorderType = BORDER_NONE;
    }
    else
        loadedBorderType = BORDER_NONE;

end:

    videoBgDisable(3);
    if (loadedBorderType == BORDER_CUSTOM) {
        REG_DISPCNT &= ~7;
        REG_DISPCNT |= 3; // Mode 3
        REG_BG3CNT = BG_MAP_BASE(16) | BG_BMP16_256x256;
        REG_BG3X = 0;
        REG_BG3Y = 0;
        REG_BG3PA = 1<<8;
        REG_BG3PB = 0;
        REG_BG3PC = 0;
        REG_BG3PD = 1<<8;
        videoBgEnable(3);
    }
    else if (loadedBorderType == BORDER_SGB) {
        REG_DISPCNT &= ~7; // Mode 0
        videoBgEnable(3);
    }
    else if (loadedBorderType == BORDER_NONE) {
        BG_PALETTE[0] = 0; // Reset backdrop (SGB borders use the backdrop)
    }
}
void refreshScaleMode() {
    int BG2PA,BG2PB,BG2PC,BG2PD;

    if (scaleMode == 0) {
        return;
    }

    if (loadedBorderType == BORDER_CUSTOM) {
        setCustomBorder(false);
    }
    videoSetModeSub(MODE_5_2D);
    videoBgDisableSub(0);
    videoBgDisableSub(1);
    bgInitSub(2, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);

    switch(scaleMode) {
        case 1:
            {
                const double scaleFactor = (double)191/144 + 0.125/144;
                BG2PA = (1<<8)/scaleFactor;
                BG2PB = 0;
                BG2PC = 0;
                BG2PD = (1<<8)/scaleFactor;
                int x = (256-160*scaleFactor)/2;
                SCALE_BGX = (1<<8)*(screenOffsX-x/scaleFactor+0.5);
                SCALE_BGY = 0;
                break;
            }
        case 2:
            {
                const double scaleFactorX = (double)255/160 + 0.125/160;
                const double scaleFactorY = (double)191/144 + 0.125/144;
                BG2PA = (1<<8)/(scaleFactorX);
                BG2PB = 0;
                BG2PC = 0;
                BG2PD = (1<<8)/(scaleFactorY);
                SCALE_BGX = (1<<8)*(screenOffsX-(256-160*scaleFactorX)/2/(scaleFactorX));
                SCALE_BGY = 0;
            }
            break;
        default:
            return;
    }

    REG_BLDCNT_SUB = 1<<2 | 1<<11 | 1<<6;
    REG_BLDALPHA_SUB = 8 | 8<<8;

    REG_BG2X_SUB = SCALE_BGX;
    REG_BG2Y_SUB = SCALE_BGY;
    REG_BG2PA_SUB = BG2PA;
    REG_BG2PB_SUB = BG2PB;
    REG_BG2PC_SUB = BG2PC;
    REG_BG2PD_SUB = BG2PD;

    if (scaleFilter == 1) {
        REG_BG3X_SUB = SCALE_BGX;
        REG_BG3Y_SUB = SCALE_BGY;
    }
    else if (scaleFilter == 2) {
        REG_BG3X_SUB = SCALE_BGX + (1<<6);
        REG_BG3Y_SUB = SCALE_BGY + (1<<6);
    }
    else {
        REG_BG3X_SUB = SCALE_BGX;
        REG_BG3Y_SUB = SCALE_BGY;
    }
    REG_BG3PA_SUB = BG2PA;
    REG_BG3PB_SUB = BG2PB;
    REG_BG3PC_SUB = BG2PC;
    REG_BG3PD_SUB = BG2PD;
}

void setGFXMask(int mask) {
    if (gfxMask == mask)
        return;
    gfxMask = mask;
    if (gfxMask == 0) {
        refreshGFX();
        if (loadedBorderType != BORDER_NONE) {
            videoBgEnable(3);
            BG_PALETTE[0] = bgPaletteData[0] | bgPaletteData[1]<<8;
        }
    }
}

void setSgbTiles(u8* src, u8 flags) {
    if (!sgbBordersEnabled)
        return;
    if (gfxMask != 0) {
        videoBgDisable(3);
        BG_PALETTE[0] = 0;
    }
    int index=0,srcIndex=0;
    if (flags&1)
        index += 0x80*16;
    for (int i=0; i<128; i++) {
        for (int y=0; y<8; y++) {
            int b1=src[srcIndex];
            int b2=src[srcIndex+1]<<1;
            int b3=src[srcIndex+16]<<2;
            int b4=src[srcIndex+17]<<3;
            srcIndex += 2;
            int bb0=0, bb1=0;
            int shift=12;
            for (int x=0; x<4; x++) {
                int colorid = b1&1;
                b1 >>= 1;
                colorid |= b2&2;
                b2 >>= 1;
                colorid |= b3&4;
                b3 >>= 1;
                colorid |= b4&8;
                b4 >>= 1;

                bb1 |= (colorid<<shift);
                shift -= 4;
            }
            shift = 12;
            for (int x=0; x<4; x++) {
                int colorid = b1&1;
                b1 >>= 1;
                colorid |= b2&2;
                b2 >>= 1;
                colorid |= b3&4;
                b3 >>= 1;
                colorid |= b4&8;
                b4 >>= 1;

                bb0 |= (colorid<<shift);
                shift -= 4;
            }
            BG_GFX[0x18000+index++] = bb0;
            BG_GFX[0x18000+index++] = bb1;
        }
        srcIndex += 16;
    }
}

void setSgbMap(u8* src) {
    if (!sgbBordersEnabled)
        return;
    if (gfxMask != 0) {
        videoBgDisable(3);
        BG_PALETTE[0] = 0;
    }
    for (int i=0; i<32*32; i++) {
        u16 val = ((u16*)src)[i];
        int tile = val&0xff;
        int paletteid = ((val>>10)&3)+8;
        int flipX = (val>>14)&1;
        int flipY = (val>>15)&1;
        borderMap[i] = tile | (paletteid<<12) | (flipX<<10) | (flipY<<11);
    }

    DC_FlushRange(src+0x800, 0x80);
    dmaCopy(src+0x800, BG_PALETTE+8*16, 0x80);

    if (loadedBorderType != BORDER_SGB)
        videoBgDisable(3);
    loadedBorderType = BORDER_SGB;
    REG_BG3CNT = BG_MAP_BASE(border_map_base) | BG_TILE_BASE(12);
    REG_BG3HOFS = 0;
    REG_BG3VOFS = -(screenOffsY-40);
    REG_DISPCNT &= ~7; // Mode 0
    videoBgEnable(3);

    BG_PALETTE[0] = bgPaletteData[0] | bgPaletteData[1]<<8;
    if (probingForBorder) {
        probingForBorder = false;
        nukeBorder = false;
        resetGameboy();
    }
}

// Possibly doing twice the work necessary in gbc games, when writing to bank 0, then bank 1.
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
    color0MapBuf[m][i] = paletteid<<12;
}

void drawTile(int tileNum, int bank) {
    int index = (tileNum<<4)+(bank*0x100*16);
    int signedIndex=index;
    if (tileNum >= 0x100)
        signedIndex -= (0x100<<4);

    bool unsign = tileNum < 0x100;
    bool sign = tileNum >= 0x80;
    u8* src = &vram[bank][tileNum<<4];
    for (int y=0; y<8; y++) {
        int b1=*(src++);
        int b2=*(src++)<<1;
        int bb0=0, bb1=0;
        int fb0=0, fb1=0;
        int sb0=0, sb1=0;
        int shift=12;
        for (int x=0; x<4; x++) {
            int colorid = b1&1;
            b1 >>= 1;
            colorid |= b2&2;
            b2 >>= 1;

            fb1 |= (colorid+1)<<shift;
            if (colorid != 0)
                bb1 |= ((colorid+1)<<shift);
            if (unsign)
                sb1 |= (colorid<<shift);
            shift -= 4;
        }
        shift = 12;
        for (int x=0; x<4; x++) {
            int colorid = b1&1;
            b1 >>= 1;
            colorid |= b2&2;
            b2 >>= 1;

            fb0 |= (colorid+1)<<shift;
            if (colorid != 0)
                bb0 |= ((colorid+1)<<shift);
            if (unsign)
                sb0 |= (colorid<<shift);
            shift -= 4;
        }
        if (unsign) {
            BG_GFX[0x8000+index] = bb0;
            BG_GFX[0x8000+index+1] = bb1;
            BG_GFX[0x10000+index] = fb0;
            BG_GFX[0x10000+index+1] = fb1;
            SPRITE_GFX[index++] = sb0;
            SPRITE_GFX[index++] = sb1;
        }
        if (sign) {
            BG_GFX[0xc000+signedIndex] = bb0;
            BG_GFX[0xc000+signedIndex+1] = bb1;
            BG_GFX[0x14000+signedIndex++] = fb0;
            BG_GFX[0x14000+signedIndex++] = fb1;
        }
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
    if (probingForBorder)
        return;
    if (sgbMode && !gfxMask)
        refreshSgbPalette();

    DC_FlushRange(mapBuf[0], 0x400*2);
    DC_FlushRange(mapBuf[1], 0x400*2);
    DC_FlushRange(color0MapBuf[0], 0x400*2);
    DC_FlushRange(color0MapBuf[1], 0x400*2);
    DC_FlushRange(overlayMapBuf[0], 0x400*2);
    DC_FlushRange(overlayMapBuf[1], 0x400*2);

    if (!(fastForwardMode || fastForwardKey))
        swiIntrWait(interruptWaitMode, IRQ_VBLANK);

    if (gfxMask)
        return;
    dmaCopy(mapBuf[0], map[0], 0x400*2);
    dmaCopy(mapBuf[1], map[1], 0x400*2);
    dmaCopy(color0MapBuf[0], color0Map[0], 0x400*2);
    dmaCopy(color0MapBuf[1], color0Map[1], 0x400*2);
    dmaCopy(overlayMapBuf[0], overlayMap[0], 0x400*2);
    dmaCopy(overlayMapBuf[1], overlayMap[1], 0x400*2);

    ScanlineStruct* tmp = renderingState;
    renderingState = drawingState;
    drawingState = tmp;

    screenDisabled = lastScreenDisabled;
    if (!(ioRam[0x40] & 0x80))
        screenDisabled = true;
    lastScreenDisabled = !(ioRam[0x40] & 0x80);

    winPosY = -1;

    // Check we're actually done drawing the screen.
    if (REG_VCOUNT >= screenOffsY+144) {
        REG_DISPCNT |= DISPLAY_SPR_ACTIVE;
    }

    updateTiles();
}

void drawSprites(u8* data, int tall) {
    for (int i=0; i<40; i++) {
        int spriteNum = i*4;
        if (data[spriteNum] == 0)
            sprites[i].attr0 = ATTR0_DISABLED;
        else
        {
            int y = data[spriteNum]-16;
            int tileNum = data[spriteNum+2];
            if (tall)
                tileNum &= ~1;
            int x = data[spriteNum+1]-8;
            int bank = 0;
            int flipX = !!(data[spriteNum+3] & 0x20);
            int flipY = !!(data[spriteNum+3] & 0x40);
            int priority = !!(data[spriteNum+3] & 0x80);
            int paletteid;

            if (gbMode == CGB)
            {
                bank = !!(data[spriteNum+3]&0x8);
                paletteid = data[spriteNum+3] & 0x7;
            }
            else
            {
                if (sgbMode) {
                    int yPos = (y < 0 ? 0 : y/8);
                    int xPos = (x < 0 ? 0 : x/8);
                    int sgbPalette = sgbMap[yPos*20 + xPos]&3;
                    paletteid = sgbPalette+(!!(data[spriteNum+3] & 0x10))*4;
                }
                else
                    paletteid = ((data[spriteNum+3] & 0x10) ? 5 : 0);
            }

            int priorityVal = (priority ? spr_priority_low : spr_priority);
            sprites[i].attr0 = (y+screenOffsY) | (tall<<15);
            sprites[i].attr1 = ((x&0x1ff)+screenOffsX) | (flipX<<12) | (flipY<<13);
            sprites[i].attr2 = (tileNum+(bank*0x100)) | (priorityVal<<10) | (paletteid<<12);
        }
    }
}

void drawScanline(int scanline) ITCM_CODE;

void drawScanline(int scanline)
{
    renderingState[scanline].mapsModified = false;
    renderingState[scanline].spritesModified = false;
    renderingState[scanline].bgPalettesModified = false;
    renderingState[scanline].sprPalettesModified = false;
    if (hblankDisabled || gfxMask)
        return;
    int winX = ioRam[0x4b];
    if (winPosY == -2)
        winPosY = ioRam[0x44]-ioRam[0x4a];
    else if (winX < 167 && ioRam[0x4a] <= scanline)
        winPosY++;
    if (scanline == 0)
        spritesModified = true;
    renderingState[scanline].spritesModified = spritesModified;
    if (spritesModified) {
        for (int i=0; i<0xa0; i++)
            renderingState[scanline].spriteData[i] = hram[i];
        renderingState[scanline].tallSprites = !!(ioRam[0x40]&4);
        lineModified = true;
        spritesModified = false;
    }
    if (scanline == 0 || scanline == ioRam[0x4a])
        lineModified = true;
    else if (!lineModified) {
        renderingState[scanline].modified = false;
        return;
    } 

    renderingState[scanline].modified = true;
    renderingState[scanline].mapsModified = true;
    lineModified = false;

    bool winOn = (ioRam[0x40] & 0x20) && winX < 167 && ioRam[0x4a] < 144 && ioRam[0x4a] <= scanline;
    renderingState[scanline].hofs = ioRam[0x43];
    renderingState[scanline].vofs = ioRam[0x42];
    renderingState[scanline].winX = winX;
    renderingState[scanline].winPosY = winPosY;
    renderingState[scanline].winY = ioRam[0x4a];

    int tileSigned,winMap,bgMap;
    if (ioRam[0x40] & 0x10)
        tileSigned = 0;
    else
        tileSigned = 1;
    if (ioRam[0x40] & 0x40)
        winMap = 1;
    else
        winMap = 0;
    if (ioRam[0x40] & 0x8)
        bgMap = 1;
    else
        bgMap = 0;

    renderingState[scanline].winOn = winOn;
    renderingState[scanline].spritesOn = ioRam[0x40] & 0x2;

    renderingState[scanline].bgMap = bgMap;
    renderingState[scanline].winMap = winMap;

    if (gbMode != CGB && !(ioRam[0x40] & 1)) {
        renderingState[scanline].winColor0Cnt = BG_MAP_BASE(off_map_base) | 3;
        renderingState[scanline].winCnt = BG_MAP_BASE(off_map_base) | 3;
        renderingState[scanline].winOverlayCnt = BG_MAP_BASE(off_map_base) | 3;
        renderingState[scanline].bgColor0Cnt = BG_MAP_BASE(off_map_base) | 3;
        renderingState[scanline].bgCnt = BG_MAP_BASE(off_map_base) | 3;
        renderingState[scanline].bgOverlayCnt = BG_MAP_BASE(off_map_base) | 3;
    }
    else {
        bool priorityOn = gbMode == CGB && ioRam[0x40] & 1;

        int winMapBase = map_base[winMap];
        int bgMapBase = map_base[bgMap];

        renderingState[scanline].tileSigned = tileSigned;

        int tileBase = (tileSigned ? 6 : 4);

        renderingState[scanline].winColor0Cnt = (BG_MAP_BASE(color0_map_base[winMap]) | BG_TILE_BASE(0) | win_color0_priority);
        renderingState[scanline].winCnt = (BG_MAP_BASE(winMapBase) | BG_TILE_BASE(tileBase) | win_priority);
        renderingState[scanline].bgColor0Cnt = (BG_MAP_BASE(color0_map_base[bgMap]) | BG_TILE_BASE(0) | bg_color0_priority);
        renderingState[scanline].bgCnt = (BG_MAP_BASE(bgMapBase) | BG_TILE_BASE(tileBase) | bg_priority);

        renderingState[scanline].winAllCnt = (BG_MAP_BASE(winMapBase) | BG_TILE_BASE(tileBase+4) | win_all_priority);

        if (priorityOn) {
            renderingState[scanline].winOverlayCnt = (BG_MAP_BASE(overlay_map_base[winMap]) | BG_TILE_BASE(tileBase) | win_overlay_priority);
            renderingState[scanline].bgOverlayCnt = (BG_MAP_BASE(overlay_map_base[bgMap]) | BG_TILE_BASE(tileBase) | bg_overlay_priority);
        }
        else {
            // Give these layers the same priority as the regular layers.
            renderingState[scanline].winOverlayCnt = (BG_MAP_BASE(overlay_map_base[winMap]) | BG_TILE_BASE(tileBase) | win_priority);
            renderingState[scanline].bgOverlayCnt = (BG_MAP_BASE(overlay_map_base[bgMap]) | BG_TILE_BASE(tileBase) | bg_priority);
        }
    }
}

void drawScanlinePalettes(int scanline) {
    if (hblankDisabled || gfxMask)
        return;
    bool palettesModified = false;
    if (scanline == 0) {
        renderingState[scanline].bgPalettesModified = true;
        renderingState[scanline].sprPalettesModified = true;
        bgPalettesModified = false;
        sprPalettesModified = false;
        palettesModified = true;
    }
    else {
        renderingState[scanline].bgPalettesModified = bgPalettesModified;
        renderingState[scanline].sprPalettesModified = sprPalettesModified;
        if (bgPalettesModified) {
            bgPalettesModified = false;
            palettesModified = true;
        }
        if (sprPalettesModified) {
            sprPalettesModified = false;
            palettesModified = true;
        }
    }
    if (!palettesModified)
        return;

    renderingState[scanline].modified = true;

    if (renderingState[scanline].bgPalettesModified) {
        for (int i=0; i<0x40; i++) {
            renderingState[scanline].bgPaletteData[i] = bgPaletteData[i];
        }
    }
    if (renderingState[scanline].sprPalettesModified) {
        for (int i=0; i<0x40; i++) {
            renderingState[scanline].sprPaletteData[i] = sprPaletteData[i];
        }
    }
    renderingState[scanline].bgPal = ioRam[0x47];
    renderingState[scanline].sprPal[0] = ioRam[0x48];
    renderingState[scanline].sprPal[1] = ioRam[0x49];
}

void writeVram(u16 addr, u8 val) {
    if (vram[vramBank][addr] == val)
        return;
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

void updateBgPalette(int paletteid, u8* data, u8 dmgPal) {
    for (int i=0; i<4; i++) {
        int id;
        if (gbMode == GB)
            id = (dmgPal>>(i*2))&3;
        else
            id = i;

        BG_PALETTE[((paletteid)*16)+i+1] = data[(paletteid*8)+(id*2)] | data[(paletteid*8)+(id*2)+1]<<8;
    }
}
void updateSprPalette(int paletteid, u8* data, u8 dmgPal) {
    int src = paletteid;
    if (gbMode == GB && paletteid >= 4) // SGB stuff
        src -= 4;
    for (int i=0; i<4; i++) {
        int id;
        if (gbMode == GB)
            id = (dmgPal>>(i*2))&3;
        else
            id = i;

        SPRITE_PALETTE[((paletteid)*16)+i] = data[(src*8)+(id*2)] | data[(src*8)+(id*2)+1]<<8;
    }
}

void handleVideoRegister(u8 ioReg, u8 val) {
    switch(ioReg) {
        case 0x40:    // LCDC
            if ((val & 0x7F) != (ioRam[0x40] & 0x7F))
                lineModified = true;
            if ((val&4) != (ioRam[0x40]&4))
                spritesModified = true;
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
                    /*
                    if (ioRam[0x44] >= 144 || ioRam[0x44] <= 1)
                        spriteData[i] = val;
                    */
                }
                spritesModified = true;

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
        case 0x47:				// BG Palette (GB classic only)
            ioRam[0x47] = val;
            if (gbMode == GB)
            {
                bgPalettesModified = true;
            }
            return;
        case 0x48:				// Spr Palette (GB classic only)
            ioRam[0x48] = val;
            if (gbMode == GB)
            {
                sprPalettesModified = true;
            }
            return;
        case 0x49:				// Spr Palette (GB classic only)
            ioRam[0x49] = val;
            if (gbMode == GB)
            {
                sprPalettesModified = true;
            }
            return;
        case 0x69:				// BG Palette Data (GBC only)
            {
                int index = ioRam[0x68] & 0x3F;
                if (bgPaletteData[index] != val) {
                    bgPaletteData[index] = val;
                    bgPalettesModified = true;
                }

                if (ioRam[0x68] & 0x80)
                    ioRam[0x68]++;
                ioRam[0x69] = bgPaletteData[ioRam[0x68]&0x3F];
                return;
            }
        case 0x6B:				// Sprite Palette Data (GBC only)
            {
                int index = ioRam[0x6A] & 0x3F;
                if (sprPaletteData[index] != val) {
                    sprPaletteData[index] = val;
                    sprPalettesModified = true;
                }

                if (ioRam[0x6A] & 0x80)
                    ioRam[0x6A]++;
                ioRam[0x6B] = sprPaletteData[ioRam[0x6A]&0x3F];
                return;
            }
        default:
            ioRam[ioReg] = val;
    }
}

void writeHram(u16 addr, u8 val) {
    hram[addr&0x1ff] = val;
    spritesModified = true;
}

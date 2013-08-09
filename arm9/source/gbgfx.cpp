#include "inputhelper.h"
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

#define BACKDROP_COLOUR RGB15(0,0,0)

const int map_base[] = {1, 2};
const int color0_map_base[] = {3, 4};
const int overlay_map_base[] = {5, 6};
const int off_map_base = 7;
const int border_map_base = 8;

u16* const map[2] = {BG_MAP_RAM(map_base[0]), BG_MAP_RAM(map_base[1])};
u16* const color0Map[2] = {BG_MAP_RAM(color0_map_base[0]), BG_MAP_RAM(color0_map_base[1])};
u16* const overlayMap[2] = {BG_MAP_RAM(overlay_map_base[0]), BG_MAP_RAM(overlay_map_base[1])};
u16* const offMap = BG_MAP_RAM(off_map_base);
u16* const borderMap = BG_MAP_RAM(border_map_base);


const int spr_priority = 2;
const int spr_priority_low = 3;

const int win_color0_priority = 3;
const int win_priority = 2;

const int bg_color0_priority = 3;
const int bg_priority = 2;

const int bg_overlay_priority = 1;
const int win_overlay_priority = 1;

const int bg_all_priority = 2;
const int win_all_priority = 2;

const int screenOffsX = 48;
const int screenOffsY = 24;

// If the game is using the GBC's priority bit for tiles, the corresponding 
// variable is set. This helps with layer management.
int usingTilePriority[2];


bool didVblank=false;
// Frame counter. Incremented each vblank.
u16 frame=0;

bool changedTile[2][0x180];
int changedTileQueueLength=0;
u16 changedTileQueue[0x300];

bool changedTileInFrame[2][0x180];
int changedTileInFrameQueueLength=0;
u16 changedTileInFrameQueue[0x300];

bool changedMap[2][0x400];
int changedMapQueueLength[2];
u16 changedMapQueue[2][0x400];

u8 bgPaletteData[0x40] ALIGN(4);
u8 sprPaletteData[0x40] ALIGN(4);

int winPosY=0;

bool lineModified;
bool mapsModified;
bool bgPalettesModified;
bool sprPalettesModified;
bool spritesModified;


// Whether to wait for vblank if emulation is running behind schedule
int interruptWaitMode=0;
bool windowDisabled = false;
bool hblankDisabled = false;

int scaleMode;
int scaleFilter=1;
u8 gfxMask;
volatile int loadedBorderType; // This is read from hblank

int SCALE_BGX, SCALE_BGY;

bool lastScreenDisabled;
bool screenDisabled;


void drawSprites(u8* data, int tallSprites);
void drawTile(int tile, int bank);
void updateTiles();
void updateTileMaps();
void updateTileMap(int map, int i);
void updateBgPalette(int paletteid, u8* data, u8 dmgPal);
void updateBgPalette_GBC(int paletteid, u8* data);
void updateSprPalette(int paletteid, u8* data, u8 dmgPal);

bool lineCompleted[144];
typedef struct {
    u8 bgPaletteData[0x40] ALIGN(4);
    u8 sprPaletteData[0x40] ALIGN(4);
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
    u16 bgAllCnt;
    u16 winColor0Cnt;   // Contains color 0 only
    u16 winCnt;         // Contains colors 1-3
    u16 winOverlayCnt;  // Contains colors 1-3, only for 'priority' tiles
    u16 winAllCnt;      // Contains colors 0-3. Used when available layers are limited.
    u8 bgMap,winMap;
    bool spritesOn;
    bool tileSigned;
    u8 bgPal;
    u8 sprPal[2];
    bool bgPalettesModified;
    bool sprPalettesModified;
    bool spritesModified;
    u8 spriteData[0xa0];
    int tallSprites;
    int bgHash;
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

void drawLine(int gbLine) ITCM_CODE;

void drawLine(int gbLine) {
    ScanlineStruct *state = &drawingState[gbLine];

    if (screenDisabled) {
        REG_DISPCNT &= ~DISPLAY_SPR_ACTIVE;
        REG_BG0CNT = BG_MAP_BASE(off_map_base);
        if (loadedBorderType)
            WIN_IN |= 7<<8;
        else
            WIN_IN |= 15<<8;
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
                if (loadedBorderType == BORDER_NONE) {
                    winLayers = 2;
                    bgLayers = 2;
                }
                else {
                    if (usingTilePriority[state->bgMap]) {
                        winLayers = 1;
                        bgLayers = 2;
                    }
                    else {
                        winLayers = 1;
                        bgLayers = 2;
                    }
                }
            }
            else {
                winLayers = 3;
                bgLayers = 0;
            }
        }
        else {
            winLayers = 0;
            bgLayers = 3;
            WIN0_X0 = screenOffsX+160;
            WIN0_Y0 = screenOffsY;
        }
        WIN_IN |= 15<<8;

        if (winLayers != 0) {
            int whofs = -(state->winX-7)-screenOffsX;
            int wvofs = -(gbLine-state->winPosY)-screenOffsY;
            if (gbMode == GB) {
                // In GB mode, BG and Window share the backdrop color.
                // So they don't need their own dedicated layers.
                BG_CNT(layer) = state->winCnt;
                BG_HOFS(layer) = whofs;
                BG_VOFS(layer++) = wvofs;
            }
            else if (winLayers == 1) {
                BG_CNT(layer) = state->winAllCnt;
                BG_HOFS(layer) = whofs;
                BG_VOFS(layer++) = wvofs;
            }
            else if (winLayers == 2) {
                if (usingTilePriority[state->winMap]) {
                    // Apply tile priority bit.
                    BG_CNT(layer) = state->winAllCnt;
                    BG_HOFS(layer) = whofs;
                    BG_VOFS(layer++) = wvofs;
                    BG_CNT(layer) = state->winOverlayCnt;
                    BG_HOFS(layer) = whofs;
                    BG_VOFS(layer++) = wvofs;
                }
                else {
                    // Apply sprite priority bit.
                    BG_CNT(layer) = state->winColor0Cnt;
                    BG_HOFS(layer) = whofs;
                    BG_VOFS(layer++) = wvofs;
                    BG_CNT(layer) = state->winCnt;
                    BG_HOFS(layer) = whofs;
                    BG_VOFS(layer++) = wvofs;
                }
            }
            else { // winLayers == 3
                BG_CNT(layer) = state->winColor0Cnt;
                BG_HOFS(layer) = whofs;
                BG_VOFS(layer++) = wvofs;
                BG_CNT(layer) = state->winCnt;
                BG_HOFS(layer) = whofs;
                BG_VOFS(layer++) = wvofs;
                BG_CNT(layer) = state->winOverlayCnt;
                BG_HOFS(layer) = whofs;
                BG_VOFS(layer++) = wvofs;
            }

            if (state->winX <= 7)
                WIN0_X0 = screenOffsX;
            else
                WIN0_X0 = state->winX-7+screenOffsX;
            WIN_IN &= ~15;
            for (int i=0; i<layer; i++) {
                WIN_IN &= ~(1<<(8+i));
                WIN_IN |= 1<<i;
            }
        }
        if (bgLayers != 0) {
            int hofs = state->hofs-screenOffsX;
            int vofs = state->vofs-screenOffsY;
            if (gbMode == GB) {
                BG_CNT(layer) = state->bgCnt;
                BG_HOFS(layer) = hofs;
                BG_VOFS(layer++) = vofs;
            }
            else {
                if (bgLayers >= 3) {
                    BG_CNT(layer) = state->bgColor0Cnt;
                    BG_HOFS(layer) = hofs;
                    BG_VOFS(layer++) = vofs;
                    BG_CNT(layer) = state->bgCnt;
                    BG_HOFS(layer) = hofs;
                    BG_VOFS(layer++) = vofs;
                    BG_CNT(layer) = state->bgOverlayCnt;
                    BG_HOFS(layer) = hofs;
                    BG_VOFS(layer++) = vofs;
                }
                else if (bgLayers == 2) { // not enough for both priority bits.
                    if (usingTilePriority[state->bgMap]) {
                        // Apply tile priority bit.
                        BG_CNT(layer) = state->bgAllCnt;
                        BG_HOFS(layer) = hofs;
                        BG_VOFS(layer++) = vofs;
                        BG_CNT(layer) = state->bgOverlayCnt;
                        BG_HOFS(layer) = hofs;
                        BG_VOFS(layer++) = vofs;
                    }
                    else {
                        // Apply sprite priority bit.
                        BG_CNT(layer) = state->bgColor0Cnt;
                        BG_HOFS(layer) = hofs;
                        BG_VOFS(layer++) = vofs;
                        BG_CNT(layer) = state->bgCnt;
                        BG_HOFS(layer) = hofs;
                        BG_VOFS(layer++) = vofs;
                    }
                }
                else { // bgLayers == 1
                    BG_CNT(layer) = state->bgAllCnt;
                    BG_HOFS(layer) = hofs;
                    BG_VOFS(layer++) = vofs;
                }
            }
        }
        if (gbMode == GB) {
            // This layer provides the backdrop for both BG and Window.
            WIN_IN |= (1<<layer); // Must be displayed over window
            BG_CNT(layer) = state->winColor0Cnt;
            BG_HOFS(layer) = 0;
            BG_VOFS(layer++) = 0;
        }
        while (layer < 4) {
            WIN_IN &= ~((1<<layer)<<8);
            WIN_IN &= ~(1<<layer++);
        }
    }

    if (state->bgPalettesModified) {
        if (gbMode == CGB) {
            for (int i=0; i<8; i++)
                updateBgPalette_GBC(i, state->bgPaletteData);
        }
        else {
            for (int i=0; i<4; i++) {
                updateBgPalette(i, state->bgPaletteData, state->bgPal);
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
    if (hblankDisabled)
        return;
    int line = REG_VCOUNT+1;
    if ((REG_DISPSTAT&3) != 2)
        line--;
    int gbLine = line-screenOffsY;

    if (gbLine >= 144 || gbLine <= 0)
        return;

    if (drawingState[gbLine-1].modified && !lineCompleted[gbLine-1])
        drawLine(gbLine-1);

    if (!drawingState[gbLine].modified)
        return;

    lineCompleted[gbLine] = true;

    drawLine(gbLine);
}

// Triggered on line 227, end of vblank
void vcountHandler() {
    drawLine(0);
    lineCompleted[0] = true;

    // These vram banks may have been allocated to arm7 for scaling stuff.
    vramSetBankC(VRAM_C_SUB_BG);
    if (sharedData->scalingOn)
        vramSetBankD(VRAM_D_LCD);
}

bool shift=false;
void vblankHandler()
{
    didVblank = true;
    if (!consoleOn) {
        if (sharedData->scalingOn) {
            // Capture the main display into vram bank D
            REG_DISPCAPCNT = 15 | 3<<16 | 0<<18 | 3<<20 | 0<<29 | 1<<31;

            // Leave the DMA copying for arm7.
            //dmaCopyWordsAsynch(0, (u16*)0x06860000+24*256, (u16*)0x06200000, 144*256*2);
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

    // Start loading
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

    return 0;
}

// This just sets up the background
void loadSGBBorder() {
    loadedBorderType = BORDER_SGB;
    videoBgDisable(3);
    WIN_OUT = 1<<3;
    REG_DISPCNT &= ~7; // Mode 0
    REG_BG3CNT = BG_MAP_BASE(border_map_base) | BG_TILE_BASE(12);
    REG_BG3HOFS = 0;
    REG_BG3VOFS = -(screenOffsY-40);
    videoBgEnable(3);
}


void initGFX()
{
    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankB(VRAM_B_MAIN_BG);
    vramSetBankE(VRAM_E_MAIN_SPRITE);

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

    WIN_IN = (1<<4) | (1<<12);
    WIN_OUT = 1<<3;
    WIN1_X0 = screenOffsX;
    WIN1_X1 = screenOffsX+160;
    WIN1_Y0 = screenOffsY;
    WIN1_Y1 = screenOffsY+144;
    WIN0_X0 = screenOffsX;
    WIN0_Y0 = screenOffsY;
    WIN0_X1 = screenOffsX+160;
    WIN0_Y1 = screenOffsY+144;

    int mode = (loadedBorderType == BORDER_CUSTOM ? MODE_3_2D : MODE_0_2D);
    videoSetMode(mode | DISPLAY_BG0_ACTIVE | DISPLAY_BG1_ACTIVE | DISPLAY_BG2_ACTIVE | DISPLAY_BG3_ACTIVE |
            DISPLAY_WIN0_ON | DISPLAY_WIN1_ON | DISPLAY_SPR_ACTIVE | DISPLAY_SPR_1D);

    checkBorder();

    REG_DISPSTAT &= 0xFF;
    REG_DISPSTAT |= 227<<8;     // Set line 227 for vcount

    irqEnable(IRQ_VCOUNT);
    irqEnable(IRQ_HBLANK);
    irqEnable(IRQ_VBLANK);
    irqSet(IRQ_VCOUNT, &vcountHandler);
    irqSet(IRQ_HBLANK, &hblankHandler);
    irqSet(IRQ_VBLANK, &vblankHandler);

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
        updateTileMap(0, i);
        updateTileMap(1, i);
    }
    changedMapQueueLength[0] = 0;
    changedMapQueueLength[1] = 0;
    lastScreenDisabled = !(ioRam[0x40] & 0x80);
    screenDisabled = lastScreenDisabled;
    winPosY = -1;

    usingTilePriority[0] = 0;
    usingTilePriority[1] = 0;
    for (int i=0x1800; i<0x1c00; i++) {
        if (vram[1][i] & 0x80)
            usingTilePriority[0]++;
    }
    for (int i=0x1c00; i<0x2000; i++) {
        if (vram[1][i] & 0x80)
            usingTilePriority[1]++;
    }
}

// SGB palettes can't quite be perfect because SGB doesn't (necessarily) align 
// palettes with tiles. Mostly problematic with scrolling around status bars.  
// Bars on the bottom are favored by this code.
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

            // BACKGROUND
            int yLoop = (y == 0 ? 2 : 1); // Give vertical tile -1 tile 0's palette as it's scrolling in
            int xLoop = (x == 19 ? 2 : 1); // Give horizontal tile 20 tile 19's palette, as it's scrolling in
            while (yLoop-- > 0) {
                for (int j=0; j<xLoop; j++) {
                    int realx = (((x+j)*8+hofs)&0xff)/8;
                    int realy = (((y-yLoop)*8+vofs+7)&0xff)/8;
                    int i = realy*32+realx;
                    map[bgMap][i] &= ~(7<<12);
                    map[bgMap][i] |= (palette<<12);
                }
            }

            // WINDOW
            if (winOn) {
                yLoop = (y == 17 ? 2 : 1); // Give vertical tile 19 tile 18's palette as it's scrolling in
                xLoop = (x == 19 ? 2 : 1); // x is treated the same as the background

                while (yLoop-- > 0) {
                    for (int j=0; j<xLoop; j++) {
                        int realx = ((x+j)*8-(winX-7))/8;
                        int realy = ((y+yLoop)*8-winY)/8;

                        if (realx >= 0 && realy >= 0 && realx < 32 && realy < 32) {
                            int i = realy*32+realx;
                            map[winMap][i] &= ~(7<<12);
                            map[winMap][i] |= (palette<<12);
                        }
                    }
                }
            }
        }
    }
}

void checkBorder() {
    int lastBorderType = loadedBorderType;
    int nextBorderType = BORDER_NONE;

    if (!nukeBorder) {
        if (lastBorderType == BORDER_SGB && sgbBordersEnabled) {
            nextBorderType = BORDER_SGB;
        }
    }
    nukeBorder = false;

    if (customBordersEnabled && scaleMode == 0 && nextBorderType == BORDER_NONE) {
        nextBorderType = BORDER_CUSTOM;
    }

end:
    /*
       if (loadedBorderType == nextBorderType)
       return;
       */
    loadedBorderType = nextBorderType;

    if (nextBorderType == BORDER_NONE) {
        WIN_OUT = 0;
        REG_DISPCNT &= ~7; // Mode 0
        BG_PALETTE[0] = BACKDROP_COLOUR; // Reset backdrop (SGB borders use the backdrop)
    }
    else {
        WIN_OUT = 1<<3;
        if (nextBorderType == BORDER_SGB) {
            loadSGBBorder();
        }
        else if (nextBorderType == BORDER_CUSTOM) {
            if (lastBorderType != BORDER_CUSTOM) { // Don't reload if it's already loaded
                videoBgDisable(3);
                if (loadBorder("/border.bmp") != 0) {
                    nextBorderType = BORDER_NONE;
                    goto end;
                }

                // Set up background
                REG_DISPCNT &= ~7;
                REG_DISPCNT |= 3; // Mode 3
                REG_BG3CNT = BG_MAP_BASE(16) | BG_BMP16_256x256;
                REG_BG3X = 0;
                REG_BG3Y = 0;
                REG_BG3PA = 1<<8;
                REG_BG3PB = 0;
                REG_BG3PC = 0;
                REG_BG3PD = 1<<8;

                swiWaitForVBlank();
                videoBgEnable(3);
            }
        }
    }
}
void refreshScaleMode() {
    int BG2PA,BG2PB,BG2PC,BG2PD;

    checkBorder();
    if (scaleMode == 0) {
        return;
    }

    videoSetModeSub(MODE_5_2D);
    videoBgDisableSub(0);
    videoBgDisableSub(1);
    bgInitSub(2, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);

    switch(scaleMode) {
        case 1: // Aspect
            {
                const double scaleFactor = (double)191/144 + 0.125/144;
                BG2PA = (1<<8)/scaleFactor;
                BG2PB = 0;
                BG2PC = 0;
                BG2PD = (1<<8)/scaleFactor;
                int x = (256-160*scaleFactor)/2;
                SCALE_BGX = (1<<8)*(screenOffsX-x/scaleFactor+0.5);
                SCALE_BGY = 1<<7; // 0.5
                break;
            }
        case 2: // Full
            {
                const double scaleFactorX = (double)255/160;
                const double scaleFactorY = (double)191/144 + 0.125/144;
                BG2PA = (1<<8)/(scaleFactorX);
                BG2PB = 0;
                BG2PC = 0;
                BG2PD = (1<<8)/(scaleFactorY);
                SCALE_BGX = (1<<8)*(screenOffsX-(256-160*scaleFactorX)/2/(scaleFactorX) + 0.5);
                SCALE_BGY = 1<<7; // 0.5
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
    printLog("Mask %d\n", mask);
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
    if (gfxMask != 0 && loadedBorderType != BORDER_NONE) {
        videoBgDisable(3);
        BG_PALETTE[0] = BACKDROP_COLOUR;
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

    loadSGBBorder();

    BG_PALETTE[0] = bgPaletteData[0] | bgPaletteData[1]<<8;
    if (probingForBorder) {
        probingForBorder = false;
        nukeBorder = false;
        resetGameboy();
    }
}

void updateTileMaps() {
    for (int m=0; m<2; m++) {
        while (changedMapQueueLength[m] != 0) {
            int tile = changedMapQueue[m][--changedMapQueueLength[m]];
            updateTileMap(m, tile);
        }
    }
}
void updateTileMap(int m, int i) {
    changedMap[m][i] = false;
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
        overlayMap[m][i] = (tileNum+(bank*0x100)) | (paletteid<<12) | (flipX<<10) | (flipY<<11);
    else {
        overlayMap[m][i] = 0x300;
    }
    map[m][i] = (tileNum+(bank*0x100)) | (paletteid<<12) | (flipX<<10) | (flipY<<11);
    color0Map[m][i] = paletteid<<12;
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

    if (!(fastForwardMode || fastForwardKey)) {
        if (interruptWaitMode == 1) // Wait for Vblank.
            swiWaitForVBlank();
        else { // Continue if we've passed vblank.
            if (__dsimode) {
                // This is essentially equivalent to using swiIntrWait(0, ...),
                // but swiIntrWait doesn't seem to work properly in dsi mode.
                if (!didVblank)
                    swiWaitForVBlank();
            }
            else
                swiIntrWait(0, IRQ_VBLANK);
        }
    }
    didVblank = false;

    sharedData->frameFlip_Gameboy = !sharedData->frameFlip_Gameboy;
    if (REG_VCOUNT == 192)
        sharedData->frameFlip_DS = sharedData->frameFlip_Gameboy;

    if (gfxMask)
        return;

    ScanlineStruct* tmp = renderingState;
    renderingState = drawingState;
    drawingState = tmp;

    screenDisabled = lastScreenDisabled;
    if (!(ioRam[0x40] & 0x80))
        screenDisabled = true;
    lastScreenDisabled = !(ioRam[0x40] & 0x80);

    winPosY = -1;

    updateTiles();
    updateTileMaps();
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
                    int yPos, xPos;
                    xPos = (x < 0 ? 0 : x/8);
                    if (tall)
                        yPos = (y < -7 ? 0 : (y+7)/8);
                    else
                        yPos = (y < -3 ? 0 : (y+3)/8);
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

int winX;
// Called after mode 2
void drawScanline(int scanline)
{
    // This japanese game, "Fushigi no Dungeon - Fuurai no Shiren..." expects 
    // writes to winX during mode 3 to take effect next scanline - at least, 
    // when winX is changed from >=167 to <167 or vice versa. Maybe this should 
    // be investigated further.
    if (winX != ioRam[0x4b]) {
        winX = ioRam[0x4b];
        lineModified = true;
        mapsModified = true;
    }
}

void drawScanline_P2(int scanline) ITCM_CODE;

// Called after mode 3
void drawScanline_P2(int scanline) {
    if (hblankDisabled)
        return;
    if (winPosY == -2)
        winPosY = ioRam[0x44]-ioRam[0x4a];
    else if (winX < 167 && ioRam[0x4a] <= scanline)
        winPosY++;

    if (scanline == 0) {
        lineModified = true;
        bgPalettesModified = true;
        sprPalettesModified = true;
        spritesModified = true;
        mapsModified = true;
    }
    else if (scanline == ioRam[0x4a]) { // First line of the window
        lineModified = true;
        mapsModified = true;
    }
    else if (!lineModified) {
        renderingState[scanline].modified = false;
        return;
    }

    renderingState[scanline].modified = true;

    renderingState[scanline].spritesModified = spritesModified;
    if (spritesModified) {
        for (int i=0; i<0xa0; i++)
            renderingState[scanline].spriteData[i] = hram[i];
        renderingState[scanline].tallSprites = !!(ioRam[0x40]&4);
        spritesModified = false;
    }

    renderingState[scanline].mapsModified = mapsModified;
    if (mapsModified) {
        mapsModified = false;

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
            renderingState[scanline].bgAllCnt = (BG_MAP_BASE(bgMapBase) | BG_TILE_BASE(tileBase+4) | bg_all_priority);

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

        bool winOn = (ioRam[0x40] & 0x20) && winX < 167 && ioRam[0x4a] < 144 && ioRam[0x4a] <= scanline;
        renderingState[scanline].winOn = winOn;
        renderingState[scanline].hofs = ioRam[0x43];
        renderingState[scanline].vofs = ioRam[0x42];
        renderingState[scanline].winX = winX;
        renderingState[scanline].winPosY = winPosY;
        renderingState[scanline].winY = ioRam[0x4a];
    }

    renderingState[scanline].bgPalettesModified = bgPalettesModified;
    if (bgPalettesModified) {
        bgPalettesModified = false;
        renderingState[scanline].bgPal = ioRam[0x47];

        // Hash is helpful for more-or-less static screens using tons of colours.
        int hash=0;
        for (int i=0; i<0x10; i++) {
            hash = ((hash << 5) + hash) + ((int*)bgPaletteData)[i];
        }

        if (renderingState[scanline].bgHash != hash ||
                scanline == 0) {    // Just to be safe in case of hash collision

            renderingState[scanline].bgHash = hash;
            for (int i=0; i<0x40; i++) {
                renderingState[scanline].bgPaletteData[i] = bgPaletteData[i];
            }
        }
    }

    renderingState[scanline].sprPalettesModified = sprPalettesModified;
    if (sprPalettesModified) {
        sprPalettesModified = false;
        renderingState[scanline].sprPal[0] = ioRam[0x48];
        renderingState[scanline].sprPal[1] = ioRam[0x49];
        for (int i=0; i<0x40; i++) {
            renderingState[scanline].sprPaletteData[i] = sprPaletteData[i];
        }
    }

    lineModified = false;
}

void writeVram(u16 addr, u8 val) {
    u8 old = vram[vramBank][addr];
    if (old == val)
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
        int tile = addr&0x3ff;
        if (vramBank == 1) {
            if ((val&0x80) && !(old&0x80))
                usingTilePriority[map]++;
            else if (!(val&0x80) && (old&0x80))
                usingTilePriority[map]--;
        }
        if (!changedMap[map][tile]) {
            changedMap[map][tile] = true;
            changedMapQueue[map][changedMapQueueLength[map]++] = tile;
        }
    }
}
void writeVram16(u16 dest, u16 src) {
    bool writingToMapFlags = (vramBank == 1 && dest >= 0x1800);
    bool changed=false;
    u8* page = memory[src>>12];
    int offset = src&0xfff;

    for (int i=0; i<16; i++) {
        u8 val = page[offset++];
        if (vram[vramBank][dest] != val) {
            changed = true;
            if (writingToMapFlags) {
                u8 old = vram[vramBank][dest];
                int map = (dest-0x1800)/0x400;
                if ((val&0x80) && !(old&0x80))
                    usingTilePriority[map]++;
                else if (!(val&0x80) && (old&0x80))
                    usingTilePriority[map]--;
            }
            vram[vramBank][dest] = val;
        }
        dest++;
    }
    if (!changed)
        return;
    dest -= 16;

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
        int map = (dest-0x1800)/0x400;
        for (int i=0; i<16; i++) {
            int tile = (dest+i)&0x3ff;
            if (!changedMap[map][tile]) {
                changedMap[map][tile] = true;
                changedMapQueue[map][changedMapQueueLength[map]++] = tile;
            }
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
        int id = (dmgPal>>(i*2))&3;

        BG_PALETTE[((paletteid)*16)+i+1] = data[(paletteid*8)+(id*2)] | data[(paletteid*8)+(id*2)+1]<<8;
    }
}

void updateBgPalette_GBC(int paletteid, u8* data) {
    u16* dest = BG_PALETTE+paletteid*16+1;
    u16* src = ((u16*)data)+paletteid*4;
    for (int i=0; i<4; i++)
        *(dest++) = *(src++);
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
            if ((val & 0x7B) != (ioRam[0x40] & 0x7B)) {
                lineModified = true;
                mapsModified = true;
            }
            if ((val&4) != (ioRam[0x40]&4)) {
                lineModified = true;
                spritesModified = true;
            }
            ioRam[0x40] = val;
            return;
        case 0x46:				// DMA
            {
                int src = val << 8;
                u8* mem = memory[src>>12];
                src &= 0xfff;
                for (int i=0; i<0xA0; i++) {
                    u8 val = mem[src++];
                    hram[i] = val;
                }
                lineModified = true;
                spritesModified = true;

                ioRam[ioReg] = val;
                //printLog("dma write %d\n", ioRam[0x44]);
                return;
            }
        case 0x42:
        case 0x43:
            if (val != ioRam[ioReg]) {
                ioRam[ioReg] = val;
                lineModified = true;
                mapsModified = true;
            }
            break;
        case 0x4B: // winX
            if (val != ioRam[ioReg]) {
                ioRam[ioReg] = val;
            }
            break;
        case 0x4A: // winY
            if (ioRam[0x44] >= 144 || val > ioRam[0x44])
                winPosY = -1;
            else {
                // Signal that winPosY must be reset according to winY
                winPosY = -2;
            }
            lineModified = true;
            mapsModified = true;
            ioRam[ioReg] = val;
            break;
        case 0x47:				// BG Palette (GB classic only)
            if (gbMode == GB && ioRam[0x47] != val) {
                lineModified = true;
                bgPalettesModified = true;
            }
            ioRam[0x47] = val;
            return;
        case 0x48:				// Spr Palette (GB classic only)
            if (gbMode == GB && ioRam[0x48] != val) {
                lineModified = true;
                sprPalettesModified = true;
            }
            ioRam[0x48] = val;
            return;
        case 0x49:				// Spr Palette (GB classic only)
            if (gbMode == GB && ioRam[0x49] != val) {
                lineModified = true;
                sprPalettesModified = true;
            }
            ioRam[0x49] = val;
            return;
        case 0x69:				// BG Palette Data (GBC only)
            {
                int index = ioRam[0x68] & 0x3F;
                if (bgPaletteData[index] != val) {
                    bgPaletteData[index] = val;
                    lineModified = true;
                    bgPalettesModified = true;
                }

                if (ioRam[0x68] & 0x80)
                    ioRam[0x68] = 0x80 | (ioRam[0x68]+1);
                ioRam[0x69] = bgPaletteData[ioRam[0x68]&0x3F];
                return;
            }
        case 0x6B:				// Sprite Palette Data (GBC only)
            {
                int index = ioRam[0x6A] & 0x3F;
                if (sprPaletteData[index] != val) {
                    sprPaletteData[index] = val;
                    lineModified = true;
                    sprPalettesModified = true;
                }

                if (ioRam[0x6A] & 0x80)
                    ioRam[0x6A] = 0x80 | (ioRam[0x6A]+1);
                ioRam[0x6B] = sprPaletteData[ioRam[0x6A]&0x3F];
                return;
            }
        default:
            ioRam[ioReg] = val;
    }
}

void writeHram(u16 addr, u8 val) {
    hram[addr&0x1ff] = val;
    lineModified = true;
    spritesModified = true;
}

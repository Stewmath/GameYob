#include "sgb.h"
#include "main.h"
#include "mmu.h"
#include "gbgfx.h"
#include "gbcpu.h"

int sgbPacketLength; // Number of packets to be transferred this command
int sgbPacketsTransferred; // Number of packets which have been transferred so far
int sgbPacketBit; // Next bit # to be sent in the packet. -1 if no packet is being transferred.
u8 sgbPacket[16];
u8 sgbCommand;

u8* sgbPalettes = vram[1]; // Borrow vram bank 1. We don't need it in sgb mode.
u8* sgbAttrFiles = vram[1]+0x1000;
u8 sgbMap[20*18];

u8 numControllers;
u8 selectedController;
u8 buttonsChecked;

// Data for various different commands
struct CmdData {
    int numDataSets;
    
    union {
        struct {
            u8 data[6];
            u8 dataBytes;
        } attrBlock;

        struct {
            u8 writeStyle;
            u8 x,y;
        } attrChr;
    };
} cmdData;

void initSGB() {
    sgbPacketLength=0;
    numControllers=1;
    selectedController=0;
    sgbPacketBit = -1;
    sgbPacketsTransferred = 0;
    buttonsChecked = 0;

    memset(sgbMap, 0, 20*18);
}

void doVramTransfer(u8* dest) {
    int map = 0x1800+((ioRam[0x40]>>3)&1)*0x400;
    int index=0;
    for (int y=0; y<18; y++) {
        for (int x=0; x<20; x++) {
            if (index == 0x1000)
                return;
            int tile = vram[0][map+y*32+x];
            if (ioRam[0x40] & 0x10)
                memcpy(dest+index, vram[0]+tile*16, 16);
            else
                memcpy(dest+index, vram[0]+0x1000+((s8)tile)*16, 16);
            index += 16;
        }
    }
}

void setBackdrop(u16 val) {
    if (loadedBorderType == BORDER_SGB)
        BG_PALETTE[0] = val;
    for (int i=0; i<4; i++) {
        bgPaletteData[i*8] = val&0xff;
        bgPaletteData[i*8+1] = val>>8;
        sprPaletteData[i*8] = val&0xff;
        sprPaletteData[i*8+1] = val>>8;
        sprPaletteData[(i+4)*8] = val&0xff;
        sprPaletteData[(i+4)*8+1] = val>>8;
    }
}

void sgbLoadAttrFile(int index) {
    if (index > 0x2c) {
        printLog("Bad Attr %x\n", index);
        return;
    }
    printLog("Load Attr %x\n", index);
    int src = index*90;
    int dest=0;
    for (int i=0; i<20*18/4; i++) {
        sgbMap[dest++] = (sgbAttrFiles[src]>>6)&3;
        sgbMap[dest++] = (sgbAttrFiles[src]>>4)&3;
        sgbMap[dest++] = (sgbAttrFiles[src]>>2)&3;
        sgbMap[dest++] = (sgbAttrFiles[src]>>0)&3;
        src++;
    }
}

void sgbPalXX(int block) {
    int s1,s2;
    switch(sgbCommand) {
        case 0:
            s1 = 0;
            s2 = 1;
            break;
        case 1:
            s1 = 2;
            s2 = 3;
            break;
        case 2:
            s1 = 0;
            s2 = 3;
            break;
        case 3:
            s1 = 1;
            s2 = 2;
            break;
        default:
            return;
    }
    memcpy(bgPaletteData+s1*8+2, sgbPacket+3, 6);
    memcpy(sprPaletteData+s1*8+2, sgbPacket+3, 6);

    memcpy(bgPaletteData+s2*8+2, sgbPacket+9, 6);
    memcpy(sprPaletteData+s2*8+2, sgbPacket+9, 6);

    setBackdrop(sgbPacket[1] | sgbPacket[2]<<8);
}

void sgbAttrBlock(int block) {
    int pos;
    if (block == 0) {
        cmdData.attrBlock.dataBytes=0;
        cmdData.numDataSets = sgbPacket[1];
        pos = 2;
    }
    else
        pos = 0;

    u8* const data = cmdData.attrBlock.data;
    while (pos < 16 && cmdData.numDataSets > 0) {
        for (; cmdData.attrBlock.dataBytes < 6 && pos < 16; cmdData.attrBlock.dataBytes++, pos++) {
            data[cmdData.attrBlock.dataBytes] = sgbPacket[pos];
        }
        if (cmdData.attrBlock.dataBytes == 6) {
            int pIn = data[1]&3;
            int pLine = (data[1]>>2)&3;
            int pOut = (data[1]>>4)&3;
            int x1=data[2];
            int y1=data[3];
            int x2=data[4];
            int y2=data[5];
            bool changeLine = data[0] & 2;
            if (!changeLine) {
                if ((data[0] & 7) == 1) {
                    changeLine = true;
                    pLine = pIn;
                }
                else if ((data[0] & 7) == 4) {
                    changeLine = true;
                    pLine = pOut;
                }
            }

            if (data[0] & 1) { // Inside block
                for (int x=x1+1; x<x2; x++) {
                    for (int y=y1+1; y<y2; y++) {
                        sgbMap[y*20+x] = pIn;
                    }
                }
            }
            if (data[0] & 4) { // Outside block
                for (int x=0; x<20; x++) {
                    if (x < x1 || x > x2) {
                        for (int y=0; y<18; y++) {
                            if (y < y1 || y > y2) {
                                sgbMap[y*20+x] = pOut;
                            }
                        }
                    }
                }
            }
            if (changeLine) { // Line surrounding block
                for (int x=x1; x<=x2; x++) {
                    sgbMap[y1*20+x] = pLine;
                    sgbMap[y2*20+x] = pLine;
                }
                for (int y=y1; y<=y2; y++) {
                    sgbMap[y*20+x1] = pLine;
                    sgbMap[y*20+x2] = pLine;
                }
            }

            cmdData.attrBlock.dataBytes = 0;
            cmdData.numDataSets--;
        }
    }
}

void sgbAttrLin(int block) {
    int index = 0;
    if (block == 0) {
        cmdData.numDataSets = sgbPacket[1];
        index = 2;
    }
    while (cmdData.numDataSets > 0 && index < 16) {
        u8 dat = sgbPacket[index++];
        cmdData.numDataSets--;

        int line = dat&0x1f;
        int pal = (dat>>5)&3;

        if (dat&0x80) { // Horizontal
            for (int i=0; i<20; i++) {
                sgbMap[i+line*20] = pal;
            }
        }
        else { // Vertical
            for (int i=0; i<18; i++) {
                sgbMap[line+i*20] = pal;
            }
        }
    }
}

void sgbAttrDiv(int block) {
    int p0 = (sgbPacket[1]>>2)&3;
    int p1 = (sgbPacket[1]>>4)&3;
    int p2 = (sgbPacket[1]>>0)&3;

    if (sgbPacket[1]&0x40) {
        for (int y=0; y<sgbPacket[2] && y<18; y++) {
            for (int x=0; x<20; x++)
                sgbMap[y*20+x] = p0;
        }
        if (sgbPacket[2] < 18) {
            for (int x=0; x<20; x++)
                sgbMap[sgbPacket[2]*20+x] = p1;
            for (int y=sgbPacket[2]+1; y<18; y++) {
                for (int x=0; x<20; x++)
                    sgbMap[y*20+x] = p2;
            }
        }
    }
    else {
        for (int x=0; x<sgbPacket[2] && x<20; x++) {
            for (int y=0; y<18; y++)
                sgbMap[y*20+x] = p0;
        }
        if (sgbPacket[2] < 20) {
            for (int y=0; y<18; y++)
                sgbMap[y*20+sgbPacket[2]] = p1;
            for (int x=sgbPacket[2]+1; x<20; x++) {
                for (int y=0; y<18; y++)
                    sgbMap[y*20+x] = p2;
            }
        }
    }
}

void sgbAttrChr(int block) {
    u8& x = cmdData.attrChr.x;
    u8& y = cmdData.attrChr.y;

    int index = 0;
    if (block == 0) {
        cmdData.numDataSets = sgbPacket[3] | (sgbPacket[4]<<8);
        cmdData.attrChr.writeStyle = sgbPacket[5]&1;
        x = (sgbPacket[1] >= 20 ? 19 : sgbPacket[1]);
        y = (sgbPacket[2] >= 18 ? 17 : sgbPacket[2]);

        index = 6*4;
    }

    while (cmdData.numDataSets != 0 && index < 16*4) {
        sgbMap[x+y*20] = (sgbPacket[index/4]>>(6-(index&3)*2))&3;
        if (cmdData.attrChr.writeStyle == 0) {
            x++;
            if (x == 20) {
                x = 0;
                y++;
                if (y == 18)
                    y = 0;
            }
        }
        else {
            y++;
            if (y == 18) {
                y = 0;
                x++;
                if (x == 20)
                    x = 0;
            }
        }
        index++;
        cmdData.numDataSets--;
    }
}

void sgbPalSet(int block) {
    for (int i=0; i<4; i++) {
        int paletteid = (sgbPacket[i*2+1] | (sgbPacket[i*2+2]<<8))&0x1ff;
        //printLog("%d: %x\n", i, paletteid);
        memcpy(bgPaletteData+i*8+2, sgbPalettes + paletteid*8+2, 6);
        memcpy(sprPaletteData+i*8+2, sgbPalettes + paletteid*8+2, 6);
    }
    int color0Paletteid = (sgbPacket[1] | (sgbPacket[2]<<8))&0x1ff;
    setBackdrop(sgbPalettes[color0Paletteid*8] | (sgbPalettes[color0Paletteid*8+1]<<8));

    if (sgbPacket[9]&0x80) {
        sgbLoadAttrFile(sgbPacket[9]&0x3f);
    }
    if (sgbPacket[9]&0x40)
        setGFXMask(0);
}
void sgbPalTrn(int block) {
    doVramTransfer(sgbPalettes);
}

void sgbDataSnd(int block) {
    //printLog("SND %.2x -> %.2x:%.2x%.2x\n", sgbPacket[4], sgbPacket[3], sgbPacket[2], sgbPacket[1]);
}

void sgbMltReq(int block) {
    numControllers = (sgbPacket[1]&3)+1;
    if (numControllers > 1)
        selectedController = 1;
    else
        selectedController = 0;
}

void sgbChrTrn(int blonk) {
    u8* data = (u8*)malloc(0x1000);
    doVramTransfer(data);
    setSgbTiles(data, sgbPacket[1]);
    free(data);
}

void sgbPctTrn(int block) {
    u8* data = (u8*)malloc(0x1000);
    doVramTransfer(data);
    setSgbMap(data);
    free(data);
}

void sgbAttrTrn(int block) {
    doVramTransfer(sgbAttrFiles);
}

void sgbAttrSet(int block) {
    sgbLoadAttrFile(sgbPacket[1]&0x3f);
    if (sgbPacket[1]&0x40)
        setGFXMask(0);
}

void sgbMask(int block) {
    setGFXMask(sgbPacket[1]&3);
}

void (*sgbCommands[])(int) = {
    sgbPalXX,sgbPalXX,sgbPalXX,sgbPalXX,sgbAttrBlock,sgbAttrLin,sgbAttrDiv,sgbAttrChr,
    NULL,NULL,sgbPalSet,sgbPalTrn,NULL,NULL,NULL,sgbDataSnd,
    NULL,sgbMltReq,NULL,sgbChrTrn,sgbPctTrn,sgbAttrTrn,sgbAttrSet,sgbMask,
    NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL
};

void sgbHandleP1(u8 val) {
    if ((val&0x30) == 0) {
        // Start packet transfer
        sgbPacketBit = 0;
        ioRam[0x00] = 0xcf;
        return;
    }
    if (sgbPacketBit != -1) {
        u8 oldVal = ioRam[0x00];
        ioRam[0x00] = val;

        int shift = sgbPacketBit%8;
        int byte = (sgbPacketBit/8)%16;
        if (shift == 0)
            sgbPacket[byte] = 0;

        int bit;
        if ((oldVal & 0x30) == 0 && (val & 0x30) != 0x30) { // A bit of speculation here. Fixes castlevania.
            sgbPacketBit = -1;
            return;
        }
        if (!(val & 0x10))
            bit = 0;
        else if (!(val & 0x20))
            bit = 1;
        else
            return;

        sgbPacket[byte] |= bit<<shift;
        sgbPacketBit++;
        if (sgbPacketBit == 128) {
            if (sgbPacketsTransferred == 0) {
                sgbCommand = sgbPacket[0]/8;
                sgbPacketLength = sgbPacket[0]&7;
                printLog("CMD %x\n", sgbCommand);
            }
            if (sgbCommands[sgbCommand] != 0)
                sgbCommands[sgbCommand](sgbPacketsTransferred);

            sgbPacketBit = -1;
            sgbPacketsTransferred++;
            if (sgbPacketsTransferred == sgbPacketLength) {
                sgbPacketLength = 0;
                sgbPacketsTransferred = 0;
            }
        }
    }
    else {
        if ((val&0x30) == 0x30) {
            if (buttonsChecked == 3) {
                selectedController++;
                if (selectedController >= numControllers)
                    selectedController = 0;
                buttonsChecked = 0;
            }
            ioRam[0x00] = 0xff - selectedController;
        }
        else {
            ioRam[0x00] = val|0xcf;
            if ((val&0x30) == 0x10)
                buttonsChecked |= 1;
            else if ((val&0x30) == 0x20)
                buttonsChecked |= 2;
        }
    }
}

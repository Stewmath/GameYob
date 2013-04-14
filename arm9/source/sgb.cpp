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

int numControllers;
int selectedController;

u8 blockData[6];
u8 blockDataBytes;
int numDataSets;

void initSGB() {
    sgbPacketLength=0;
    numControllers=1;
    selectedController=0;
    sgbPacketBit = -1;
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
    memcpy(bgPaletteData+s1*8, sgbPacket+1, 8);
    memcpy(sprPaletteData+s1*8, sgbPacket+1, 8);

    memcpy(bgPaletteData+s2*8, sgbPacket+1, 2);
    memcpy(sprPaletteData+s2*8, sgbPacket+1, 2);
    memcpy(bgPaletteData+s2*8+2, sgbPacket+9, 6);
    memcpy(sprPaletteData+s2*8+2, sgbPacket+9, 6);
}

void sgbAttrBlock(int block) {
    int pos;
    if (block == 0) {
        blockDataBytes=0;
        numDataSets = sgbPacket[1];
        pos = 2;
    }
    else
        pos = 0;

    while (pos < 16 && numDataSets > 0) {
        for (; blockDataBytes < 6 && pos < 16; blockDataBytes++, pos++) {
            blockData[blockDataBytes] = sgbPacket[pos];
        }
        if (blockDataBytes == 6) {
            printLog("Block ");
            if (blockData[0]&1)
                printLog("INSIDE ");
            if (blockData[0]&2)
                printLog("LINE ");
            if (blockData[0]&4)
                printLog("OUTSIDE");
            printLog("\n");
            int palette = blockData[1]&3;
            int x1=blockData[2];
            int y1=blockData[3];
            int x2=blockData[4];
            int y2=blockData[5];

            for (int x=x1; x<=x2; x++) {
                for (int y=y1; y<=y2; y++) {
                    sgbMap[y*20+x] = palette;
                }
            }

            blockDataBytes = 0;
            numDataSets--;
        }
    }
}

void sgbPalSet(int block) {
    for (int i=0; i<4; i++) {
        int paletteid = (sgbPacket[i*2+1] | (sgbPacket[i*2+2]<<8));
        //printLog("%d: %x\n", i, paletteid);
        memcpy(bgPaletteData+i*8, sgbPalettes + (sgbPacket[i*2+1] | (sgbPacket[i*2+2]<<8))*8, 8);
        memcpy(sprPaletteData+i*8, sgbPalettes + (sgbPacket[i*2+1] | sgbPacket[i*2+2]<<8)*8, 8);
    }
    if (sgbPacket[9]&0x80) {
        sgbLoadAttrFile(sgbPacket[9]&0x3f);
    }
}
void sgbPalTrn(int block) {
    memcpy(sgbPalettes, vram[0]+0x800, 0x1000);
}

void sgbMltReq(int block) {
    numControllers = (sgbPacket[1]&3)+1;
    if (numControllers > 1)
        selectedController = 1;
    else
        selectedController = 0;
}

void sgbAttrTrn(int block) {
    printLog("Attr TRN\n");
    memcpy(sgbAttrFiles, vram[0]+0x800, 0xfd2);
}

void sgbAttrSet(int block) {
    sgbLoadAttrFile(sgbPacket[1]&0x3f);
}

void (*sgbCommands[])(int) = {
    sgbPalXX,sgbPalXX,sgbPalXX,sgbPalXX,sgbAttrBlock,NULL,NULL,NULL,
    NULL,NULL,sgbPalSet,sgbPalTrn,NULL,NULL,NULL,NULL,
    NULL,sgbMltReq,NULL,NULL,NULL,sgbAttrTrn,sgbAttrSet,NULL,
    NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL
};

void sgbHandleP1(u8 val) {
    if (sgbPacketBit != -1) {
        int shift = sgbPacketBit%8;
        int byte = (sgbPacketBit/8)%16;
        if (shift == 0)
            sgbPacket[byte] = 0;

        int bit;
        if (!(val & 0x10))
            bit = 0;
        else if (!(val & 0x20))
            bit = 1;
        else
            return;

        sgbPacket[byte] |= bit<<shift;
        sgbPacketBit++;
        if (sgbPacketLength == 0 && sgbPacketBit == 8) {
            sgbCommand = sgbPacket[0]/8;
            sgbPacketLength = sgbPacket[0]&7;
            sgbPacketsTransferred = 0;
            printLog("CMD %x\n", sgbCommand);
        }
        if (sgbPacketBit == 128) {
            if (sgbCommands[sgbCommand] != 0)
                sgbCommands[sgbCommand](sgbPacketsTransferred);
            /*
            if (sgbCommand == 4) {
                for (int i=0; i<16; i++)
                    printLog("%.2x ", sgbPacket[i]);
                printLog("\n");
            }
            */
            sgbPacketBit = -1;
            sgbPacketsTransferred++;
            if (sgbPacketsTransferred == sgbPacketLength) {
                sgbPacketLength = 0;
            }
        }
    }
    else {
        if ((val&0x30) == 0) {
            sgbPacketBit = 0;
            ioRam[0x00] = 0xcf;
        }
        else if ((val&0x30) == 0x30) {
            selectedController++;
            if (selectedController >= numControllers)
                selectedController = 0;
            ioRam[0x00] = 0xff - selectedController;
            //printLog("0x00 = %.2x\n", ioRam[0x00]);
        }
        else
            ioRam[0x00] = val;
    }
}

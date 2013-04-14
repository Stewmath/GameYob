#include "sgb.h"
#include "main.h"
#include "mmu.h"
#include "gbgfx.h"
#include "gbcpu.h"

int sgbPacketLength; // Length of packet to be transferred (in bits)
int sgbPacketBit; // Bit # currently being sent
int sgbPacketsTransferred;
u8 sgbPacket[16];
u8 sgbCommand;

u8* sgbPalettes = vram[1]; // Borrow vram bank 1. We don't need it in sgb mode.
u8* sgbMap = vram[1]+0x1000;

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
    memcpy(bgPaletteData+s2*8+2, sgbPacket+9, 8);
    memcpy(sprPaletteData+s2*8+2, sgbPacket+9, 8);
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
            int palette = blockData[1]&3;
            int x1=blockData[2];
            int y1=blockData[3];
            int x2=blockData[4];
            int y2=blockData[5];

            for (int i=0; i<6; i++)
                printLog("%.2x ", blockData[i]);
            printLog("\n");
            for (int x=x1; x<=x2; x++) {
                for (int y=y1; y<=y2; y++) {
                    vram[1][0x1000+y*20+x] = palette;
                }
            }

            blockDataBytes = 0;
            numDataSets--;
        }
    }
}

void sgbPalSet(int block) {
    printLog("SET\n");
    for (int i=0; i<4; i++) {
        int paletteid = (sgbPacket[i*2+1] | (sgbPacket[i*2+2]<<8));
        printLog("%d: %x\n", i, paletteid);
        memcpy(bgPaletteData+i*8, sgbPalettes + (sgbPacket[i*2+1] | (sgbPacket[i*2+2]<<8))*8, 8);
        memcpy(sprPaletteData+i*8, sgbPalettes + (sgbPacket[i*2+1] | sgbPacket[i*2+2]<<8)*8, 8);
    }
}
void sgbPalTrn(int block) {
    printLog("TRN\n");
    memcpy(sgbPalettes, vram[0]+0x800, 0x1000);
}

void sgbMltReq(int block) {
    numControllers = (sgbPacket[1]&3)+1;
    if (numControllers > 1)
        selectedController = 1;
    else
        selectedController = 0;
}

void (*sgbCommands[])(int) = {
    sgbPalXX,sgbPalXX,sgbPalXX,sgbPalXX,sgbAttrBlock,NULL,NULL,NULL,
    NULL,NULL,sgbPalSet,sgbPalTrn,NULL,NULL,NULL,NULL,
    NULL,sgbMltReq,NULL,NULL,NULL,NULL,NULL,NULL,
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

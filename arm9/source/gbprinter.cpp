#include "gbprinter.h"
#include "nifi.h"
#include "console.h"
#include <fat.h>

#define PRINTER_STATUS_READY        0x08
#define PRINTER_STATUS_REQUESTED    0x04
#define PRINTER_STATUS_PRINTING     0x02
#define PRINTER_STATUS_CHECKSUM     0x01

#define PRINTER_WIDTH 160 // in pixels
#define PRINTER_HEIGHT 200

u8 printerGfx[PRINTER_WIDTH*PRINTER_HEIGHT/4];
int printerGfxIndex;

int printerPacketByte;
u8 printerStatus;
u8 printerCmd;
u16 printerCmdLength;

bool printerPacketCompressed;
u8 printerCompressionByte;
u8 printerCompressionLen;

u16 printerExpectedChecksum;
u16 printerChecksum;

u8 cmd2Index;
u8 printerPalette;

void printerSaveFile();

void initGbPrinter() {
    printerPacketByte = 0;
    printerChecksum = 0;
    printerStatus = 0;
    printerGfxIndex = 0;
    cmd2Index = 0;
    memset(printerGfx, 0, sizeof(printerGfx));
}

void printerSendVariableLenData(u8 dat) {
    switch(printerCmd) {

        case 0x2: // Print
            switch(cmd2Index) {
                case 0: // Unknown (0x01)
                    break;
                case 1: // Margins (ignored)
                    break;
                case 2: // Palette
                    printerPalette = dat;
                    break;
                case 3: // Exposure / brightness (ignored)
                    break;
            }
            cmd2Index++;
            break;

        case 0x4: // Fill buffer
            if (printerGfxIndex < PRINTER_WIDTH*PRINTER_HEIGHT/4) {
                printerGfx[printerGfxIndex++] = dat;
                if (printerGfxIndex >= 0x280)
                    printerStatus |= PRINTER_STATUS_READY;
            }
            break;
    }
}

void sendGbPrinterByte(u8 dat) {
    //printLog("Byte %d = %x\n", printerPacketByte, dat);

    // "Byte" 6 is actually a number of bytes. The counter stays at 6 until the 
    // required number of bytes have been read.
    if (printerPacketByte == 6 && printerCmdLength == 0)
        printerPacketByte++;

    // Checksum: don't count the magic bytes or checksum bytes
    if (printerPacketByte != 0 && printerPacketByte != 1 && printerPacketByte != 7 && printerPacketByte != 8)
        printerChecksum += dat;

    switch(printerPacketByte) {
        case 0: // Magic byte
            linkReceivedData = 0x00;
            if (dat != 0x88)
                goto endPacket;
            break;
        case 1: // Magic byte
            linkReceivedData = 0x00;
            if (dat != 0x33)
                goto endPacket;
            break;
        case 2: // Command
            linkReceivedData = 0x00;
            printerCmd = dat;
            break;
        case 3: // Compression flag
            linkReceivedData = 0x00;
            printerPacketCompressed = dat;
            if (printerPacketCompressed)
                printerCompressionLen = 0;
            break;
        case 4: // Length (LSB)
            linkReceivedData = 0x00;
            printerCmdLength = dat;
            break;
        case 5: // Length (MSB)
            linkReceivedData = 0x00;
            printerCmdLength |= dat<<8;
            break;

        case 6: // variable-length data
            linkReceivedData = 0x00;

            if (!printerPacketCompressed) {
                printerSendVariableLenData(dat);
            }
            else {
                // Handle RLE compression
                if (printerCompressionLen == 0) {
                    printerCompressionByte = dat;
                    printerCompressionLen = (dat&0x7f)+1;
                    if (printerCompressionByte & 0x80)
                        printerCompressionLen++;
                }
                else {
                    if (printerCompressionByte & 0x80) {
                        while (printerCompressionLen != 0) {
                            printerSendVariableLenData(dat);
                            printerCompressionLen--;
                        }
                    }
                    else {
                        printerSendVariableLenData(dat);
                        printerCompressionLen--;
                    }
                }
            }
            printerCmdLength--;
            return; // printerPacketByte won't be incremented

        case 7: // Checksum (LSB)
            linkReceivedData = 0x00;
            printerExpectedChecksum = dat;
            break;
        case 8: // Checksum (MSB)
            linkReceivedData = 0x00;
            printerExpectedChecksum |= dat<<8;
            break;

        case 9: // Alive indicator
            linkReceivedData = 0x81;
            break;

        case 10: // Status
            if (printerChecksum != printerExpectedChecksum) {
                printerStatus |= PRINTER_STATUS_CHECKSUM;
                printLog("Checksum %.4x, expected %.4x\n", printerChecksum, printerExpectedChecksum);
            }
            else
                printerStatus &= ~PRINTER_STATUS_CHECKSUM;

            switch(printerCmd) {
                case 1: // Initialize
                    initGbPrinter();
                    break;
                case 2: // Start printing
                    printerStatus &= ~PRINTER_STATUS_READY;
                    printerStatus |= PRINTER_STATUS_REQUESTED;
                    printerSaveFile();
                    break;
                case 4: // Fill buffer
                    // Data has been read, nothing more to do
                    break;
            }

            linkReceivedData = printerStatus;
            goto endPacket;
    }
    printerPacketByte++;
    return;

endPacket:
    printerPacketByte = 0;
    printerChecksum = 0;
    cmd2Index = 0;
    return;
}

inline void WRITE_32(u8* ptr, u32 x) {
    *ptr = x&0xff;
    *(ptr+1) = (x>>8)&0xff;
    *(ptr+2) = (x>>16)&0xff;
    *(ptr+3) = (x>>24)&0xff;
}

u8 bmpHeader[] = { // Contains header data & palettes
0x42, 0x4d, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46, 0x00, 0x00, 0x00, 0x28, 0x00,
0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x04, 0x00, 0x00, 0x00,
0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x12, 0x0b, 0x00, 0x00, 0x12, 0x0b, 0x00, 0x00, 0x04, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x55, 0x55, 0x55, 0x55, 0xaa, 0xaa,
0xaa, 0xaa, 0xff, 0xff, 0xff, 0xff
};
// BMP notes:
// File size at 0x02 and 0x22 must be set.
// 0x12 = width
// 0x16 = height
// 0x1c = color depth
// 0x2e is number of colors (leave at 0)
int numPrinted = 0;
void printerSaveFile() {
    int width = PRINTER_WIDTH;

    if (printerGfxIndex%(width/4*16) != 0)
        printerGfxIndex += (width/4*16)-(printerGfxIndex%(width/4*16));

    int height = printerGfxIndex / width * 4;
    int pixelArraySize = (width*height+1)/2;

    printLog("%.4x bytes\n", printerGfxIndex);

    // Set up the palette
    for (int i=0; i<4; i++) {
        u8 rgb;
        switch((printerPalette>>(i*2))&3) {
            case 0:
                rgb = 0xff;
                break;
            case 1:
                rgb = 0xaa;
                break;
            case 2:
                rgb = 0x55;
                break;
            case 3:
                rgb = 0x00;
                break;
        }
        for (int j=0; j<4; j++)
            bmpHeader[0x36+i*4+j] = rgb;
    }

    u16* pixelData = (u16*)malloc(pixelArraySize);
    if (pixelData == NULL) {
        printLog("VERY BAD\n");
        while(1);
    }

    printLog("Max = %x\n", width*height/2);
    for (int i=0; i<printerGfxIndex; i+=2) {
        u8 b1 = printerGfx[i];
        u8 b2 = printerGfx[i+1];

        int pixel = i*4;
        int tile = pixel/64;
        int metaRow = tile/40;

        int index = metaRow*width*16;
        index = tile/20*width*8;
        index += (tile%20)*8;
        index += ((pixel%64)/8)*width;
        index += (pixel%8);
        index /= 4;

        if (index >= width*height/4) {
            printLog("Over: %x -> %x\n", i, index);
            while(1);
        }
        pixelData[index] = 0;
        pixelData[index+1] = 0;
        for (int j=0; j<2; j++) {
            pixelData[index] |= (((b1>>j>>4)&1) | (((b2>>j>>4)&1)<<1))<<(j*4+8);
            pixelData[index] |= (((b1>>j>>6)&1) | (((b2>>j>>6)&1)<<1))<<(j*4);
            pixelData[index+1] |= (((b1>>j)&1) | (((b2>>j)&1)<<1))<<(j*4+8);
            pixelData[index+1] |= (((b1>>j>>2)&1) | (((b2>>j>>2)&1)<<1))<<(j*4);
        }
    }

    char fileString[30];
    siprintf(fileString, "/printed%d.bmp", numPrinted);
    FILE* file = fopen(fileString, "w");

    WRITE_32(bmpHeader+2, sizeof(bmpHeader) + pixelArraySize);
    WRITE_32(bmpHeader+0x22, pixelArraySize);
    WRITE_32(bmpHeader+0x12, width);
    WRITE_32(bmpHeader+0x16, -height);

    fwrite(bmpHeader, 1, sizeof(bmpHeader), file);
    fwrite(pixelData, 1, pixelArraySize, file);

    fclose(file);

    free(pixelData);
    printerGfxIndex = 0;
    numPrinted++;
}

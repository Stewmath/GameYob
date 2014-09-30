#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "gbprinter.h"
#include "gameboy.h"
#include "gbgfx.h"
#include "inputhelper.h"
#include "nifi.h"
#include "console.h"
#include "romfile.h"

#define PRINTER_STATUS_READY        0x08
#define PRINTER_STATUS_REQUESTED    0x04
#define PRINTER_STATUS_PRINTING     0x02
#define PRINTER_STATUS_CHECKSUM     0x01

#define PRINTER_WIDTH 160
#define PRINTER_HEIGHT 208 // The actual value is 200, but 16 divides 208.

// Local variables

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

int printerMargins;
int lastPrinterMargins; // it's an int so that it can have a "nonexistant" value ("never set").
u8 printerCmd2Index;
u8 printerPalette;
u8 printerExposure; // Ignored

int numPrinted; // Corresponds to the number after the filename

int printCounter=0; // Timer until the printer "stops printing".

// Local functions
void resetGbPrinter();
void printerSendVariableLenData();
void printerSaveFile();


// Called along with other initialization routines
void initGbPrinter() {
    printerPacketByte = 0;
    printerChecksum = 0;
    printerCmd2Index = 0;

    printerMargins = -1;
    lastPrinterMargins = -1;

    numPrinted = 0;

    resetGbPrinter();
}

// Can be invoked by the game (command 1)
void resetGbPrinter() {
    printerStatus = 0;
    printerGfxIndex = 0;
    memset(printerGfx, 0, sizeof(printerGfx));
    printCounter = 0;
}


void printerSendVariableLenData(u8 dat) {
    switch(printerCmd) {

        case 0x2: // Print
            switch(printerCmd2Index) {
                case 0: // Unknown (0x01)
                    break;
                case 1: // Margins
                    lastPrinterMargins = printerMargins;
                    printerMargins = dat;
                    break;
                case 2: // Palette
                    printerPalette = dat;
                    break;
                case 3: // Exposure / brightness
                    printerExposure = dat;
                    break;
            }
            printerCmd2Index++;
            break;

        case 0x4: // Fill buffer
            if (printerGfxIndex < PRINTER_WIDTH*PRINTER_HEIGHT/4) {
                printerGfx[printerGfxIndex++] = dat;
            }
            break;
    }
}

u8 sendGbPrinterByte(u8 dat) {
    u8 linkReceivedData = 0x00;
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
            return linkReceivedData; // printerPacketByte won't be incremented

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
                    resetGbPrinter();
                    break;
                case 2: // Start printing (after a short delay)
                    printCounter = 1;
                    break;
                case 4: // Fill buffer
                    // Data has been read, nothing more to do
                    break;
            }

            linkReceivedData = printerStatus;

            // The received value apparently shouldn't contain this until next 
            // packet.
            if (printerGfxIndex >= 0x280)
                printerStatus |= PRINTER_STATUS_READY;

            goto endPacket;
    }

    printerPacketByte++;
    return linkReceivedData;

endPacket:
    printerPacketByte = 0;
    printerChecksum = 0;
    printerCmd2Index = 0;
    return linkReceivedData;
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

// Save the image as a 4bpp bitmap
void printerSaveFile() {
    displayIcon(ICON_PRINTER);

    // if "appending" is true, this image will be slapped onto the old one.
    // Some games have a tendency to print an image in multiple goes.
    bool appending = false;
    if (lastPrinterMargins != -1 &&
            (lastPrinterMargins&0x0f) == 0 &&   // Last printed item left 0 space after
            (printerMargins&0xf0)     == 0) {   // This item leaves 0 space before
        appending = true;
    }

    // Find the first available "print number".
    char filename[300];
    while (true) {
        printf(filename, "%s-%d.bmp", gameboy->getRomFile()->getBasename(), numPrinted);
        if (appending ||                        // If appending, the last file written to is already selected.
                access(filename, R_OK) != 0) {  // Else, if the file doesn't exist, we're done searching.

            if (appending && access(filename, R_OK) != 0) {
                // This is a failsafe, this shouldn't happen
                appending = false;
                printLog("The image to be appended to doesn't exist!");
                continue;
            }
            else
                break;
        }
        numPrinted++;
    }

    int width = PRINTER_WIDTH;

    // In case of error, size must be rounded off to the nearest 16 vertical pixels.
    if (printerGfxIndex%(width/4*16) != 0)
        printerGfxIndex += (width/4*16)-(printerGfxIndex%(width/4*16));

    int height = printerGfxIndex / width * 4;
    int pixelArraySize = (width*height+1)/2;

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

    // Convert the gameboy's tile-based 2bpp into a linear 4bpp format.
    for (int i=0; i<printerGfxIndex; i+=2) {
        u8 b1 = printerGfx[i];
        u8 b2 = printerGfx[i+1];

        int pixel = i*4;
        int tile = pixel/64;

        int index = tile/20*width*8;
        index += (tile%20)*8;
        index += ((pixel%64)/8)*width;
        index += (pixel%8);
        index /= 4;

        pixelData[index] = 0;
        pixelData[index+1] = 0;
        for (int j=0; j<2; j++) {
            pixelData[index] |= (((b1>>j>>4)&1) | (((b2>>j>>4)&1)<<1))<<(j*4+8);
            pixelData[index] |= (((b1>>j>>6)&1) | (((b2>>j>>6)&1)<<1))<<(j*4);
            pixelData[index+1] |= (((b1>>j)&1) | (((b2>>j)&1)<<1))<<(j*4+8);
            pixelData[index+1] |= (((b1>>j>>2)&1) | (((b2>>j>>2)&1)<<1))<<(j*4);
        }
    }

    FileHandle* file;
    if (appending) {
        file = file_open(filename, "r+b");
        int temp;

        // Update height
        file_seek(file, 0x16, SEEK_SET);
        file_read(&temp, 4, 1, file);
        temp = -(height + (-temp));
        file_seek(file, 0x16, SEEK_SET);
        file_write(&temp, 4, 1, file);

        // Update pixelArraySize
        file_seek(file, 0x22, SEEK_SET);
        file_read(&temp, 4, 1, file);
        temp += pixelArraySize;
        file_seek(file, 0x22, SEEK_SET);
        file_write(&temp, 4, 1, file);

        // Update file size
        temp += sizeof(bmpHeader);
        file_seek(file, 0x2, SEEK_SET);
        file_write(&temp, 4, 1, file);

        file_close(file);
        file = file_open(filename, "ab");
    }
    else { // Not appending; making a file from scratch
        file = file_open(filename, "ab");
        WRITE_32(bmpHeader+2, sizeof(bmpHeader) + pixelArraySize);
        WRITE_32(bmpHeader+0x22, pixelArraySize);
        WRITE_32(bmpHeader+0x12, width);
        WRITE_32(bmpHeader+0x16, -height); // negative means it's top-to-bottom
        file_write(bmpHeader, 1, sizeof(bmpHeader), file);
    }

    file_write(pixelData, 1, pixelArraySize, file);

    file_close(file);

    free(pixelData);
    printerGfxIndex = 0;

    printCounter = height; // PRINTER_STATUS_PRINTING will be unset after this many frames
    if (printCounter == 0)
        printCounter = 1;
}

void updateGbPrinter() {
    if (printCounter != 0) {
        printCounter--;
        if (printCounter == 0) {
            if (printerStatus & PRINTER_STATUS_PRINTING) {
                printerStatus &= ~PRINTER_STATUS_PRINTING;
                displayIcon(ICON_NULL); // Disable the printing icon
            }
            else {
                printerStatus |= PRINTER_STATUS_REQUESTED;
                printerStatus |= PRINTER_STATUS_PRINTING;
                printerStatus &= ~PRINTER_STATUS_READY;
                printerSaveFile();
            }
        }
    }
}

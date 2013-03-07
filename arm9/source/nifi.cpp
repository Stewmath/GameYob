#include <nds.h>
#include <dswifi9.h>
#include "nifi.h"
#include "mmu.h"
#include "main.h"
#include "gameboy.h"

volatile int packetData=-1;
volatile int sendData;

void packetHandler(int packetID, int readlength)
{
    static char data[4096];
    static int bytesRead;

    // Wifi_RxRawReadPacket:  Allows user code to read a packet from within the WifiPacketHandler function
    //  long packetID:		a non-unique identifier which locates the packet specified in the internal buffer
    //  long readlength:		number of bytes to read (actually reads (number+1)&~1 bytes)
    //  unsigned short * data:	location for the data to be read into
    bytesRead = Wifi_RxRawReadPacket(packetID, readlength, (unsigned short *)data);

    // Check this is the right kind of packet

    u8 command = data[32];
    u8 val = data[33];
    //packetData = 0;
    switch(command) {
        // Command sent from "external clock"
        case 55:
            serialCounter = clockSpeed/8192;
            packetData = val;
            //ioRam[0x01] = packetData;
            sendPacketByte(56, sendData);
            break;
        // External clock receives a response from internal clock
        case 56:
            //serialCounter = clockSpeed/8192*8;
            packetData = val;
            //ioRam[0x01] = packetData;
            break;
        default:
            break;
    }
    if (command == 55 || command == 56) {
        packetData = val;
        printLog("Received %x\n", packetData);
    }
}



void initNifi()
{
    swiWaitForVBlank();
	Wifi_InitDefault(false);

// Wifi_SetPromiscuousMode: Allows the DS to enter or leave a "promsicuous" mode, in which 
//   all data that can be received is forwarded to the arm9 for user processing.
//   Best used with Wifi_RawSetPacketHandler, to allow user code to use the data
//   (well, the lib won't use 'em, so they're just wasting CPU otherwise.)
//  int enable:  0 to disable promiscuous mode, nonzero to engage
	Wifi_SetPromiscuousMode(1);

// Wifi_EnableWifi: Instructs the ARM7 to go into a basic "active" mode, not actually
//   associated to an AP, but actively receiving and potentially transmitting
	Wifi_EnableWifi();

// Wifi_RawSetPacketHandler: Set a handler to process all raw incoming packets
//  WifiPacketHandler wphfunc:  Pointer to packet handler (see WifiPacketHandler definition for more info)
	Wifi_RawSetPacketHandler(packetHandler);

// Wifi_SetChannel: If the wifi system is not connected or connecting to an access point, instruct
//   the chipset to change channel
//  int channel: the channel to change to, in the range of 1-13
	Wifi_SetChannel(10);
    swiWaitForVBlank();
}

void sendPacketByte(u8 command, u8 data)
{
    unsigned char buffer[2];
    buffer[0] = command;
    buffer[1] = data;
    printLog("Sent %x\n", data);
	if (Wifi_RawTxFrame(2, 0x0014, (unsigned short *)buffer) != 0)
        printLog("Nifi send error\n");
}

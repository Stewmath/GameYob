#include <nds.h>
#include <dswifi9.h>
#include "nifi.h"
#include "mmu.h"
#include "main.h"
#include "gameboy.h"

int packetData=-1;

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
	if (data[32] == 'Y' && data[33] == 'O' && data[34] == 'B') {
        packetData = data[35];
        if (!sentPacket) {
            sentPacket = true;
            sendPacketByte(ioRam[0x01]);
        }
        if (!receivedPacket) {
            receivedPacket = true;
            ioRam[0x01] = data[35];
            ioRam[0x02] &= ~0x80;
            requestInterrupt(SERIAL);
        }
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

void sendPacketByte(u8 data)
{
    unsigned char buffer[4];
    buffer[0] = 'Y';
    buffer[1] = 'O';
    buffer[2] = 'B';
    buffer[3] = data;
	if (Wifi_RawTxFrame(4, 0x0014, (unsigned short *)buffer) != 0)
        printLog("Nifi send error\n");
}

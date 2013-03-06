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

	// Lazy test to see if this is our packet (does it start with ** ?).
	if (data[32] == 'A' && data[33] == 'C') {
        packetData = data[34];
        if (ioRam[0x02] & 0x80) {
            ioRam[0x02] &= ~0x80;
            data[0] = 'A';
            data[1] = 'C';
            data[2] = ioRam[0x01];
            data[3] = 0;
            sendPacket(4,(unsigned short *)data);
            ioRam[0x01] = packetData;
            requestInterrupt(SERIAL);
        }
    }
	else {
	//	PA_OutputText(1, 0, 5,"Recv Unknown:%s    ",data+32);
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

void sendPacket(int bytes, unsigned short buffer[])
{
// Wifi_RawTxFrame: Send a raw 802.11 frame at a specified rate
//  unsigned short datalen:	The length in bytes of the frame to send
//  unsigned short rate:	The rate to transmit at (Specified as mbits/10, 1mbit=0x000A, 2mbit=0x0014)		
//  unsigned short * data:	Pointer to the data to send (should be halfword-aligned)
//  Returns:			Nothing of interest.
	if (Wifi_RawTxFrame(bytes, 0x0014, buffer) != 0)
        printLog("Nifi send error\n");
}

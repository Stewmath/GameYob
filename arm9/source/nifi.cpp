#include <nds.h>
#include <dswifi9.h>
#include "nifi.h"
#include "mmu.h"
#include "main.h"
#include "gameboy.h"
#include "console.h"
#include "inputhelper.h"

inline int INT_AT(u8* ptr) {
    return *ptr | *(ptr+1)<<8 | *(ptr+2)<<16 | *(ptr+3)<<24;
}
inline void INT_TO(u8* ptr, int i) {
    *ptr = i&0xff;
    *(ptr+1) = (i>>8)&0xff;
    *(ptr+2) = (i>>16)&0xff;
    *(ptr+3) = (i>>24)&0xff;
}

volatile int linkReceivedData=-1;
volatile int linkSendData;
volatile bool transferWaiting = false;
volatile bool transferReady = false;
volatile bool readyToSend=true;
int lastSendid = 0xff;
volatile int nifiSendid;


enum ClientStatus {
    CLIENT_IDLE=0,
    CLIENT_WAITING,
    CLIENT_CONNECTING,
    CLIENT_CONNECTED
};
enum HostStatus {
    HOST_IDLE=0,
    HOST_WAITING,
    HOST_CONNECTED
};

bool nifiEnabled=true;

volatile bool foundClient;
volatile bool foundHost;
bool isClient = false;
bool isHost = false;

volatile int status = 0;
volatile u32 hostId;

char hostRomTitle[20];

volatile u8 receivedInput[32];
volatile bool receivedInputReady[32];

const int CLIENT_FRAME_LAG = 4;

void nifiSendPacket(u8 command, u8* data, int dataLen)
{
    if (!nifiEnabled)
        return;
    u8* buffer = (u8*)malloc(dataLen + 9);

    buffer[0] = 'Y';
    buffer[1] = 'O';
    buffer[2] = 'B';
    buffer[3] = 'P';
    *(u32*)(buffer+4) = hostId;
    buffer[8] = command;
    memcpy(buffer+9, data, dataLen);

    //printLog("%d: Sent %x\n", ioRam[0x02]&1, data);

    if (Wifi_RawTxFrame(dataLen+9, 0x0014, (unsigned short *)buffer) != 0)
        printLog("Nifi send error\n");

    free(buffer);
}

u32 packetHostId(u8* packet) {
    return (u32)INT_AT(packet+32+4);
}
bool verifyPacket(u8* packet, int len) {
    return (len >= 32+7 &&
            packet[32+0] == 'Y' &&
            packet[32+1] == 'O' &&
            packet[32+2] == 'B' &&
            packet[32+3] == 'P' &&
            ((isClient && status == CLIENT_WAITING) ||
             packetHostId(packet) == hostId));
}
u8 packetCommand(u8* packet) {
    return packet[32+8];
}
u8* packetData(u8* packet) {
    return packet+32+9;
}


void packetHandler(int packetID, int readlength)
{
    static u32 pkt[4096/2];
    static u8* packet = (u8*)pkt;
    // static int bytesRead = 0; // Not used

    // Wifi_RxRawReadPacket:  Allows user code to read a packet from within the WifiPacketHandler function
    //  long packetID:		a non-unique identifier which locates the packet specified in the internal buffer
    //  long readlength:		number of bytes to read (actually reads (number+1)&~1 bytes)
    //  unsigned short * data:	location for the data to be read into
    
	// bytesRead = Wifi_RxRawReadPacket(packetID, readlength, (unsigned short *)data); // Not used
	Wifi_RxRawReadPacket(packetID, readlength, (unsigned short *)packet);
	
    if (verifyPacket(packet, readlength)) {
        u8* data = packetData(packet);

        switch(packetCommand(packet)) {
            case NIFI_CMD_HOST:
                if (isClient && status == CLIENT_WAITING) {
                    foundHost = true;
                    hostId = packetHostId(packet);
                    strncpy(hostRomTitle, (char*)data, 19);
                    hostRomTitle[19] = '\0';
                }
                break;
            case NIFI_CMD_CLIENT:
                if (isHost && status == HOST_WAITING) {
                    foundClient = true;
                }
                break;

            case NIFI_CMD_INPUT:
                if (true || isClient) {
                    int num = data[0];
                    int frame1 = INT_AT(data+1);

                    for (int i=0; i<num; i++) {
                        int frame = frame1+i;

                        if (receivedInputReady[frame&31]) {
                            //printf("OVERFLOW\n");
                        }
                        else {
                            if (((frame - gameboy->gameboyFrameCounter)&31) < 16) {
                                receivedInputReady[frame&31] = true;
                                receivedInput[frame&31] = data[5+i];
                            }
                        }
                    }
                }
                break;
        }

    }
}


void Timer_10ms(void) {
	Wifi_Timer(10);
}

void enableNifi()
{
	Wifi_InitDefault(false);

// Wifi_SetPromiscuousMode: Allows the DS to enter or leave a "promsicuous" mode, in which 
//   all data that can be received is forwarded to the arm9 for user processing.
//   Best used with Wifi_RawSetPacketHandler, to allow user code to use the data
//   (well, the lib won't use 'em, so they're just wasting CPU otherwise.)
//  int enable:  0 to disable promiscuous mode, nonzero to engage
	Wifi_SetPromiscuousMode(1);

// Wifi_RawSetPacketHandler: Set a handler to process all raw incoming packets
//  WifiPacketHandler wphfunc:  Pointer to packet handler (see WifiPacketHandler definition for more info)
	Wifi_RawSetPacketHandler(packetHandler);

// Wifi_SetChannel: If the wifi system is not connected or connecting to an access point, instruct
//   the chipset to change channel
//  int channel: the channel to change to, in the range of 1-13
	Wifi_SetChannel(10);

	if(1) {
		//for secial configuration for wifi
		irqDisable(IRQ_TIMER3);
		irqSet(IRQ_TIMER3, Timer_10ms); // replace timer IRQ
		// re-set timer3
		TIMER3_CR = 0;
		TIMER3_DATA = -(6553 / 5); // 6553.1 * 256 / 5 cycles = ~10ms;
		TIMER3_CR = 0x00C2; // enable, irq, 1/256 clock
		irqEnable(IRQ_TIMER3);
	}


// Wifi_EnableWifi: Instructs the ARM7 to go into a basic "active" mode, not actually
//   associated to an AP, but actively receiving and potentially transmitting
	Wifi_EnableWifi();

    transferWaiting = false;
    nifiEnabled = true;
}

void disableNifi() {
    Wifi_DisableWifi();
    nifiEnabled = false;
}


void nifiHostMenu(int value) {
    consoleClear();

    foundClient = false;
    isHost = true;
    isClient = false;
    status = HOST_WAITING;
    hostId = rand();

    printf("Waiting for client...\n");
    printf("Host ID: %d\n\n", hostId);
    printf("Press B to give up.\n\n");

    bool willConnect;
    while (!foundClient) {
        swiWaitForVBlank();
        swiWaitForVBlank();
        swiWaitForVBlank();
        readKeys();
        if (keyJustPressed(KEY_B))
            break;

        char* romTitle = gameboy->getRomTitle();
        nifiSendPacket(NIFI_CMD_HOST, (u8*)romTitle, strlen(romTitle)+1);
    }

    if (foundClient) {
        printf("Found client.\n");
        status = HOST_CONNECTED;
        willConnect = true;
    }
    else {
        isHost = false;
        status = 0;
        printf("Couldn't find client.\n");
        willConnect = false;
    }
    
    for (int i=0; i<90; i++) swiWaitForVBlank();

    if (willConnect) {
        gameboy->init();
    }
}

void nifiClientMenu(int value) {
    consoleClear();
    printf("Waiting for host...\n\n");
    printf("Press B to give up.\n\n");

    foundHost = false;
    isClient = true;
    isHost = false;
    status = CLIENT_WAITING;

    while (!foundHost) {
        swiWaitForVBlank();
        swiWaitForVBlank();
        swiWaitForVBlank();
        readKeys();
        if (keyJustPressed(KEY_B))
            break;
    }

    bool willConnect;
    if (foundHost) {
        printf("Found host.\n");
        printf("Host ROM: \"%s\"\n\n", hostRomTitle);
        printf("Press A to connect, B to cancel.\n\n");

        while (true) {
            swiWaitForVBlank();
            readKeys();
            if (keyJustPressed(KEY_A)) {
                willConnect = true;
                nifiSendPacket(NIFI_CMD_CLIENT, 0, 0);
                printf("Connected to host. Id: %d\n", hostId);
                status = CLIENT_CONNECTED;

                memset((void*)receivedInputReady, 0, sizeof(receivedInputReady));
                break;
            }
            else if (keyJustPressed(KEY_B)) {
                willConnect = false;
                printf("Connection cancelled.\n");
                break;
            }
        }
    }
    else {
        isClient = false;
        status = 0;
        printf("Couldn't find host.\n");
    }
    
    for (int i=0; i<90; i++) swiWaitForVBlank();

    if (willConnect) {
        gameboy->init();
        gameboy->sgbSetActiveController(1);
        gameboy->pause();
    }
}

bool nifiIsHost() { return isHost; }
bool nifiIsClient() { return isClient; }
bool nifiIsLinked() { return isHost || isClient; }

void nifiCheckInput() {
    if (nifiIsClient()) {
        if (!receivedInputReady[gameboy->gameboyFrameCounter&31]) {
            gameboy->pause();
            printLog("Nifi pause...\n");
        }
        else
            gameboy->unpause();
    }
    else if (nifiIsHost()) {
        if (gameboy->gameboyFrameCounter >= CLIENT_FRAME_LAG &&
                !receivedInputReady[gameboy->gameboyFrameCounter&31]) {
            gameboy->pause();
            printLog("Nifi pause...\n");
        }
        else
            gameboy->unpause();
    }
}

const int MAX = CLIENT_FRAME_LAG;
u8 oldInputs[MAX+1];

void nifiUpdateInput() {
    u32 bfr[4];
    u8* buffer = (u8*)bfr;
    int frame = gameboy->gameboyFrameCounter;
    if (nifiIsClient())
        frame += CLIENT_FRAME_LAG;

    u8 olderInput = oldInputs[0];

    if (nifiIsLinked()) {
        // Send input to other ds
        for (int i=0; i<MAX-1; i++)
            oldInputs[i] = oldInputs[i+1];
        oldInputs[MAX-1] = buttonsPressed;
        int number = (frame < MAX-1 ? frame+1 : MAX);
        //number = 1;

        INT_TO(buffer+1, frame-number+1);
        for (int i=0; i<number; i++)
            buffer[5+i] = oldInputs[(MAX-number)+i];
        //buffer[5] = oldInputs[2];
        buffer[0] = number;
        nifiSendPacket(NIFI_CMD_INPUT, buffer, 5+number);


        frame = gameboy->gameboyFrameCounter;

        // Set other controller's input
        if (receivedInputReady[frame&31]) {
            gameboy->controllers[!gameboy->sgbGetActiveController()] = receivedInput[frame&31];
            receivedInputReady[frame&31] = false;
        }
        else
            printLog("NIFI NOT READY\n");
    }

    frame = gameboy->gameboyFrameCounter;

    if (!nifiIsLinked() || nifiIsHost()) {
        gameboy->controllers[gameboy->sgbGetActiveController()] = buttonsPressed;
    }
    else if (nifiIsClient()) {
        if (frame >= CLIENT_FRAME_LAG)
            gameboy->controllers[gameboy->sgbGetActiveController()] = olderInput;
        else
            gameboy->controllers[gameboy->sgbGetActiveController()] = 0xff;
    }
}

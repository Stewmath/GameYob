#include <nds.h>
#include <dswifi9.h>
#include "romfile.h"
#include "nifi.h"
#include "mmu.h"
#include "main.h"
#include "gameboy.h"
#include "console.h"
#include "inputhelper.h"
#include "gbmanager.h"
#include "menu.h"

void nifiLinkTypeMenu();
void nifiHostMenu();
void nifiClientMenu();

inline int INT_AT(u8* ptr) {
    return *ptr | *(ptr+1)<<8 | *(ptr+2)<<16 | *(ptr+3)<<24;
}
inline void INT_TO(u8* ptr, int i) {
    *ptr = i&0xff;
    *(ptr+1) = (i>>8)&0xff;
    *(ptr+2) = (i>>16)&0xff;
    *(ptr+3) = (i>>24)&0xff;
}

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
enum LinkType {
    LINK_CABLE=0,
    LINK_SGB
};
enum NifiCmd {
    NIFI_CMD_HOST=0,
    NIFI_CMD_CLIENT,
    NIFI_CMD_ACKNOWLEDGE,
    NIFI_CMD_INPUT,
    NIFI_CMD_TRANSFER_SRAM,

    NIFI_CMD_FRAGMENT
};

enum {
    HEADER_HOSTID       = 0x04, // u32
    HEADER_DATASIZE     = 0x08, // u32
    HEADER_COMMAND      = 0x0c, // u8
    HEADER_CHECKSUM     = 0x0d, // u8
    HEADER_ACKNOWLEDGE  = 0x0e, // u8
};
const int PACKET_HEADER_SIZE = 0x10;

const int CLIENT_FRAME_LAG = 4;
const int FRAGMENT_SIZE = 0x200;

const int OLD_INPUTS_BUFFER_SIZE = CLIENT_FRAME_LAG + 2;

u8* fragmentBuffer = NULL;
u8 lastFragment;

bool nifiEnabled=true;
bool nifiInitialized = false;
volatile bool packetAcknowledged;

volatile bool foundClient;
volatile bool foundHost;
bool isClient = false;
bool isHost = false;

int nifiFrameCounter;
int nifiLinkType;
volatile bool receivedSram;

u8* nifiInputDest;      // Where input for this DS goes
u8* nifiOtherInputDest; // Where input from other DS goes

int nifiConsecutiveWaitingFrames = 0;

volatile int status = 0;
volatile u32 hostId;

char hostRomTitle[20];

volatile u8 receivedInput[32];
volatile bool receivedInputReady[32];

u8 oldInputs[OLD_INPUTS_BUFFER_SIZE];

u8 nifiGetChecksum(u8* data, u32 dataLen) {
    u8 checksum = 0;
    for (int i=0; i<dataLen+PACKET_HEADER_SIZE; i++) {
        if (i == HEADER_CHECKSUM)
            continue;
        checksum *= 7;
        checksum += data[i];
    }
    return checksum;
}

int nifiSendPacket(u8 command, u8* data, u32 dataLen, bool acknowledge)
{
    if (!nifiEnabled || !nifiInitialized)
        return 1;

    int errcode = 0;

    if (command == NIFI_CMD_FRAGMENT || dataLen <= FRAGMENT_SIZE) {
        u8* buffer = (u8*)malloc(dataLen + PACKET_HEADER_SIZE);
        if (!buffer) {
            printLog("Nifi out of memory\n");
            return 1;
        }

        buffer[0] = 'Y';
        buffer[1] = 'O';
        buffer[2] = 'B';
        buffer[3] = 'P';

        INT_TO(buffer+HEADER_HOSTID, hostId);
        buffer[HEADER_COMMAND] = command;
        INT_TO(buffer+HEADER_DATASIZE, dataLen);
        buffer[HEADER_ACKNOWLEDGE] = (acknowledge ? 1 : 0);

        memcpy(buffer+PACKET_HEADER_SIZE, data, dataLen);

        buffer[HEADER_CHECKSUM] = nifiGetChecksum(buffer, dataLen);

        packetAcknowledged = false;
        if (Wifi_RawTxFrame(dataLen+PACKET_HEADER_SIZE, 0x0014, (unsigned short *)buffer) != 0) {
            printLog("Nifi send error\n");
            errcode = 1;
        }
        if (acknowledge) {
            int attemptCounter = 0;
            while (!packetAcknowledged) {
                int frameCounter = 0;
                while (!packetAcknowledged && frameCounter < 10) {
                    swiWaitForVBlank();
                    frameCounter++;
                }
                if (!packetAcknowledged) {
                    if (attemptCounter > 10) {
                        errcode = 1;
                        printLog("Connection lost.\n");
                        nifiStop();
                        break;
                    }
                    else
                        Wifi_RawTxFrame(dataLen+PACKET_HEADER_SIZE, 0x0014,
                                (unsigned short *)buffer);
                }
                attemptCounter++;
            }
        }

        free(buffer);
    }
    else {
        u8* buffer = (u8*)malloc(FRAGMENT_SIZE + 0x10);

        u8 numFragments = (dataLen+(FRAGMENT_SIZE-1))/FRAGMENT_SIZE;

        for (int i=0; i<numFragments; i++) {
            int fragmentSize = FRAGMENT_SIZE;
            if (i == numFragments-1) {
                fragmentSize = dataLen % FRAGMENT_SIZE;
                if (fragmentSize == 0)
                    fragmentSize = FRAGMENT_SIZE;
            }

            INT_TO(buffer, dataLen);
            buffer[4] = command;
            buffer[5] = numFragments;
            buffer[6] = i;
            memcpy(buffer+0x10, data+i*FRAGMENT_SIZE, fragmentSize);

            printLog("SEND %d\n", i);
            if (nifiSendPacket(NIFI_CMD_FRAGMENT, buffer,
                        fragmentSize+0x10, acknowledge)) {
                errcode = 1;
                break;
            }
            swiWaitForVBlank(); // Excessive?
            // Send it twice for safety
            nifiSendPacket(NIFI_CMD_FRAGMENT, buffer,
                    fragmentSize+0x10, acknowledge);

            swiWaitForVBlank(); // Excessive?
        }

        free(buffer);
    }

    return errcode;
}

u32 packetHostId(u8* packet) {
    return (u32)INT_AT(packet+32+HEADER_HOSTID);
}
bool verifyPacket(u8* packet, int len) {
    if (len >= 32+PACKET_HEADER_SIZE &&
            packet[32+0] == 'Y' &&
            packet[32+1] == 'O' &&
            packet[32+2] == 'B' &&
            packet[32+3] == 'P' &&
            ((isClient && status == CLIENT_WAITING) ||
             packetHostId(packet) == hostId)) {

        /*
        if (packet[32+HEADER_COMMAND] == NIFI_CMD_FRAGMENT)
            return true; // Checksum is broken for fragmented packets?
            */
         u8 checksum =
             nifiGetChecksum(packet+32, INT_AT(packet+32+HEADER_DATASIZE));
         if (checksum == packet[32+HEADER_CHECKSUM])
             return true;
         else
             printLog("Nifi bad packet checksum\n");
    }
    return false;
}
u8 packetCommand(u8* packet) {
    return packet[32+HEADER_COMMAND];
}
u8* packetData(u8* packet) {
    return packet+32+PACKET_HEADER_SIZE;
}

void handlePacketCommand(int command, u8* data) {
    switch(command) {
        case NIFI_CMD_ACKNOWLEDGE:
            packetAcknowledged = true;
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

                if (nifiConsecutiveWaitingFrames >= 60)
                    printLog("Received packet: %x\n", frame1);

                for (int i=0; i<num; i++) {
                    int frame = frame1+i;

                    if (frame >= mgr_frameCounter) {
                        if (receivedInputReady[frame&31]) {
                            if (receivedInput[frame&31] != data[5+i])
                                printLog("MISMATCH %x\n", frame);
                        }
                        else {
                            receivedInputReady[frame&31] = true;
                            receivedInput[frame&31] = data[5+i];
                        }
                    }
                }
            }
            break;
        case NIFI_CMD_TRANSFER_SRAM:
            {
                u8* dest;
                if (nifiLinkType == LINK_SGB)
                    memcpy(gameboy->externRam, data, gameboy->getNumRamBanks()*0x2000);
                else if (gb2)
                    memcpy(gb2->externRam, data, gb2->getNumRamBanks()*0x2000);
                else
                    printLog("GB2 NOT INITIALIZED!\n");
                printLog("Received SRAM.\n");
                receivedSram = true;
            }
            break;

            // A command broken up into multiple packets
        case NIFI_CMD_FRAGMENT:
            {
                u32 totalSize = INT_AT(data);
                u8 command = data[4];
                u8 numFragments = data[5];
                u8 fragment = data[6];

                int fragmentSize = FRAGMENT_SIZE;
                if (fragment == numFragments-1) {
                    fragmentSize = totalSize % FRAGMENT_SIZE;
                    if (fragmentSize == 0)
                        fragmentSize = FRAGMENT_SIZE;
                }

                if (fragmentBuffer == NULL && fragment != 0) {
                    printLog("NULL Buffer.\n");
                    return;
                }
                if (fragment == 0) {
                    if (fragmentBuffer != NULL)
                        free(fragmentBuffer);
                    fragmentBuffer = (u8*)malloc(totalSize);
                    if (fragmentBuffer == 0) {
                        printLog("Nifi not enough memory\n");
                        return;
                    }
                }
                else if (lastFragment > fragment) {
                    if (fragmentBuffer != NULL) {
                        free(fragmentBuffer);
                        fragmentBuffer = NULL;
                    }
                    printLog("Fragment mismatch\n");
                    lastFragment = -1;
                    return;
                }

                if (fragment == 0 || lastFragment+1 == fragment)
                    memcpy(fragmentBuffer+fragment*FRAGMENT_SIZE, data+0x10, fragmentSize);

                lastFragment = fragment;

                if (fragment == numFragments-1) {
                    handlePacketCommand(command, fragmentBuffer);
                    free(fragmentBuffer);
                    lastFragment = -1;
                }
            }
            break;
    }
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
        if (*(packet+32+HEADER_ACKNOWLEDGE))
            nifiSendPacket(NIFI_CMD_ACKNOWLEDGE, 0, 0, false);
        u8* data = packetData(packet);

        if (packetCommand(packet) == NIFI_CMD_HOST) {
            if (isClient && status == CLIENT_WAITING) {
                foundHost = true;
                hostId = packetHostId(packet);
                nifiLinkType = data[0];
                strcpy(hostRomTitle, (char*)(data+8));
            }
        }
        else
            handlePacketCommand(packetCommand(packet), data);
    }
}


void Timer_10ms(void) {
	Wifi_Timer(10);
}

void nifiStop() {
    isClient = false;
    isHost = false;
    disableNifi();
    nifiUnpause();
}

void enableNifi()
{
    if (nifiInitialized)
        return;

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

    nifiInitialized = true;
}

void disableNifi() {
    if (!nifiInitialized)
        return;

    Wifi_DisableWifi();
    nifiInitialized = false;
}

void nifiInterLinkMenu() {
    int selection = 0;
    receivedSram = false;

    for (;;) {
        swiWaitForVBlank();
        clearConsole();

        printf("\n\n");
        if (selection == 0) {
            iprintfColored(CONSOLE_COLOR_LIGHT_YELLOW, "* As Host\n\n");
            iprintfColored(CONSOLE_COLOR_WHITE, "  As Client\n\n");
        }
        else {
            iprintfColored(CONSOLE_COLOR_WHITE, "  As Host\n\n");
            iprintfColored(CONSOLE_COLOR_LIGHT_YELLOW, "* As Client\n\n");
        }

        inputUpdateVBlank();
        if (keyJustPressed(mapMenuKey(MENU_KEY_B)))
            return;
        if (keyJustPressed(mapMenuKey(MENU_KEY_A))) {
            isHost = selection == 0;
            if (isHost)
                nifiLinkTypeMenu();
            else
                nifiClientMenu();
            break;
        }
        if (keyPressedAutoRepeat(mapMenuKey(MENU_KEY_UP) | mapMenuKey(MENU_KEY_DOWN)))
            selection = !selection;
    }
}

void nifiLinkTypeMenu() {
    int selection = 0;

    for (;;) {
        swiWaitForVBlank();
        clearConsole();

        printf("\n\n");
        if (selection == 0) {
            iprintfColored(CONSOLE_COLOR_LIGHT_YELLOW, "* Cable Link\n\n");
            iprintfColored(CONSOLE_COLOR_WHITE, "  SGB Multiplayer\n\n");
        }
        else {
            iprintfColored(CONSOLE_COLOR_WHITE, "  Cable Link\n\n");
            iprintfColored(CONSOLE_COLOR_LIGHT_YELLOW, "* SGB Multiplayer\n\n");
        }

        inputUpdateVBlank();
        if (keyJustPressed(mapMenuKey(MENU_KEY_B)))
            return;
        if (keyJustPressed(mapMenuKey(MENU_KEY_A))) {
            nifiLinkType = selection;

            if (isHost)
                nifiHostMenu();
            else
                nifiClientMenu();
            break;
        }
        if (keyPressedAutoRepeat(mapMenuKey(MENU_KEY_UP) | mapMenuKey(MENU_KEY_DOWN)))
            selection = !selection;
    }
}

void nifiSendSram() {
    nifiSendPacket(NIFI_CMD_TRANSFER_SRAM, gameboy->externRam,
            gameboy->getNumRamBanks()*0x2000, false);
    printLog("Sent SRAM.\n");
}

void nifiReceiveSram() {
    nifiConsecutiveWaitingFrames = 0;
    while (!receivedSram) {
        swiWaitForVBlank();
        nifiConsecutiveWaitingFrames++;
        if (nifiConsecutiveWaitingFrames >= 60*5) {
            printLog("Connection lost.\n");
            nifiStop();
            printLog("Nifi turned off.\n");

            for (int i=0; i<60*3; i++) swiWaitForVBlank();
            break;
        }
    }
}

void nifiStartLink() {
    bool waitForSram = false;
    bool sendSram = false;

    nifiFrameCounter = -1;

    mgr_reset();
    if (nifiLinkType == LINK_CABLE) {
        printLog("Start Gb2\n");
        mgr_startGb2(0);
    }

    if (isHost) {
        if (nifiLinkType == LINK_CABLE)
            mgr_setInternalClockGb(gameboy);

        // Fill in first few frames of client's input
        for (int i=0; i<CLIENT_FRAME_LAG; i++) {
            receivedInputReady[i] = true;
            receivedInput[i] = 0xff;
        }

        // Set input destinations
        if (nifiLinkType == LINK_SGB) {
            nifiInputDest = &gameboy->controllers[0];
            nifiOtherInputDest = &gameboy->controllers[1];
        }
        else if (nifiLinkType == LINK_CABLE) {
            nifiInputDest = &gameboy->controllers[0];
            nifiOtherInputDest = &gb2->controllers[0];
        }

        // Sram transfers
        sendSram = true;
        if (nifiLinkType == LINK_CABLE)
            waitForSram = true;
    }
    else if (isClient) {
        if (nifiLinkType == LINK_CABLE)
            mgr_setInternalClockGb(gb2);

        // First few frames of input are skipped, so fill them in
        for (int i=0; i<OLD_INPUTS_BUFFER_SIZE; i++)
            oldInputs[i] = 0xff;

        // Set input destinations
        if (nifiLinkType == LINK_SGB) {
            nifiInputDest = &gameboy->controllers[1];
            nifiOtherInputDest = &gameboy->controllers[0];
        }
        else if (nifiLinkType == LINK_CABLE) {
            nifiInputDest = &gameboy->controllers[0];
            nifiOtherInputDest = &gb2->controllers[0];
        }

        // Sram transfers
        waitForSram = true;
        if (nifiLinkType == LINK_CABLE)
            sendSram = true;
    }

    printLog("Begin nifi link.\n");
    if (isHost) {
        if (sendSram && gameboy->getNumRamBanks())
            nifiSendSram();
        if (waitForSram && gameboy->getNumRamBanks())
            nifiReceiveSram();
    }
    else {
        if (waitForSram && gameboy->getNumRamBanks())
            nifiReceiveSram();
        for (int i=0;i<60;i++)
            swiWaitForVBlank();
        if (sendSram && gameboy->getNumRamBanks())
            nifiSendSram();
    }

    nifiConsecutiveWaitingFrames = 0;
    //closeMenu();
}

void nifiHostMenu() {
    enableNifi();
    clearConsole();

    foundClient = false;
    isHost = true;
    isClient = false;
    status = HOST_WAITING;
    hostId = rand();

    printf("Waiting for client...\n");
    printf("Host ID: %d\n\n", hostId);
    printf("Press B to give up.\n\n");

    bool willConnect=false;
    while (!foundClient) {
        swiWaitForVBlank();
        swiWaitForVBlank();
        swiWaitForVBlank();
        inputUpdateVBlank();
        if (keyJustPressed(KEY_B))
            break;

        u8 buffer[30];
        buffer[0] = nifiLinkType;
        strcpy((char*)(buffer+8), gameboy->getRomFile()->getRomTitle());
        nifiSendPacket(NIFI_CMD_HOST, buffer, 30, false);
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
    
    if (willConnect) {
        nifiStartLink();
    }

    for (int i=0; i<90; i++) swiWaitForVBlank();
}

void nifiClientMenu() {
    enableNifi();
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
        inputUpdateVBlank();
        if (keyJustPressed(KEY_B))
            break;
    }

    bool willConnect = false;
    if (foundHost) {
        printf("Found host.\n\n");
        printf("Host ROM: \"%s\"\n", hostRomTitle);
        printf("Link Type: ");
        if (nifiLinkType == LINK_CABLE)
            printf("Cable Link\n\n");
        else if (nifiLinkType == LINK_SGB)
            printf("SGB Multiplayer\n\n");
        printf("Press A to connect, B to cancel.\n\n");

        while (true) {
            swiWaitForVBlank();
            inputUpdateVBlank();
            if (keyJustPressed(KEY_A)) {
                willConnect = true;
                nifiSendPacket(NIFI_CMD_CLIENT, 0, 0, true);
                printf("Connected to host.\nHost Id: %d\n", hostId);
                status = CLIENT_CONNECTED;

                memset((void*)receivedInputReady, 0, sizeof(receivedInputReady));
                break;
            }
            else if (keyJustPressed(KEY_B)) {
                willConnect = false;
                printf("Connection cancelled.\n");
                for (int i=0; i<90; i++) swiWaitForVBlank();
                break;
            }
        }
    }
    else {
        isClient = false;
        status = 0;
        printf("Couldn't find host.\n");
        nifiStop();
        for (int i=0; i<90; i++) swiWaitForVBlank();
    }
    
    if (willConnect) {
        nifiStartLink();
    }
}

bool nifiIsHost() { return isHost; }
bool nifiIsClient() { return isClient; }
bool nifiIsLinked() { return isHost || isClient; }

int nifiWasPaused = -1;
void nifiPause() {
    if (nifiWasPaused == -1) {
        nifiWasPaused = mgr_isPaused();
    }
    mgr_pause();
}
void nifiUnpause() {
    if (nifiWasPaused == -1 || !nifiWasPaused) {
        mgr_unpause();
    }
    nifiWasPaused = -1;
}

void nifiUpdateInput() {
    u8* inputDest;
    u8* otherInputDest;
    if (nifiIsLinked()) {
        inputDest = nifiInputDest;
        otherInputDest = nifiOtherInputDest;
    }
    else
        inputDest = &gameboy->controllers[0];

    u32 bfr[4];
    u8* buffer = (u8*)bfr;

    u32 actualFrame = mgr_frameCounter;
    u32 inputFrame = mgr_frameCounter;
    bool frameHasPassed = nifiFrameCounter != mgr_frameCounter;
    if (nifiFrameCounter == -1)
        printf("Start at %d", mgr_frameCounter);
    if (frameHasPassed && nifiFrameCounter > 0)
        receivedInputReady[(nifiFrameCounter-1)&31] = false;
    nifiFrameCounter = mgr_frameCounter;

    if (nifiIsClient())
        inputFrame += CLIENT_FRAME_LAG;

    u8 olderInput = oldInputs[OLD_INPUTS_BUFFER_SIZE-CLIENT_FRAME_LAG];

    if (nifiIsLinked()) {
        if (frameHasPassed) {
            for (int i=0; i<OLD_INPUTS_BUFFER_SIZE-1; i++)
                oldInputs[i] = oldInputs[i+1];
            oldInputs[OLD_INPUTS_BUFFER_SIZE-1] = buttonsPressed;
        }

        // Send input to other ds
        INT_TO(buffer+1, inputFrame-OLD_INPUTS_BUFFER_SIZE+1);
        for (int i=0; i<OLD_INPUTS_BUFFER_SIZE; i++)
            buffer[5+i] = oldInputs[i];
        buffer[0] = OLD_INPUTS_BUFFER_SIZE;
        nifiSendPacket(NIFI_CMD_INPUT, buffer, 5+OLD_INPUTS_BUFFER_SIZE, false);

        // Set other controller's input
        if (receivedInputReady[actualFrame&31]) {
            *otherInputDest = receivedInput[actualFrame&31];
            nifiUnpause();
            nifiConsecutiveWaitingFrames = 0;
        }
        else {
            nifiConsecutiveWaitingFrames++;
            printLog("NIFI NOT READY %x\n", nifiFrameCounter);
            nifiPause();
        }
        if (nifiConsecutiveWaitingFrames >= 120) {
            nifiConsecutiveWaitingFrames = 0;
            printLog("Connection lost!\n");
            nifiStop();
            printLog("Nifi turned off.\n");
        }
    }

    if (!nifiIsLinked() || nifiIsHost()) {
        *inputDest = buttonsPressed;
    }
    else if (nifiIsClient()) {
        *inputDest = olderInput;
    }
}

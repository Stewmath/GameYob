#pragma once

extern volatile int linkReceivedData;
extern volatile int linkSendData;
extern volatile bool transferWaiting;
extern volatile bool transferReady;
extern volatile bool receivedPacket;
extern volatile int nifiSendid;
// Don't write directly
extern bool nifiEnabled;

void enableNifi();
void disableNifi();
//void sendPacketByte(u8 command, u8 data);

void nifiSendPacket(u8 command, u8* data, int dataLen);

void nifiHostMenu(int value);
void nifiClientMenu(int value);

bool nifiIsHost();
bool nifiIsClient();
bool nifiIsLinked();

void nifiCheckInput();
void nifiUpdateInput();

enum NifiCmd {
    NIFI_CMD_HOST=0,
    NIFI_CMD_CLIENT,
    NIFI_CMD_INPUT,
};


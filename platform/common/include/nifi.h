#pragma once

extern volatile int linkReceivedData;
extern volatile int linkSendData;
extern volatile bool transferWaiting;
extern volatile bool receivedPacket;
extern volatile int nifiSendid;
// Don't write directly
extern bool nifiEnabled;

void enableNifi();
void disableNifi();
//void sendPacketByte(u8 command, u8 data);

int nifiSendPacket(u8 command, u8* data, u32 dataLen, bool acknowledge = false);

void nifiStop();

void nifiInterLinkMenu();

bool nifiIsHost();
bool nifiIsClient();
bool nifiIsLinked();

void nifiPause();
void nifiUnpause();

void nifiUpdateInput();

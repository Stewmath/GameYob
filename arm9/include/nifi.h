#pragma once

extern volatile int packetData;
extern volatile int sendData;
extern volatile bool transferWaiting;
extern volatile bool transferReady;
extern volatile bool receivedPacket;
extern volatile int nifiSendid;
// Don't write directly
extern bool nifiEnabled;

void enableNifi();
void disableNifi();
void sendPacketByte(u8 command, u8 data);

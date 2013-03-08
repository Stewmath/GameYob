#pragma once

extern volatile int packetData;
extern volatile int sendData;
extern bool transferWaiting;

void initNifi();
void sendPacketByte(u8 command, u8 data);

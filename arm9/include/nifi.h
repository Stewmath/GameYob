#pragma once

extern int packetData;

void initNifi();
void sendPacket(int bytes, unsigned short buffer[]);

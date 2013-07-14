#pragma once
#include <nds.h>

extern u8 sgbMap[20*18];

// These aren't touched outside of sgb.cpp except for save states
extern int sgbPacketLength;
extern int sgbPacketsTransferred;
extern int sgbPacketBit;
extern u8 sgbCommand;


void initSGB();
void sgbHandleP1(u8 val);

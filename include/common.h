// This file can be included by arm7 and arm9.
#pragma once

typedef struct SharedData {
    u8 scalingOn;
    u8 scaleTransferReady;
} SharedData;

extern volatile SharedData* sharedData;

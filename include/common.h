// This file can be included by arm7 and arm9.
#pragma once

typedef struct SharedData {
    u8 scalingOn;
    u8 scaleTransferReady;
    int fifosSent, fifosReceived;

    // Sound stuff
    bool hyperSound;
    int cycles;
    int dsCycles;
    bool frameFlip_Gameboy;
    bool frameFlip_DS;

    u8 volControl; // NR50
    u8 chanOutput; // NR51
    u8 chanRealVol[4];
    int chanRealFreq[4];
    u8 chanPan[4];
    int chanDuty[2];
    u8 chanOn;
    bool chanEnabled[4];
    bool lfsr7Bit;
    u8* sampleData;

    u32 message;
} SharedData;

enum {
    GBSND_UPDATE_COMMAND=0,
    GBSND_START_COMMAND,
    GBSND_VOLUME_COMMAND,
    GBSND_MASTER_VOLUME_COMMAND,
    GBSND_KILL_COMMAND,
    GBSND_MUTE_COMMAND,
    GBSND_HYPERSOUND_ENABLE_COMMAND,
    GBSND_DUMMY_COMMAND
};

extern volatile SharedData* sharedData;

extern bool __dsimode;

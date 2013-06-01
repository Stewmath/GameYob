// This file can be included by arm7 and arm9.
#pragma once

typedef struct SharedData {
    u8 scalingOn;
    u8 scaleTransferReady;
    int fifosWaiting;

    // Sound stuff
    bool hyperSound;
    int cycles;
    int dsCycles;
    bool frameFlip_Gameboy;
    bool frameFlip_DS;

    int SO1Vol, SO2Vol;
    u8 chanRealVol[4];
    int chanRealFreq[4];
    u8 chanPan[4];
    int chanDuty[2];
    u8 chanOn;
    bool lfsr7Bit;
    u32 message;
    u8* sampleData;

} SharedData;

typedef struct GbSndMessage {
	u16 type;

    u8 channel;
	union {
		struct {
			const void* data;
			u32 dataSize;
			u16 loopPoint;
			int freq;
			u8 volume;
			u8 pan;
			bool loop;
			u8 format;
		} SoundPlay;

		struct {
			int freq;
			u8 dutyCycle;
			u8 volume;
			u8 pan;
		} SoundPsg;

        struct {
            u8 volume;
            int freq;
        } SoundModify;
	};
} ALIGN(4) GbSndMessage;

enum {
    GBSND_PLAY_MESSAGE=0,
    GBSND_PSG_MESSAGE,
    GBSND_NOISE_MESSAGE,
    GBSND_MODIFY_MESSAGE,
    GBSND_KILL_MESSAGE
};

enum {
    GBSND_UPDATE_COMMAND=0,
    GBSND_START_COMMAND,
    GBSND_VOLUME_COMMAND,
    GBSND_MASTER_VOLUME_COMMAND,
    GBSND_KILL_COMMAND,
    GBSND_CHECK_COMMAND
};

#define VALUE32_SIZE 16
extern volatile SharedData* sharedData;


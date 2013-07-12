#include <nds/arm7/audio.h>
#include <nds/ipc.h>
#include <nds/fifocommon.h>
#include <nds/fifomessages.h>
#include <nds/system.h>
#include <nds/timers.h>
#include "common.h"

#define SOUND_RESOLUTION 100

int channels[4] = {8,9,0,14};
const int dutyIndex[4] = {0, 1, 3, 5};

u8 backgroundSample[16];

bool currentLfsr;

// Use this with the 7-bit lfsr.
// Note: On a gameboy it's 127 samples, but on a ds, # samples must be a 
// multiple of 4. So, it's repeated 4 times.
u8 lfsr7NoiseSample[] ALIGN(4) = {
0x80, 0x80, 0x80, 0x80, 0x80, 0, 0x80, 0x80, 0x80, 0x80, 0x80, 0, 0, 0x80, 0x80, 0x80, 0x80, 0, 0x80, 0, 0x80, 0x80, 0x80, 0, 0, 0, 0, 0x80, 0x80, 0, 0x80, 0x80, 0x80, 0, 0x80, 0, 0, 0x80, 0x80, 0, 0, 0, 0x80, 0, 0x80, 0, 0x80, 0x80, 0, 0, 0, 0, 0, 0x80, 0, 0x80, 0x80, 0x80, 0x80, 0, 0, 0, 0x80, 0x80, 0x80, 0, 0x80, 0x80, 0, 0x80, 0x80, 0, 0, 0x80, 0, 0, 0x80, 0, 0x80, 0, 0, 0x80, 0, 0, 0, 0, 0x80, 0, 0, 0x80, 0x80, 0x80, 0, 0, 0x80, 0, 0x80, 0x80, 0, 0x80, 0, 0, 0, 0x80, 0, 0, 0, 0x80, 0x80, 0, 0, 0x80, 0x80, 0, 0x80, 0, 0x80, 0, 0x80, 0, 0, 0, 0, 0, 0, 0, 0x80,
0x80, 0x80, 0x80, 0x80, 0x80, 0, 0x80, 0x80, 0x80, 0x80, 0x80, 0, 0, 0x80, 0x80, 0x80, 0x80, 0, 0x80, 0, 0x80, 0x80, 0x80, 0, 0, 0, 0, 0x80, 0x80, 0, 0x80, 0x80, 0x80, 0, 0x80, 0, 0, 0x80, 0x80, 0, 0, 0, 0x80, 0, 0x80, 0, 0x80, 0x80, 0, 0, 0, 0, 0, 0x80, 0, 0x80, 0x80, 0x80, 0x80, 0, 0, 0, 0x80, 0x80, 0x80, 0, 0x80, 0x80, 0, 0x80, 0x80, 0, 0, 0x80, 0, 0, 0x80, 0, 0x80, 0, 0, 0x80, 0, 0, 0, 0, 0x80, 0, 0, 0x80, 0x80, 0x80, 0, 0, 0x80, 0, 0x80, 0x80, 0, 0x80, 0, 0, 0, 0x80, 0, 0, 0, 0x80, 0x80, 0, 0, 0x80, 0x80, 0, 0x80, 0, 0x80, 0, 0x80, 0, 0, 0, 0, 0, 0, 0, 0x80,
0x80, 0x80, 0x80, 0x80, 0x80, 0, 0x80, 0x80, 0x80, 0x80, 0x80, 0, 0, 0x80, 0x80, 0x80, 0x80, 0, 0x80, 0, 0x80, 0x80, 0x80, 0, 0, 0, 0, 0x80, 0x80, 0, 0x80, 0x80, 0x80, 0, 0x80, 0, 0, 0x80, 0x80, 0, 0, 0, 0x80, 0, 0x80, 0, 0x80, 0x80, 0, 0, 0, 0, 0, 0x80, 0, 0x80, 0x80, 0x80, 0x80, 0, 0, 0, 0x80, 0x80, 0x80, 0, 0x80, 0x80, 0, 0x80, 0x80, 0, 0, 0x80, 0, 0, 0x80, 0, 0x80, 0, 0, 0x80, 0, 0, 0, 0, 0x80, 0, 0, 0x80, 0x80, 0x80, 0, 0, 0x80, 0, 0x80, 0x80, 0, 0x80, 0, 0, 0, 0x80, 0, 0, 0, 0x80, 0x80, 0, 0, 0x80, 0x80, 0, 0x80, 0, 0x80, 0, 0x80, 0, 0, 0, 0, 0, 0, 0, 0x80,
0x80, 0x80, 0x80, 0x80, 0x80, 0, 0x80, 0x80, 0x80, 0x80, 0x80, 0, 0, 0x80, 0x80, 0x80, 0x80, 0, 0x80, 0, 0x80, 0x80, 0x80, 0, 0, 0, 0, 0x80, 0x80, 0, 0x80, 0x80, 0x80, 0, 0x80, 0, 0, 0x80, 0x80, 0, 0, 0, 0x80, 0, 0x80, 0, 0x80, 0x80, 0, 0, 0, 0, 0, 0x80, 0, 0x80, 0x80, 0x80, 0x80, 0, 0, 0, 0x80, 0x80, 0x80, 0, 0x80, 0x80, 0, 0x80, 0x80, 0, 0, 0x80, 0, 0, 0x80, 0, 0x80, 0, 0, 0x80, 0, 0, 0, 0, 0x80, 0, 0, 0x80, 0x80, 0x80, 0, 0, 0x80, 0, 0x80, 0x80, 0, 0x80, 0, 0, 0, 0x80, 0, 0, 0, 0x80, 0x80, 0, 0, 0x80, 0x80, 0, 0x80, 0, 0x80, 0, 0x80, 0, 0, 0, 0, 0, 0, 0, 0x80
};


// Use this with the 15-bit lfsr.
// noise.h is huge. Like, 32 kilobytes of white noise.
// I wasn't satisfied with the ds's noise channel in some situations, like when 
// the maku tree disappears in Oracle of Ages.
// If the arm7 binary hits its size limit, this may need to be reworked.
#include "noise.h"


// Callback for hyperSound / Sound Fix, which works with arm9 to synchronize 
// sound to the cycle.
void timerCallback() {
    sharedData->dsCycles+=SOUND_RESOLUTION;
    if (sharedData->cycles != -1) {
        bool doit=false;
        if (sharedData->frameFlip_Gameboy == sharedData->frameFlip_DS) {
            if (sharedData->dsCycles >= sharedData->cycles)
                doit = true;
        }
        else {
            doit = true;
        }
        if (doit) {
            sharedData->cycles = -1;
            doCommand(sharedData->message);
        }
    }
}


void setChannelVolume(int c) {
    int channel = channels[c];

    int volume = sharedData->chanRealVol[c]*2;
    if (sharedData->chanPan[c] >= 128)
        volume = 0;
    else if (sharedData->chanPan[c] == 64) {
        volume *= 2;   // DS divides sound by 2 for each speaker; gameboy doesn't
    }

    if (c == 3) // Noise channel
        volume *= 2;

    SCHANNEL_CR(channel) &= ~0xff;
    SCHANNEL_CR(channel) |= volume;
}
void updateChannel(int c) {
    int channel = channels[c];

    if (!(sharedData->chanOn & (1<<c)) || !sharedData->chanEnabled[c]) {
        SCHANNEL_CR(channel) &= ~0x7f; // Set volume to zero
        return;
    }

    SCHANNEL_TIMER(channel) = SOUND_FREQ(sharedData->chanRealFreq[c]);
    if (c < 2) {
        SCHANNEL_CR(channel) &= ~(SOUND_PAN(127) | 7<<24);
        SCHANNEL_CR(channel) |= SOUND_PAN(sharedData->chanPan[c]) | (dutyIndex[sharedData->chanDuty[c]] << 24);
    }
    else if (c == 2) {
        SCHANNEL_CR(channel) &= ~(SOUND_PAN(127));
        SCHANNEL_CR(channel) |= SOUND_PAN(sharedData->chanPan[c]);
    }
    else if (c == 3) {
        if (currentLfsr != sharedData->lfsr7Bit)
            startChannel(c);
        SCHANNEL_CR(channel) &= ~(SOUND_PAN(127));
        SCHANNEL_CR(channel) |= SOUND_PAN(sharedData->chanPan[c]);
    }
    setChannelVolume(c);
}


void startChannel(int c) {
    int channel = channels[c];
    SCHANNEL_CR(channel) &= ~0x7f;

    if (!sharedData->chanEnabled[c])
        return;

    if (c == 2) {
        SCHANNEL_SOURCE(channel) = (u32)sharedData->sampleData;
        SCHANNEL_REPEAT_POINT(channel) = 0;
        SCHANNEL_LENGTH(channel) = 0x20>>2;
        SCHANNEL_CR(channel) = (0 << 29) | SOUND_REPEAT;
    }
    else if (c == 3) {
        currentLfsr = sharedData->lfsr7Bit;
        if (sharedData->lfsr7Bit) {
            SCHANNEL_SOURCE(channel) = (u32)lfsr7NoiseSample;
            SCHANNEL_LENGTH(channel) = 127*4>>2;
        }
        else {
            SCHANNEL_SOURCE(channel) = (u32)lfsr15NoiseSample;
            SCHANNEL_LENGTH(channel) = 32768>>2;
        }
        SCHANNEL_REPEAT_POINT(channel) = 0;
        SCHANNEL_CR(channel) = (0 << 29) | SOUND_REPEAT;
    }
    else { // PSG channels
        SCHANNEL_CR(channel) = (3 << 29);
    }

    updateChannel(c);
    SCHANNEL_CR(channel) |= SCHANNEL_ENABLE;
}

void updateMasterVolume() {
    {
        // TODO: when SO1Vol != SO2Vol
        if (sharedData->SO1Vol*16 != (REG_SOUNDCNT & 0x7f)) {
            REG_SOUNDCNT &= ~0x7f;
            REG_SOUNDCNT |= sharedData->SO1Vol*16;
        }
    }
}

void setHyperSound(int enabled) {
    if (enabled)
        timerStart(1, ClockDivider_1, TIMER_FREQ(4194304/SOUND_RESOLUTION), timerCallback);
    else
        timerStop(1);
}

void doCommand(u32 command) {
	int cmd = (command>>20)&0xf;
	int data = command & 0xFFFF;
    int i;
	
    switch(cmd) {

        // This is used when "basicSound" is enabled.
        case GBSND_UPDATE_VBLANK_COMMAND:
            for (i=0; i<4; i++) {
                if (sharedData->channelsToStart & (1<<i))
                    startChannel(i);

                updateChannel(i);
            }
            updateMasterVolume();
            sharedData->channelsToStart = 0;
            break;


        case GBSND_UPDATE_COMMAND:
            if (data == 4) {
                int i;
                for (i=0; i<4; i++) {
                    updateChannel(i);
                }
            }
            else
                updateChannel(data);
            break;

        case GBSND_START_COMMAND:
            startChannel(data);
            break;

        case GBSND_VOLUME_COMMAND:
            setChannelVolume(data);
            break;

        case GBSND_MASTER_VOLUME_COMMAND:
            updateMasterVolume();
            break;

        case GBSND_KILL_COMMAND:
            //SCHANNEL_CR(channel) &= ~SCHANNEL_ENABLE;
            break;

        case GBSND_MUTE_COMMAND:
            REG_SOUNDCNT &= ~0x7f;
            break;

        case GBSND_HYPERSOUND_ENABLE_COMMAND:
            setHyperSound(data);
            break;

        default:
            return;
    }
}

void gameboySoundCommandHandler(u32 command, void* userdata) {
    sharedData->fifosReceived++;
    doCommand(command);
}

void installGameboySoundFIFO() {
    fifoSetValue32Handler(FIFO_USER_01, gameboySoundCommandHandler, 0);
    sharedData->cycles = -1;
    sharedData->fifosSent = 0;
    sharedData->fifosReceived = 0;
    sharedData->frameFlip_DS = 0;
    sharedData->frameFlip_Gameboy = 0;
    setHyperSound(1);

    // Continuously play this channel.
    // By simply existing, this allows for games to adjust global volume to make 
    // certain complex sound effects - even when all other channels are muted.
    int i;
    for (i=0; i<16; i++)
        backgroundSample[i] = 0x7f;

    SCHANNEL_SOURCE(1) = (u32)backgroundSample;
    SCHANNEL_REPEAT_POINT(1) = 0;
    SCHANNEL_LENGTH(1) = 16>>2;
    SCHANNEL_CR(1) = SCHANNEL_ENABLE | SOUND_VOL(0xff) | 
        SOUND_PAN(64) | (0 << 29) | SOUND_REPEAT;
}

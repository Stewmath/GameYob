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
u32 schannelCR[4];

u8 backgroundSample[16];

bool currentLfsr;

// Use this with the 7-bit lfsr.
// Note: On a gameboy it's 127 samples, but on a ds, # samples must be a 
// multiple of 4. Previously I just repeated this 4 times. But that doesn't work 
// when the frequency is above 0xffff, apparently? So one extra byte is 
// appended instead. (I still have mild problems with frequencies >0xffff.)
u8 lfsr7NoiseSample[] ALIGN(4) = {
0x80, 0x80, 0x80, 0x80, 0x80, 0x7f, 0x80, 0x80, 0x80, 0x80, 0x80, 0x7f, 0x7f, 0x80, 0x80, 0x80, 0x80, 0x7f, 0x80, 0x7f, 0x80, 0x80, 0x80, 0x7f, 0x7f, 0x7f, 0x7f, 0x80, 0x80, 0x7f, 0x80, 0x80, 0x80, 0x7f, 0x80, 0x7f, 0x7f, 0x80, 0x80, 0x7f, 0x7f, 0x7f, 0x80, 0x7f, 0x80, 0x7f, 0x80, 0x80, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x80, 0x7f, 0x80, 0x80, 0x80, 0x80, 0x7f, 0x7f, 0x7f, 0x80, 0x80, 0x80, 0x7f, 0x80, 0x80, 0x7f, 0x80, 0x80, 0x7f, 0x7f, 0x80, 0x7f, 0x7f, 0x80, 0x7f, 0x80, 0x7f, 0x7f, 0x80, 0x7f, 0x7f, 0x7f, 0x7f, 0x80, 0x7f, 0x7f, 0x80, 0x80, 0x80, 0x7f, 0x7f, 0x80, 0x7f, 0x80, 0x80, 0x7f, 0x80, 0x7f, 0x7f, 0x7f, 0x80, 0x7f, 0x7f, 0x7f, 0x80, 0x80, 0x7f, 0x7f, 0x80, 0x80, 0x7f, 0x80, 0x7f, 0x80, 0x7f, 0x80, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x80, 0x7f
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


void setChannelVolume(int c, bool write) {
    int channel = channels[c];

    int volume = sharedData->chanRealVol[c]*2;
    if (sharedData->chanPan[c] >= 128)
        volume = 0;
    else if (sharedData->chanPan[c] == 64) {
        volume *= 2;   // DS divides sound by 2 for each speaker; gameboy doesn't
    }

    schannelCR[c] &= ~0x7f;
    schannelCR[c] |= volume;
    if (write) {
        SCHANNEL_CR(channel) &= ~0x7f;
        SCHANNEL_CR(channel) |= volume;
    }
}
void updateChannel(int c, bool write) {
    int channel = channels[c];

    if (!(sharedData->chanOn & (1<<c)) || !sharedData->chanEnabled[c]) {
        schannelCR[c] &= ~0x7f;
        SCHANNEL_CR(channel) &= ~0x7f; // Set volume to zero
        return;
    }

    SCHANNEL_TIMER(channel) = SOUND_FREQ(sharedData->chanRealFreq[c]);
    if (c < 2) {
        schannelCR[c] &= ~(SOUND_PAN(127) | 7<<24);
        schannelCR[c] |= SOUND_PAN(sharedData->chanPan[c]) | (dutyIndex[sharedData->chanDuty[c]] << 24);
    }
    else if (c == 2) {
        schannelCR[c] &= ~(SOUND_PAN(127));
        schannelCR[c] |= SOUND_PAN(sharedData->chanPan[c]);
    }
    else if (c == 3) {
        if (currentLfsr != sharedData->lfsr7Bit)
            startChannel(c);
        schannelCR[c] &= ~(SOUND_PAN(127));
        schannelCR[c] |= SOUND_PAN(sharedData->chanPan[c]);
    }
    if (write) {
        SCHANNEL_CR(channel) &= ~(SOUND_PAN(127) | 7<<24);
        SCHANNEL_CR(channel) |= (schannelCR[c] & (SOUND_PAN(127) | 7<<24));
    }
    setChannelVolume(c, write);
}


void startChannel(int c) {
    int channel = channels[c];

    if (!sharedData->chanEnabled[c]) {
        SCHANNEL_CR(channel) = 0;
        return;
    }

    if (c == 2) {
        SCHANNEL_SOURCE(channel) = (u32)sharedData->sampleData;
        SCHANNEL_REPEAT_POINT(channel) = 0;
        SCHANNEL_LENGTH(channel) = 0x20>>2;
        schannelCR[c] = (0 << 29) | SOUND_REPEAT;
    }
    else if (c == 3) {
        SCHANNEL_CR(channel) = 0; // Why does this help? It seems to make other channels worse.

        currentLfsr = sharedData->lfsr7Bit;
        if (sharedData->lfsr7Bit) {
            SCHANNEL_SOURCE(channel) = (u32)lfsr7NoiseSample;
            SCHANNEL_LENGTH(channel) = 128>>2;
        }
        else {
            SCHANNEL_SOURCE(channel) = (u32)lfsr15NoiseSample;
            SCHANNEL_LENGTH(channel) = 32768>>2;
        }
        SCHANNEL_REPEAT_POINT(channel) = 0;
        schannelCR[c] = (0 << 29) | SOUND_REPEAT;
    }
    else { // PSG channels
        schannelCR[c] = (3 << 29);
    }

    updateChannel(c, false);

    SCHANNEL_CR(channel) = schannelCR[c] | SCHANNEL_ENABLE;
}

void updateMasterVolume() {
    // TODO: when SO1Vol != SO2Vol
    int SO1Vol = sharedData->volControl & 7;
    if (SO1Vol*18 != (REG_SOUNDCNT & 0x7f)) {
        REG_SOUNDCNT &= ~0x7f;
        REG_SOUNDCNT |= SO1Vol*18;
    }

    // Each sound channel enabled in NR51 adds a bit of a "background tone".
    // I'm not really sure if this behaviour is correct.
    // I'm trying to strike a balance between the "Warlocked/Perfect Dark/Alone 
    // in the Dark" sound effects, and the annoying clicking in some games.
    int vol=0;
    int i;
    for (i=0; i<4; i++) {
        if (sharedData->chanOutput & ((1<<i) | (1<<(i+4))) )
            vol += 0x20;
    }
    if (vol == 0x80)
        vol = 0x7f;
    SCHANNEL_CR(1) &= ~0x7f;
    if (sharedData->chanOutput)
        SCHANNEL_CR(1) |= vol;
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

        case GBSND_UPDATE_COMMAND:
            if (data == 4) {
                for (i=0; i<4; i++) {
                    updateChannel(i, true);
                }
            }
            else
                updateChannel(data, true);
            break;

        case GBSND_START_COMMAND:
            startChannel(data);
            break;

        case GBSND_VOLUME_COMMAND:
            setChannelVolume(data, true);
            break;

        case GBSND_MASTER_VOLUME_COMMAND:
            updateMasterVolume();
            break;

        case GBSND_KILL_COMMAND:
            //SCHANNEL_CR(channel) &= ~SCHANNEL_ENABLE;
            break;

        case GBSND_MUTE_COMMAND:
            // This does not touch the "background hum" to prevent clicking.
            for (i=0; i<4; i++) {
                SCHANNEL_CR(channels[i]) &= ~0x7f;
            }
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

    int i;
    for (i=0; i<16; i++)
        backgroundSample[i] = 0x7f;

    // The gameboy produces a sort of background "hum".
    // By simply existing, this allows for games to adjust global volume to make 
    // certain complex sound effects - even when all other channels are muted.
    // Eg. Warlocked, Perfect Dark.
    // This is the cause of "clicking" in certain games. It exists to an extent 
    // on real gameboys as well.
    SCHANNEL_SOURCE(1) = (u32)backgroundSample;
    SCHANNEL_REPEAT_POINT(1) = 0;
    SCHANNEL_LENGTH(1) = 16>>2;
    SCHANNEL_CR(1) = SCHANNEL_ENABLE | SOUND_VOL(0) | 
        SOUND_PAN(64) | (0 << 29) | SOUND_REPEAT;
}

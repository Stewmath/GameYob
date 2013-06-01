/* This code is basically a few reimplementations of sound code from libnds.
 * Using FIFO channel USER_01, sound commands are passed the same as in libnds, 
 * except the channel is passed from the arm9 side. This is because random 
 * crashes occured when arm9 needed to wait for a response from arm7... 
 * apparently in some cases, a response was never received.
 */

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

u8 popSample[4];

// Use this with the 7-bit lfsr
u8 noiseSample[] = { 0x80, 0x80, 0x80, 0x80, 0x80, 0, 0x80, 0x80, 0x80, 0x80, 0x80, 0, 0, 0x80, 0x80, 0x80, 0x80, 0, 0x80, 0, 0x80, 0x80, 0x80, 0, 0, 0, 0, 0x80, 0x80, 0, 0x80, 0x80, 0x80, 0, 0x80, 0, 0, 0x80, 0x80, 0, 0, 0, 0x80, 0, 0x80, 0, 0x80, 0x80, 0, 0, 0, 0, 0, 0x80, 0, 0x80, 0x80, 0x80, 0x80, 0, 0, 0, 0x80, 0x80, 0x80, 0, 0x80, 0x80, 0, 0x80, 0x80, 0, 0, 0x80, 0, 0, 0x80, 0, 0x80, 0, 0, 0x80, 0, 0, 0, 0, 0x80, 0, 0, 0x80, 0x80, 0x80, 0, 0, 0x80, 0, 0x80, 0x80, 0, 0x80, 0, 0, 0, 0x80, 0, 0, 0, 0x80, 0x80, 0, 0, 0x80, 0x80, 0, 0x80, 0, 0x80, 0, 0x80, 0, 0, 0, 0, 0, 0, 0, 0x80};

void setChannelVolume(int c) {
    int channel = channels[c];

    int volume = sharedData->chanRealVol[c]*2;
    if (sharedData->chanPan[c] >= 128)
        volume = 0;
    else if (sharedData->chanPan[c] == 64) {
        volume *= 2;   // DS divides sound by 2 for each speaker; gameboy doesn't
    }

    SCHANNEL_CR(channel) &= ~0xff;
    SCHANNEL_CR(channel) |= volume;
}
void updateChannel(int c) {
    int channel = channels[c];

    if (!(sharedData->chanOn & (1<<c))) {
        SCHANNEL_CR(channel) &= ~SCHANNEL_ENABLE;
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
        SCHANNEL_CR(channel) &= ~(SOUND_PAN(127));
        SCHANNEL_CR(channel) |= SOUND_PAN(sharedData->chanPan[c]);
    }
    setChannelVolume(c);
}


void startChannel(int c) {
    int channel = channels[c];
    SCHANNEL_CR(channel) &= ~SCHANNEL_ENABLE;

    if (!sharedData->chanEnabled[c])
        return;

    if (c == 2) {
        SCHANNEL_SOURCE(channel) = (u32)sharedData->sampleData;
        SCHANNEL_REPEAT_POINT(channel) = 0;
        SCHANNEL_LENGTH(channel) = 0x20>>2;
        SCHANNEL_CR(channel) = (0 << 29) | SOUND_REPEAT;
    }
    else if (c == 3) {
        if (sharedData->lfsr7Bit) {
            SCHANNEL_SOURCE(channel) = noiseSample;
            SCHANNEL_REPEAT_POINT(channel) = 0;
            SCHANNEL_LENGTH(channel) = 128>>2;
            SCHANNEL_CR(channel) = (0 << 29) | SOUND_REPEAT;
        }
        else
            SCHANNEL_CR(channel) = (3 << 29);
    }
    else {
        SCHANNEL_CR(channel) = (3 << 29);
    }

    updateChannel(c);
    SCHANNEL_CR(channel) |= SCHANNEL_ENABLE;
}
void gameboySoundDataHandler(int bytes, void *user_data) {
    GbSndMessage msg;

    fifoGetDatamsg(FIFO_USER_01, bytes, (u8*)&msg);

    u8 channel = msg.channel;

    switch(msg.type) {
        case GBSND_PLAY_MESSAGE:
            SCHANNEL_SOURCE(channel) = (u32)msg.SoundPlay.data;
            SCHANNEL_REPEAT_POINT(channel) = msg.SoundPlay.loopPoint;
            SCHANNEL_LENGTH(channel) = msg.SoundPlay.dataSize;
            SCHANNEL_TIMER(channel) = SOUND_FREQ(msg.SoundPlay.freq);
            SCHANNEL_CR(channel) = SCHANNEL_ENABLE | SOUND_VOL(msg.SoundPlay.volume) | 
                SOUND_PAN(msg.SoundPlay.pan) | (msg.SoundPlay.format << 29) | (msg.SoundPlay.loop ? SOUND_REPEAT : SOUND_ONE_SHOT);
            break;

        case GBSND_PSG_MESSAGE:
            SCHANNEL_CR(channel) = SCHANNEL_ENABLE | msg.SoundPsg.volume | 
                SOUND_PAN(msg.SoundPsg.pan) | (3 << 29) | (msg.SoundPsg.dutyCycle << 24);
            SCHANNEL_TIMER(channel) = SOUND_FREQ(msg.SoundPsg.freq);
            break;

        case GBSND_NOISE_MESSAGE:
            SCHANNEL_CR(channel) = SCHANNEL_ENABLE | msg.SoundPsg.volume | SOUND_PAN(msg.SoundPsg.pan) | (3 << 29);
            SCHANNEL_TIMER(channel) = SOUND_FREQ(msg.SoundPsg.freq);
            break;

        case GBSND_MODIFY_MESSAGE:
            SCHANNEL_TIMER(channel) = SOUND_FREQ(msg.SoundModify.freq);
            if ((SCHANNEL_CR(channel)&0xff) != msg.SoundModify.volume) {
                SCHANNEL_CR(channel) &= ~0xff;
                SCHANNEL_CR(channel) |= msg.SoundModify.volume;
            }
            break;

        case GBSND_KILL_MESSAGE:
            SCHANNEL_CR(channel) &= ~SCHANNEL_ENABLE;
            break;

        default:
            return;
    }
}

void doCommand(u32 command) {
	int cmd = command>>28;
    int channel = (command>>24)&0xf;
	int data = command & 0xFFFFFF;
	
    switch(cmd) {

        case GBSND_CHECK_COMMAND:
            //sharedData->cycles = -1;
            gameboySoundCommandHandler(sharedData->message, 0);
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
            {
                // TODO: when SO1Vol != SO2Vol
                /*
                int bias = (sharedData->SO1Vol-sharedData->SO2Vol)*0x200/7+0x200;
                if (bias == 0x400)
                    bias = 0x3ff;
                    */
                REG_SOUNDCNT &= ~0x7f;
                REG_SOUNDCNT |= sharedData->SO1Vol*16;
                //REG_SOUNDBIAS = bias;
            }
            break;

        case GBSND_KILL_COMMAND:
            SCHANNEL_CR(channel) &= ~SCHANNEL_ENABLE;
            break;

        default:
            return;
    }
}

void gameboySoundCommandHandler(u32 command, void* userdata) {
    doCommand(command);
    sharedData->fifosWaiting--;
}

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

void installGameboySoundFIFO() {
    fifoSetDatamsgHandler(FIFO_USER_01, gameboySoundDataHandler, 0);
    fifoSetValue32Handler(FIFO_USER_01, gameboySoundCommandHandler, 0);
    sharedData->cycles = -1;
    sharedData->fifosWaiting = 0;
    sharedData->frameFlip_DS = 0;
    sharedData->frameFlip_Gameboy = 0;
    timerStart(1, ClockDivider_1, TIMER_FREQ(4194304/SOUND_RESOLUTION), timerCallback);

    // Continuously play this channel.
    // By simply existing, this allows for games to adjust global volume to make 
    // certain complex sound effects - even when all other channels are muted.
    int i;
    for (i=0; i<4; i++)
        popSample[i] = 0x7f;

    SCHANNEL_SOURCE(1) = (u32)popSample;
    SCHANNEL_REPEAT_POINT(1) = 0;
    SCHANNEL_LENGTH(1) = 4>>2;
    SCHANNEL_CR(1) = SCHANNEL_ENABLE | SOUND_VOL(0xff) | 
        SOUND_PAN(64) | (0 << 29) | SOUND_REPEAT;
}

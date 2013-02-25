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

void gameboySoundDataHandler(int bytes, void *user_data) {
    int channel = -1;

    FifoMessage msg;

    fifoGetDatamsg(FIFO_USER_01, bytes, (u8*)&msg);

    if(msg.type == SOUND_PLAY_MESSAGE) {

        channel = msg.SoundPlay.channel;

        SCHANNEL_SOURCE(channel) = (u32)msg.SoundPlay.data;
        SCHANNEL_REPEAT_POINT(channel) = msg.SoundPlay.loopPoint;
        SCHANNEL_LENGTH(channel) = msg.SoundPlay.dataSize;
        SCHANNEL_TIMER(channel) = SOUND_FREQ(msg.SoundPlay.freq);
        SCHANNEL_CR(channel) = SCHANNEL_ENABLE | SOUND_VOL(msg.SoundPlay.volume) | SOUND_PAN(msg.SoundPlay.pan) | (msg.SoundPlay.format << 29) | (msg.SoundPlay.loop ? SOUND_REPEAT : SOUND_ONE_SHOT);

    } else if(msg.type == SOUND_PSG_MESSAGE) {
        channel = msg.SoundPsg.channel;

        SCHANNEL_CR(channel) = SCHANNEL_ENABLE | msg.SoundPsg.volume | SOUND_PAN(msg.SoundPsg.pan) | (3 << 29) | (msg.SoundPsg.dutyCycle << 24);
        SCHANNEL_TIMER(channel) = SOUND_FREQ(msg.SoundPsg.freq);
    } else if(msg.type == SOUND_NOISE_MESSAGE) {

        channel = msg.SoundPsg.channel;

        if(channel >= 0) {	
            SCHANNEL_CR(channel) = SCHANNEL_ENABLE | msg.SoundPsg.volume | SOUND_PAN(msg.SoundPsg.pan) | (3 << 29);
            SCHANNEL_TIMER(channel) = SOUND_FREQ(msg.SoundPsg.freq);
        }
    }

	//fifoSendValue32(FIFO_USER_01, (u32)channel);
}

void installGameboySoundFIFO() {
    fifoSetDatamsgHandler(FIFO_USER_01, gameboySoundDataHandler, 0);
}

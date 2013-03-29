#include <nds.h>
#include <nds/fifomessages.h>
#include <time.h>
#include "mmu.h"
#include "main.h"
#include "gameboy.h"
#include "gbsnd.h"

#define CHAN_1 1
#define CHAN_2 2
#define CHAN_3 4
#define CHAN_4 8

inline void setChan1() {ioRam[0x26] |= 1;}
inline void clearChan1() {ioRam[0x26] &= ~1;}
inline void setChan2() {ioRam[0x26] |= 2;}
inline void clearChan2() {ioRam[0x26] &= ~2;}
inline void setChan3() {ioRam[0x26] |= 4;}
inline void clearChan3() {ioRam[0x26] &= ~4;}
inline void setChan4() {ioRam[0x26] |= 8;}
inline void clearChan4() {ioRam[0x26] &= ~8;}

bool soundDisabled=false;
int chanEnabled[] = {1,1,1,1};
// Global volume
int soundMultiplier = 4;

double chan4FreqRatio;
int chan4Width;
int chan1SweepTime;
int chan1SweepCounter;
int chan1SweepDir;
int chan1SweepAmount;
int chanDuty[2];
int chanLen[4];
int chanLenCounter[4];
int chanUseLen[4];
u32 chanFreq[4];
// Frequency converted
u32 chanRealFreq[4];
int chanOn;
int chanVol[4];
int chanEnvDir[4];
int chanEnvCounter[4];
int chanEnvSweep[4];
int chanToOut1[4];
int chanToOut2[4];
int sound[4] = {8,9,0,14};

int pcmVals[16];
u8 sampleData[0x20];

DutyCycle dutyIndex[4] = {DutyCycle_12, DutyCycle_25, DutyCycle_50, DutyCycle_75};
int SO1Vol=0;
int SO2Vol=0;

void setSoundVolume(int i);
void refreshSoundFreq(int i);
void updateSoundSample(int byte);

void playPSG(int channel, DutyCycle cycle, u32 freq, u8 volume, u8 pan){
    if (freq > 0xffff) {
        printLog("Bad PSG frequency %x\n", freq);
        freq = 0xffff;
    }
    FifoMessage msg;

    msg.type = SOUND_PSG_MESSAGE;
    msg.SoundPsg.dutyCycle = cycle;
    msg.SoundPsg.freq = freq;
    msg.SoundPsg.volume = volume;
    msg.SoundPsg.pan = pan;
    msg.SoundPsg.channel = channel;

    fifoSendDatamsg(FIFO_USER_01, sizeof(msg), (u8*)&msg);

    /*
	while(!fifoCheckValue32(FIFO_USER_01));
    int useless = fifoGetValue32(FIFO_USER_01);
    */
}
void playSample(int channel, const void* data, SoundFormat format, u32 dataSize, u32 freq, u8 volume, u8 pan, bool loop, u16 loopPoint){ 
    if (freq > 0xffff) {
        printLog("Bad sample frequency %x\n", freq);
        freq = 0xffff;
    }
    FifoMessage msg;

    msg.type = SOUND_PLAY_MESSAGE;
    msg.SoundPlay.data = data;
    msg.SoundPlay.freq = freq;
    msg.SoundPlay.volume = volume;
    msg.SoundPlay.pan = pan;
    msg.SoundPlay.loop = loop;
    msg.SoundPlay.format = format;
    msg.SoundPlay.loopPoint = loopPoint;
    msg.SoundPlay.dataSize = dataSize >> 2;
    msg.SoundPlay.channel = channel;

    fifoSendDatamsg(FIFO_USER_01, sizeof(msg), (u8*)&msg);

    /*
	while(!fifoCheckValue32(FIFO_USER_01));
    int useless = fifoGetValue32(FIFO_USER_01);
    */
}
void playNoise(int channel, u32 freq, u8 volume, u8 pan){
    if (freq > 0xffff) {
        //printLog("Bad noise frequency %x\n", freq);
        freq = 0xffff;
    }
    FifoMessage msg;

    msg.type = SOUND_NOISE_MESSAGE;
    msg.SoundPsg.freq = freq;
    msg.SoundPsg.volume = volume;
    msg.SoundPsg.pan = pan;
    msg.SoundPsg.channel = channel;

    fifoSendDatamsg(FIFO_USER_01, sizeof(msg), (u8*)&msg);

    /*
	while(!fifoCheckValue32(FIFO_USER_01));
    int useless = fifoGetValue32(FIFO_USER_01);
    */
}

void initSND()
{
    for (int i=0x27; i<=0x2f; i++)
        ioRam[i] = 0xff;

    ioRam[0x26] = 0xf1;

    ioRam[0x10] = 0x80;
    ioRam[0x11] = 0xBF;
    ioRam[0x12] = 0xF3;
    ioRam[0x14] = 0xBF;
    ioRam[0x16] = 0x3F;
    ioRam[0x17] = 0x00;
    ioRam[0x19] = 0xbf;
    ioRam[0x1a] = 0x7f;
    ioRam[0x1b] = 0xff;
    ioRam[0x1c] = 0x9f;
    ioRam[0x1e] = 0xbf;
    ioRam[0x20] = 0xff;
    ioRam[0x21] = 0x00;
    ioRam[0x22] = 0x00;
    ioRam[0x23] = 0xbf;
    ioRam[0x24] = 0x77;
    ioRam[0x25] = 0xf3;

    static double analog[] = { -1, -0.8667, -0.7334, -0.6, -0.4668, -0.3335, -0.2, -0.067, 0.0664, 0.2, 0.333, 0.4668, 0.6, 0.7334, 0.8667, 1  } ;
    int i;
    for (i=0; i<16; i++)
    {
        pcmVals[i] = analog[i]*0x70;
    }
    refreshSND();
}

void refreshSND() {
    soundEnable();
    chanOn = 0;

    // Ordering note: Writing a byte to FF26 with bit 7 set enables writes to
    // the other registers. With bit 7 unset, writes are ignored.
    handleSoundRegister(0x26, ioRam[0x26]);

    for (int i=0x10; i<=0x3F; i++) {
        if (i == 0x14 || i == 0x19 || i == 0x1e || i == 0x23)
            // Don't restart the sound channels.
            handleSoundRegister(i, ioRam[i]&~0x80);
        else
            handleSoundRegister(i, ioRam[i]);
    }
}

void enableChannel(int i) {
    chanEnabled[i] = true;
}
void disableChannel(int i) {
    chanEnabled[i] = false;
}

void setSoundVolume(int i)
{
    if (!(chanOn & (1<<i)) || !chanEnabled[i])
    {
        soundSetVolume(sound[i], 0);
        return;
    }
    int volume = 0;
    if (chanToOut1[i])
        volume += chanVol[i]*SO1Vol;
    if (chanToOut2[i])
        volume += chanVol[i]*SO2Vol;
    if (sound[i] != -1) {
        soundSetVolume(sound[i], volume/4);
    }
}

void refreshSoundFreq(int i) {
    if (i == 2) {
        chanRealFreq[2] = 65536/(2048-chanFreq[2])*32;
    }
    else if (i == 3) {
        chanRealFreq[3] = (int)(524288 / chan4FreqRatio) >> (chanFreq[3]+1);
        /*
           if (chanRealFreq[3] >= 0xffff)
           printLog("bad sound %x\n", chanRealFreq[3]);
           */
    }
}

void updateSoundSample(int byte) {
    u8 sample = ioRam[0x30+byte];
    sampleData[byte*2] = pcmVals[sample>>4];
    sampleData[byte*2+1] = pcmVals[sample&0xf];
}

void updateSound(int cycles) ITCM_CODE;

void updateSound(int cycles)
{
    if (soundDisabled)
        return;
    bool changedVol[4];
    memset(changedVol, 0, sizeof(changedVol));
    if (doubleSpeed)
        cycles >>= 1;
    if (chan1SweepTime != 0)
    {
        chan1SweepCounter -= cycles;
        if (chan1SweepCounter <= 0)
        {
            chan1SweepCounter = (clockSpeed/(128/chan1SweepTime))+chan1SweepCounter;
            chanFreq[0] += (chanFreq[0]>>chan1SweepAmount)*chan1SweepDir;
            chanRealFreq[0] = 131072/(2048-chanFreq[0])*8;
            soundSetFreq(sound[0], chanRealFreq[0]);

            if (chanFreq[0] > 0x7FF)
            {
                chanOn &= ~CHAN_1;
                clearChan1();
                changedVol[0] = true;
            }
        }
    }
    if ((chanOn & CHAN_4) && chanEnvSweep[3] != 0) {
        chanEnvCounter[3] -= cycles;
        if (chanEnvCounter[3] <= 0)
        {
            chanEnvCounter[3] = chanEnvSweep[3]*clockSpeed/64;
            chanVol[3] += chanEnvDir[3];
            if (chanVol[3] < 0)
                chanVol[3] = 0;
            if (chanVol[3] > 0xF)
                chanVol[3] = 0xF;
            changedVol[3] = true;
        }
    }
    for (int i=0; i<2; i++)
    {
        if (chanOn & (1<<i))
        {
            if (chanEnvSweep[i] != 0)
            {
                chanEnvCounter[i] -= cycles;
                if (chanEnvCounter[i] <= 0)
                {
                    chanEnvCounter[i] = chanEnvSweep[i]*clockSpeed/64;
                    chanVol[i] += chanEnvDir[i];
                    if (chanVol[i] < 0)
                        chanVol[i] = 0;
                    if (chanVol[i] > 0xF)
                        chanVol[i] = 0xF;
                    changedVol[i] = true;
                }
            }
        }
    }
    for (int i=0; i<4; i++)
    {
        if ((chanOn & (1<<i)) && chanUseLen[i])
        {
            chanLenCounter[i] -= cycles;
            if (chanLenCounter[i] <= 0)
            {
                chanOn &= ~(1<<i);
                changedVol[i] = true;
                if (i==0)
                    clearChan1();
                else if (i == 1)
                    clearChan2();
                else if (i == 2)
                    clearChan3();
                else
                    clearChan4();
            }
        }
        if (changedVol[i])
            setSoundVolume(i);
    }
}

void handleSoundRegister(u8 ioReg, u8 val)
{
    if (soundDisabled)
        return;
    // If sound is globally disabled via shutting down the APU,
    if (!(ioRam[0x26] & 0x80)
        // ignore register writes to between FF10 and FF25 inclusive.
     && ioReg >= 0x10 && ioReg <= 0x25)
            return;
    switch (ioReg)
    {
        // CHANNEL 1
        // Sweep
        case 0x10:
            chan1SweepTime = (val>>4)&0x7;
            if (chan1SweepTime != 0)
                chan1SweepCounter = clockSpeed/(128/chan1SweepTime);
            chan1SweepDir = (val&0x8) ? -1 : 1;
            chan1SweepAmount = (val&0x7);
            ioRam[0x10] = val | 0x80;
            break;
            // Length / Duty
        case 0x11:
            chanLen[0] = val&0x3F;
            chanLenCounter[0] = (64-chanLen[0])*clockSpeed/256;
            chanDuty[0] = val>>6;
            playPSG(sound[0], dutyIndex[chanDuty[0]], chanRealFreq[0], 0, 64);
            setSoundVolume(0);
            ioRam[0x11] = val;
            break;
            // Envelope
        case 0x12:
            chanVol[0] = val>>4;
            if (val & 0x8)
                chanEnvDir[0] = 1;
            else
                chanEnvDir[0] = -1;
            chanEnvSweep[0] = val&0x7;
            setSoundVolume(0);
            ioRam[0x12] = val;
            break;
            // Frequency (low)
        case 0x13:
            chanFreq[0] &= 0x700;
            chanFreq[0] |= val;
            chanRealFreq[0] = 131072/(2048-chanFreq[0])*8;
            soundSetFreq(sound[0], chanRealFreq[0]);
            ioRam[0x13] = val;
            break;
            // Frequency (high)
        case 0x14:
            chanFreq[0] &= 0xFF;
            chanFreq[0] |= (val&0x7)<<8;
            if (val & 0x80)
            {
                chanLenCounter[0] = (64-chanLen[0])*clockSpeed/256;
                chanOn |= CHAN_1;
                chanVol[0] = ioRam[0x12]>>4;
                chanRealFreq[0] = 131072/(2048-chanFreq[0])*8;
                if (chan1SweepTime != 0)
                    chan1SweepCounter = clockSpeed/(128/chan1SweepTime);
                setChan1();
            }
            if (val & 0x40)
                chanUseLen[0] = 1;
            else
                chanUseLen[0] = 0;

            playPSG(sound[0], dutyIndex[chanDuty[0]], chanRealFreq[0], 0, 64);
            setSoundVolume(0);

            ioRam[0x14] = val;
            break;
            // CHANNEL 2
            // Length / Duty
        case 0x16:
            chanLen[1] = val&0x3F;
            chanLenCounter[1] = (64-chanLen[1])*clockSpeed/256;
            chanDuty[1] = val>>6;
            playPSG(sound[1], dutyIndex[chanDuty[1]], chanRealFreq[1], 0, 64);
            setSoundVolume(1);
            ioRam[0x16] = val;
            break;
            // Envelope
        case 0x17:
            chanVol[1] = val>>4;
            if (val & 0x8)
                chanEnvDir[1] = 1;
            else
                chanEnvDir[1] = -1;
            chanEnvSweep[1] = val&0x7;
            setSoundVolume(1);
            ioRam[0x17] = val;
            break;
            // Frequency (low)
        case 0x18:
            chanFreq[1] &= 0x700;
            chanFreq[1] |= val;
            chanRealFreq[1] = 131072/(2048-chanFreq[1])*8;
            soundSetFreq(sound[1], chanRealFreq[1]);
            ioRam[0x18] = val;
            break;
            // Frequency (high)
        case 0x19:
            chanFreq[1] &= 0xFF;
            chanFreq[1] |= (val&0x7)<<8;
            if (val & 0x80)
            {
                chanLenCounter[1] = (64-chanLen[1])*clockSpeed/256;
                chanOn |= CHAN_2;
                chanVol[1] = ioRam[0x17]>>4;
                chanRealFreq[1] = 131072/(2048-chanFreq[1])*8;
                setChan2();
            }
            if (val & 0x40)
                chanUseLen[1] = 1;
            else
                chanUseLen[1] = 0;

            playPSG(sound[1], dutyIndex[chanDuty[1]], chanRealFreq[1], 0, 64);
            setSoundVolume(1);
            ioRam[0x19] = val;
            break;
            // CHANNEL 3
            // On/Off
        case 0x1A:
            if ((val & 0x80) == 0)
            {
                chanOn &= ~CHAN_3;
                clearChan3();
                //buf.clear();
            }
            else {
                chanOn |= CHAN_3;
                setChan3();
            }
            setSoundVolume(2);
            ioRam[0x1a] = val | 0x7f;
            //playChan3();
            break;
            // Length
        case 0x1B:
            chanLen[2] = val;
            chanLenCounter[2] = (256-chanLen[2])*clockSpeed/256;
            ioRam[0x1b] = val;
            break;
            // Volume
        case 0x1C:
            {
                chanVol[2] = (val>>5)&3;
                switch(chanVol[2])
                {
                    case 0:
                        break;
                    case 1:
                        chanVol[2] = 15;
                        break;
                    case 2:
                        chanVol[2] = 15>>1;
                        break;
                    case 3:
                        chanVol[2] = 15>>2;
                        break;
                }
                setSoundVolume(2);
                ioRam[0x1c] = val | 0x9f;
                break;
            }
            // Frequency (low)
        case 0x1D:
            chanFreq[2] &= 0xFF00;
            chanFreq[2] |= val;
            refreshSoundFreq(2);
            soundSetFreq(sound[2], chanRealFreq[2]);
            ioRam[0x1d] = val;
            break;
            // Frequency (high)
        case 0x1E:
            chanFreq[2] &= 0xFF;
            chanFreq[2] |= (val&7)<<8;
            if ((val & 0x80) && (ioRam[0x1A] & 0x80))
            {
                chanOn |= CHAN_3;
                chanLenCounter[2] = (256-chanLen[2])*clockSpeed/256;
                setChan3();
            }
            if (val & 0x40)
            {
                chanUseLen[2] = 1;
            }
            else
            {
                chanUseLen[2] = 0;
            }
            ioRam[0x1e] = val;
            refreshSoundFreq(2);
            playSample(sound[2], sampleData, SoundFormat_8Bit, 0x20, chanRealFreq[2], 0, 64, true, 0);
            setSoundVolume(2);
            break;
            // CHANNEL 4
            // Length
        case 0x20:
            chanLen[3] = val&0x3F;
            chanLenCounter[3] = (64-chanLen[3])*clockSpeed/256;
            ioRam[0x20] = val;
            break;
            // Volume
        case 0x21:
            chanVol[3] = val>>4;
            if (val & 0x8)
                chanEnvDir[3] = 1;
            else
                chanEnvDir[3] = -1;
            chanEnvSweep[3] = val&0x7;
            setSoundVolume(3);
            ioRam[0x21] = val;
            break;
            // Frequency
        case 0x22:
            chanFreq[3] = val>>4;
            chan4FreqRatio = val&0x7;
            if (chan4FreqRatio == 0)
                chan4FreqRatio = 0.5;
            chan4Width = !!(val&0x8);
            refreshSoundFreq(3);
            soundSetFreq(sound[3], chanRealFreq[3]);
            ioRam[0x22] = val;
            break;
            // Start
        case 0x23:
            if (val&0x80)
            {
                chanLenCounter[3] = (64-chanLen[3])*clockSpeed/256;
                chanOn |= CHAN_4;
                setChan4();
                refreshSoundFreq(3);
            }
            playNoise(sound[3], chanRealFreq[3], 0, 64);
            setSoundVolume(3);
            chanUseLen[3] = !!(val&0x40);
            ioRam[0x23] = val | 0x3f;
            break;
            // GENERAL REGISTERS
        case 0x24:
            SO1Vol = val&0x7;
            SO2Vol = (val>>4)&0x7;
            {
                int i;
                for (i=0; i<4; i++)
                {
                    setSoundVolume(i);
                }
            }
            ioRam[0x24] = val;
            break;
        case 0x25:
            chanToOut1[0] = !!(val&0x1);
            chanToOut1[1] = !!(val&0x2);
            chanToOut1[2] = !!(val&0x4);
            chanToOut1[3] = !!(val&0x8);
            chanToOut2[0] = !!(val&0x10);
            chanToOut2[1] = !!(val&0x20);
            chanToOut2[2] = !!(val&0x40);
            chanToOut2[3] = !!(val&0x80);
            {
                int i;
                for (i=0; i<4; i++)
                {
                    setSoundVolume(i);
                }
            }
            ioRam[0x25] = val;
            break;
        case 0x26:
            ioRam[0x26] &= 0x7F;
            ioRam[0x26] |= val&0x80;
            if (!(val&0x80))
            {
                chanOn = 0;
                clearChan1();
                clearChan2();
                clearChan3();
                clearChan4();
                for (int reg = 0x10; reg <= 0x25; reg++)
                    ioRam[reg] = 0;
                for (int i=0; i<4; i++)
                    setSoundVolume(i);
            }
            break;
        case 0x30:
        case 0x31:
        case 0x32:
        case 0x33:
        case 0x34:
        case 0x35:
        case 0x36:
        case 0x37:
        case 0x38:
        case 0x39:
        case 0x3A:
        case 0x3B:
        case 0x3C:
        case 0x3D:
        case 0x3E:
        case 0x3F:
            ioRam[ioReg] = val;
            updateSoundSample(ioReg-0x30);
            break;
        default:
            ioRam[ioReg] = val;
            break;
    }
}

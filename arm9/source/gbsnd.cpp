// There are 2 ways sound can be handled here.
//
// If "hyperSound" (aka "Sound Fix") is enabled, each piece of sound is 
// synchronized with ds hardware. This allows for very accurate emulation of 
// sound effects like Pikachu.
//
// If "hyperSound" is not enabled, sound is not synchronized to the cycle, it is 
// simply played as soon as it is computed.

#include <nds.h>
#include <nds/fifomessages.h>
#include <time.h>
#include "mmu.h"
#include "main.h"
#include "gameboy.h"
#include "gbsnd.h"
#include "common.h"
#include "gbcpu.h"

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

// NOTE: Don't check this variable to see if hyperSound is enabled.
// Check sharedData->hyperSound instead. It is enabled or disabled depending on 
// the situation. For instance, it is disabled when fast forwarding.
bool hyperSound=false;

int cyclesToSoundEvent=0;

double chan4FreqRatio;
int chan1SweepTime;
int chan1SweepCounter;
int chan1SweepDir;
int chan1SweepAmount;
int chanLen[4];
int chanLenCounter[4];
int chanUseLen[4];
u32 chanFreq[4];
int chanVol[4];
int chanEnvDir[4];
int chanEnvCounter[4];
int chanEnvSweep[4];

int pcmVals[16];
u8* const sampleData = (u8*)memUncached(malloc(0x20));

const DutyCycle dutyIndex[4] = {DutyCycle_12, DutyCycle_25, DutyCycle_50, DutyCycle_75};

// If this many fifo commands have been sent but not received, skip further 
// commands. This helps prevent crashing.
#define MAX_FIFOS_WAITING 60

inline void FIFO_SEND(u32 message) {
    if (sharedData->fifosSent-sharedData->fifosReceived < MAX_FIFOS_WAITING) {
        sharedData->fifosSent++;
        fifoSendValue32(FIFO_USER_01, message);
    }
}

void refreshSoundVolume(int i, bool send=false);
void refreshSoundFreq(int i);
void updateSoundSample(int byte);

// If Sound Fix is enabled, enter a loop until exactly the right moment at which 
// the sound should be updated.
// NOTE: scale transfer, which is done by arm7, tends to interfere with this.
inline void synchronizeSound() {
    int cycles = (cyclesSinceVblank+extraCycles)>>doubleSpeed;
    if (sharedData->hyperSound &&
            (!sharedData->scalingOn || !sharedData->scaleTransferReady) && // Scale transfer eats up a lot of arm7's time
            !(sharedData->frameFlip_Gameboy != sharedData->frameFlip_DS || sharedData->dsCycles >= cycles)) {

        sharedData->cycles = cycles;
        while (sharedData->cycles != -1) { // Wait for arm7 to set sharedData->cycles.
            if (sharedData->scaleTransferReady) { // Is arm7 doing the scale transfer? If so ABORT
                sharedData->cycles = -1;
                goto sendByFifo;
            }
        }
    }
    else {
sendByFifo:
        FIFO_SEND(sharedData->message);
    }
}

void sendStartMessage(int i) {
    sharedData->message = GBSND_START_COMMAND<<20 | i;
    synchronizeSound();
}

void sendUpdateMessage(int i) {
    if (i == -1)
        i = 4;
    sharedData->message = GBSND_UPDATE_COMMAND<<20 | i;
    synchronizeSound();
}

void sendGlobalVolumeMessage() {
    sharedData->message = GBSND_MASTER_VOLUME_COMMAND<<20;
    synchronizeSound();
}

void refreshSoundPan(int i) {
    if ((sharedData->chanOutput & (1<<i)) && (sharedData->chanOutput & (1<<(i+4))))
        sharedData->chanPan[i] = 64;
    else if (sharedData->chanOutput & (1<<i))
        sharedData->chanPan[i] = 127;
    else if (sharedData->chanOutput & (1<<(i+4)))
        sharedData->chanPan[i] = 0;
    else {
        sharedData->chanPan[i] = 128;   // Special signal
    }
}

void refreshSoundVolume(int i, bool send)
{
    if (!(sharedData->chanOn & (1<<i)) || !sharedData->chanEnabled[i])
    {
        return;
    }

    int volume = chanVol[i];

    if (send && sharedData->chanRealVol[i] != volume) {
        sharedData->chanRealVol[i] = volume;
        sharedData->message = GBSND_VOLUME_COMMAND<<20 | i;
        synchronizeSound();
    }
    else
        sharedData->chanRealVol[i] = volume;
}

void refreshSoundFreq(int i) {
    int freq=0;
    if (i <= 1) {
        freq = 131072/(2048-chanFreq[i])*8;
    }
    else if (i == 2) {
        freq = 65536/(2048-chanFreq[i])*32;
    }
    else if (i == 3) {
        freq = (int)(524288 / chan4FreqRatio) >> (chanFreq[i]+1);
        //printLog("%.2x: Freq %x\n", ioRam[0x22], freq);
    }
    sharedData->chanRealFreq[i] = freq;
}

void refreshSoundDuty(int i) {
    if ((sharedData->chanOn & (1<<i))) {
    }
}


void initSND()
{
    // Send the cached mirror to preserve dsi compatibility.
    // Because of this, on arm9, always use the local sampleData which is 
    // uncached.
    sharedData->sampleData = (u8*)memCached(sampleData);
    for (int i=0x27; i<=0x2f; i++)
        ioRam[i] = 0xff;

    ioRam[0x26] = 0xf0;

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
    if (soundDisabled)
        return;

    sharedData->chanOn = 0;

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

    if (ioRam[0x26] & 1)
        handleSoundRegister(0x14, ioRam[0x14]|0x80);
    if (ioRam[0x26] & 2)
        handleSoundRegister(0x19, ioRam[0x19]|0x80);
    if (ioRam[0x26] & 4)
        handleSoundRegister(0x1e, ioRam[0x1e]|0x80);
    if (ioRam[0x26] & 8)
        handleSoundRegister(0x23, ioRam[0x23]|0x80);
}

void enableChannel(int i) {
    sharedData->chanEnabled[i] = true;
}
void disableChannel(int i) {
    sharedData->chanEnabled[i] = false;
}


void updateSoundSample(int byte) {
    u8 sample = ioRam[0x30+byte];
    sampleData[byte*2] = pcmVals[sample>>4];
    sampleData[byte*2+1] = pcmVals[sample&0xf];
}

void setSoundEventCycles(int cycles) {
    if (cyclesToSoundEvent > cycles) {
        cyclesToSoundEvent = cycles;
        setEventCycles(cyclesToSoundEvent);
    }
}

void updateSound(int cycles) ITCM_CODE;

void updateSound(int cycles)
{
    if (soundDisabled)
        return;
    bool changed=false;
    if ((sharedData->chanOn & CHAN_1) && chan1SweepTime != 0)
    {
        chan1SweepCounter -= cycles;
        while (chan1SweepCounter <= 0)
        {
            chan1SweepCounter += (clockSpeed/(128/chan1SweepTime))+chan1SweepCounter;
            chanFreq[0] += (chanFreq[0]>>chan1SweepAmount)*chan1SweepDir;
            if (chanFreq[0] > 0x7FF)
            {
                sharedData->chanOn &= ~CHAN_1;
                clearChan1();
            }
            else {
                refreshSoundFreq(0);
            }
            changed = true;
        }

        if (sharedData->chanOn & CHAN_1)
            setSoundEventCycles(chan1SweepCounter);
    }
    for (int i=0; i<4; i++)
    {
        if (i != 2 && sharedData->chanOn & (1<<i))
        {
            if (chanEnvSweep[i] != 0)
            {
                chanEnvCounter[i] -= cycles;
                while (chanEnvCounter[i] <= 0)
                {
                    chanEnvCounter[i] += chanEnvSweep[i]*clockSpeed/64;
                    chanVol[i] += chanEnvDir[i];
                    if (chanVol[i] < 0)
                        chanVol[i] = 0;
                    if (chanVol[i] > 0xF)
                        chanVol[i] = 0xF;
                    changed = true;
                    refreshSoundVolume(i);
                }
                if (chanVol[i] != 0 && chanVol[i] != 0xF)
                    setSoundEventCycles(chanEnvCounter[i]);
            }
        }
    }
    for (int i=0; i<4; i++)
    {
        if ((sharedData->chanOn & (1<<i)) && chanUseLen[i])
        {
            chanLenCounter[i] -= cycles;
            if (chanLenCounter[i] <= 0)
            {
                sharedData->chanOn &= ~(1<<i);
                changed = true;
                if (i==0)
                    clearChan1();
                else if (i == 1)
                    clearChan2();
                else if (i == 2)
                    clearChan3();
                else
                    clearChan4();
            }
            else
                setSoundEventCycles(chanLenCounter[i]);
        }
    }
    if (changed) {
        if (hyperSound)
            sendUpdateMessage(-1);
        else {
            // Force immediate update, even though hyperSound isn't on.
            FIFO_SEND(GBSND_UPDATE_COMMAND<<28 | 4);
        }
    }
}

void soundUpdateVBlank() {
    // This debug stuff helps when debugging Pokemon Diamond
    //printLog("%d\n", sharedData->fifosSent-sharedData->fifosReceived);
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
            if (chan1SweepTime != 0) {
                chan1SweepCounter = clockSpeed/(128/chan1SweepTime);
                setSoundEventCycles(chan1SweepCounter);
            }
            chan1SweepDir = (val&0x8) ? -1 : 1;
            chan1SweepAmount = (val&0x7);
            ioRam[0x10] = val | 0x80;
            break;
            // Length / Duty
        case 0x11:
            {
                chanLen[0] = val&0x3F;
                chanLenCounter[0] = (64-chanLen[0])*clockSpeed/256;
                if (chanUseLen[0])
                    setSoundEventCycles(chan1SweepCounter);
                sharedData->chanDuty[0] = val>>6;
                refreshSoundDuty(0);
                sendUpdateMessage(0);
                ioRam[0x11] = val;
                break;
            }
            // Envelope / Volume
        case 0x12:
            chanVol[0] = val>>4;
            if (val & 0x8)
                chanEnvDir[0] = 1;
            else
                chanEnvDir[0] = -1;
            chanEnvSweep[0] = val&0x7;
            refreshSoundVolume(0, true);
            ioRam[0x12] = val;
            break;
            // Frequency (low)
        case 0x13:
            chanFreq[0] &= 0x700;
            chanFreq[0] |= val;
            refreshSoundFreq(0);
            sendUpdateMessage(0);
            ioRam[0x13] = val;
            break;
            // Start / Frequency (high)
        case 0x14:
            chanFreq[0] &= 0xFF;
            chanFreq[0] |= (val&0x7)<<8;
            refreshSoundFreq(0);

            if (val & 0x40)
                chanUseLen[0] = 1;
            else
                chanUseLen[0] = 0;

            if (val & 0x80)
            {
                chanLenCounter[0] = (64-chanLen[0])*clockSpeed/256;
                if (chanUseLen[0])
                    setSoundEventCycles(chanLenCounter[0]);

                sharedData->chanOn |= CHAN_1;
                chanVol[0] = ioRam[0x12]>>4;
                if (chan1SweepTime != 0) {
                    chan1SweepCounter = clockSpeed/(128/chan1SweepTime);
                    setSoundEventCycles(chan1SweepCounter);
                }
                setChan1();

                refreshSoundVolume(0, false);
                sendStartMessage(0);
            }
            else
                sendUpdateMessage(0);

            ioRam[0x14] = val;
            break;
            // CHANNEL 2
            // Length / Duty
        case 0x16:
            chanLen[1] = val&0x3F;
            chanLenCounter[1] = (64-chanLen[1])*clockSpeed/256;
            if (chanUseLen[1])
                setSoundEventCycles(chanLenCounter[1]);
            sharedData->chanDuty[1] = val>>6;
            sendUpdateMessage(1);
            ioRam[0x16] = val;
            break;
            // Volume / Envelope
        case 0x17:
            chanVol[1] = val>>4;
            if (val & 0x8)
                chanEnvDir[1] = 1;
            else
                chanEnvDir[1] = -1;
            chanEnvSweep[1] = val&0x7;
            refreshSoundVolume(1, true);
            ioRam[0x17] = val;
            break;
            // Frequency (low)
        case 0x18:
            chanFreq[1] &= 0x700;
            chanFreq[1] |= val;

            refreshSoundFreq(1);
            sendUpdateMessage(1);
            ioRam[0x18] = val;
            break;
            // Start / Frequency (high)
        case 0x19:
            chanFreq[1] &= 0xFF;
            chanFreq[1] |= (val&0x7)<<8;
            refreshSoundFreq(1);

            if (val & 0x40)
                chanUseLen[1] = 1;
            else
                chanUseLen[1] = 0;

            if (val & 0x80)
            {
                chanLenCounter[1] = (64-chanLen[1])*clockSpeed/256;
                if (chanUseLen[1])
                    setSoundEventCycles(chanLenCounter[1]);
                sharedData->chanOn |= CHAN_2;
                chanVol[1] = ioRam[0x17]>>4;
                setChan2();

                refreshSoundVolume(1, false);
                sendStartMessage(1);
            }
            else
                sendUpdateMessage(1);

            ioRam[0x19] = val;
            break;
            // CHANNEL 3
            // On/Off
        case 0x1A:
            if ((val & 0x80) == 0)
            {
                sharedData->chanOn &= ~CHAN_3;
                clearChan3();
            }
            sendUpdateMessage(2);
            ioRam[0x1a] = val | 0x7f;
            break;
            // Length
        case 0x1B:
            chanLen[2] = val;
            chanLenCounter[2] = (256-chanLen[2])*clockSpeed/256;
            if (chanUseLen[2])
                setSoundEventCycles(chanLenCounter[2]);
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
                refreshSoundVolume(2, true);
                ioRam[0x1c] = val | 0x9f;
                break;
            }
            // Frequency (low)
        case 0x1D:
            chanFreq[2] &= 0xFF00;
            chanFreq[2] |= val;
            refreshSoundFreq(2);
            sendUpdateMessage(2);
            ioRam[0x1d] = val;
            break;
            // Start / Frequency (high)
        case 0x1E:
            chanFreq[2] &= 0xFF;
            chanFreq[2] |= (val&7)<<8;
            refreshSoundFreq(2);

            if (val & 0x40)
                chanUseLen[2] = 1;
            else
                chanUseLen[2] = 0;

            if (val & 0x80)
            {
                sharedData->chanOn |= CHAN_3;
                chanLenCounter[2] = (256-chanLen[2])*clockSpeed/256;
                if (chanUseLen[2])
                    setSoundEventCycles(chanLenCounter[2]);
                setChan3();

                refreshSoundVolume(2, false);
                sendStartMessage(2);
            }
            else {
                sendUpdateMessage(2);
            }
            ioRam[0x1e] = val;
            break;
            // CHANNEL 4
            // Length
        case 0x20:
            chanLen[3] = val&0x3F;
            chanLenCounter[3] = (64-chanLen[3])*clockSpeed/256;
            if (chanUseLen[3])
                setSoundEventCycles(chanLenCounter[3]);
            ioRam[0x20] = val;
            break;
            // Volume / Envelope
        case 0x21:
            chanVol[3] = val>>4;
            if (val & 0x8)
                chanEnvDir[3] = 1;
            else
                chanEnvDir[3] = -1;
            chanEnvSweep[3] = val&0x7;
            refreshSoundVolume(3, true);
            ioRam[0x21] = val;
            break;
            // Frequency
        case 0x22:
            ioRam[0x22] = val;
            chanFreq[3] = val>>4;
            chan4FreqRatio = val&0x7;
            if (chan4FreqRatio == 0)
                chan4FreqRatio = 0.5;
            sharedData->lfsr7Bit = val&0x8;
            refreshSoundFreq(3);
            sendUpdateMessage(3);
            break;
            // Start
        case 0x23:
            chanUseLen[3] = !!(val&0x40);
            if (val&0x80)
            {
                chanLenCounter[3] = (64-chanLen[3])*clockSpeed/256;
                if (chanUseLen[3])
                    setSoundEventCycles(chanLenCounter[3]);
                sharedData->chanOn |= CHAN_4;
                setChan4();
                chanVol[3] = ioRam[0x21]>>4;
                refreshSoundVolume(3, false);
                sendStartMessage(3);
            }
            ioRam[0x23] = val | 0x3f;
            break;
            // GENERAL REGISTERS
        case 0x24:
            if ((sharedData->volControl&0x7) != (val&0x7)) {
                sharedData->volControl = val;
                sendGlobalVolumeMessage();
            }
            else
                sharedData->volControl = val;
            ioRam[0x24] = val;
            break;
        case 0x25:
            sharedData->chanOutput = val;
            for (int i=0; i<4; i++)
                refreshSoundPan(i);
            sendUpdateMessage(-1);
            sendGlobalVolumeMessage();
            ioRam[0x25] = val;
            break;
        case 0x26:
            ioRam[0x26] &= 0x7F;
            ioRam[0x26] |= val&0x80;
            if (!(val&0x80))
            {
                sharedData->chanOn = 0;
                clearChan1();
                clearChan2();
                clearChan3();
                clearChan4();
                for (int reg = 0x10; reg <= 0x25; reg++)
                    ioRam[reg] = 0;
                sendUpdateMessage(-1);
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

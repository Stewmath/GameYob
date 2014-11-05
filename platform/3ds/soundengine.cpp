#include <3ds.h>
#include "soundengine.h"
#include "gameboy.h"
#include "menu.h"
#include "noise.h"
#include <math.h>
#include <time.h>


extern "C" Result CSND_sharedmemtype0_cmd9(u32, u32);

void CSND_setchannel_enabled(u32 channel, u32 value)
{
	u32 cmdparams[0x18>>2];

	memset(cmdparams, 0, 0x18);

	cmdparams[0] = channel & 0x1f;
	cmdparams[1] = value;

	CSND_writesharedmem_cmdtype0(0x0, (u8*)&cmdparams);
}



#define FRAMES_PER_BUFFER 3

#define CYCLES_UNTIL_SAMPLE (4*0x54)
#define CSND_FREQUENCY (CYCLES_PER_FRAME * 59.7 / CYCLES_UNTIL_SAMPLE)
#define CSND_BUFFER_SIZE ((CYCLES_PER_FRAME / CYCLES_UNTIL_SAMPLE) * FRAMES_PER_BUFFER)


s16* bufferDat = (s16*)linearAlloc(2*CSND_BUFFER_SIZE*2);

s16* buffers[2];
int playingBuffer = 0;
int recordingBuffer = 1;
int recordingPos = 0;
int framecnt;

void initSampler() {
    buffers[0] = bufferDat;
    buffers[1] = bufferDat+CSND_BUFFER_SIZE;

    playingBuffer = 0;
    recordingBuffer = 1;
    recordingPos = 0;
    framecnt = 0;

    //CSND_playsound(9, 1, CSND_ENCODING_PCM16, CSND_FREQUENCY, (u32*)buffers[playingBuffer], (u32*)buffers[playingBuffer]+CSND_BUFFER_SIZE*2-1, CSND_BUFFER_SIZE*4, 2, 0);

    CSND_setchannel_playbackstate(9, 0);

    CSND_sharedmemtype0_cmdupdatestate(0);
}

// Called once every 4 cycles
void addSample(s16 sample) {
    if (recordingPos >= CSND_BUFFER_SIZE) {
        printf("RECORD LOOP\n");
        return;
    }
    buffers[recordingBuffer][recordingPos++] = sample;
}

void swapBuffers() {
    if (--framecnt <= 0) {
        framecnt = FRAMES_PER_BUFFER;

        playingBuffer = !playingBuffer;
        recordingBuffer = !recordingBuffer;

        if (recordingPos != CSND_BUFFER_SIZE) {
            printf("recordingpos %d\n", recordingPos);
        }
        recordingPos = 0;

        u32* addr = (u32*)buffers[playingBuffer];
        CSND_playsound(8+playingBuffer, 0, CSND_ENCODING_PCM16, CSND_FREQUENCY, addr, addr, CSND_BUFFER_SIZE*2, 0, 0);
    }
}


SoundEngine::SoundEngine(Gameboy* g)
{
    setGameboy(g);
}

SoundEngine::~SoundEngine() {

}

void SoundEngine::setGameboy(Gameboy* g) {
    gameboy = g;
}

void SoundEngine::init() {
    for (int i=0; i<4; i++) {
        chanPolarity[i] = 1;
        chanPolarityCounter[i] = 0;
    }
    cyclesUntilSample = CYCLES_UNTIL_SAMPLE;
    lfsr = 0;
    chan3WavPos = 0;

    initSampler();

    refresh();
}

void SoundEngine::refresh() {
    // Ordering note: Writing a byte to FF26 with bit 7 set enables writes to
    // the other registers. With bit 7 unset, writes are ignored.
    handleSoundRegister(0x26, gameboy->readIO(0x26));

    for (int i=0x10; i<=0x3F; i++) {
        if (i == 0x14 || i == 0x19 || i == 0x1e || i == 0x23)
            // Don't restart the sound channels.
            handleSoundRegister(i, gameboy->readIO(i)&~0x80);
        else
            handleSoundRegister(i, gameboy->readIO(i));
    }

    if (gameboy->readIO(0x26) & 1)
        handleSoundRegister(0x14, gameboy->readIO(0x14)|0x80);
    if (gameboy->readIO(0x26) & 2)
        handleSoundRegister(0x19, gameboy->readIO(0x19)|0x80);
    if (gameboy->readIO(0x26) & 4)
        handleSoundRegister(0x1e, gameboy->readIO(0x1e)|0x80);
    if (gameboy->readIO(0x26) & 8)
        handleSoundRegister(0x23, gameboy->readIO(0x23)|0x80);

    unmute();
}

void SoundEngine::mute() {
    muted = true;
}

void SoundEngine::unmute() {
    muted = false;
}


void SoundEngine::updateSound(int cycles)
{
    if (soundDisabled)
        return;

	if (gameboy->doubleSpeed)
		cycles /= 2;
//	chanOn[0] = 0;
//	chanOn[1] = 0;
//	chanOn[2] = 0;
//	chanOn[3] = 0;

	if (chan1SweepTime != 0)
	{
		chan1SweepCounter -= cycles;
		if (chan1SweepCounter <= 0)
		{
			chan1SweepCounter = (clockSpeed/(128/chan1SweepTime))+chan1SweepCounter;
			chanFreq[0] += (chanFreq[0]>>chan1SweepAmount)*chan1SweepDir;

			if (chanFreq[0] > 0x7FF)
			{
				chanOn[0] = 0;
				gameboy->clearSoundChannel(CHAN_1);
			}
		}
	}
	for (int i=0; i<2; i++)
	{
		if (chanOn[i])
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
				}
			}

			if (chanUseLen[i])
			{
				chanLenCounter[i] -= cycles;
				if (chanLenCounter[i] <= 0)
				{
					chanOn[i] = 0;
					if (i==0)
						gameboy->clearSoundChannel(CHAN_1);
					else
						gameboy->clearSoundChannel(CHAN_2);
				}
			}
		}

	}

	// Channel 3
	if (chanOn[2])
	{
		if (chanUseLen[2])
		{
			chanLenCounter[2] -= cycles;
			if (chanLenCounter[2] <= 0)
			{
				chanOn[2] = 0;
				gameboy->clearSoundChannel(CHAN_3);
			}
		}
	}
	if (chanOn[3])
	{
		chanEnvCounter[3] -= cycles;
		if (chanEnvSweep[3] != 0 && chanEnvCounter[3] <= 0)
		{
			chanEnvCounter[3] = chanEnvSweep[3]*clockSpeed/64;
			chanVol[3] += chanEnvDir[3];
			if (chanVol[3] < 0)
				chanVol[3] = 0;
			if (chanVol[3] > 0xF)
				chanVol[3] = 0xF;
		}

		if (chanUseLen[3])
		{
			chanLenCounter[3] -= cycles;
			if (chanLenCounter[3] <= 0)
			{
				chanOn[3] = 0;
				gameboy->clearSoundChannel(CHAN_4);
			}
		}
	}

    cyclesUntilSample -= cycles;
    while (cyclesUntilSample <= 0) {
        int c = CYCLES_UNTIL_SAMPLE;
        cyclesUntilSample += c;

        s16 tone = 0, tone1 = 0, tone2 = 0;

        for (int j=0; j<2; j++) {
            if (chanOn[j]) {
                chanPolarityCounter[j] -= c;
                while (chanPolarityCounter[j] <= 0)
                {
                    int oldPolarityCounter = chanPolarityCounter[j];
                    chanPolarity[j] *= -1;

                    if (chanDuty[j] == 0)
                    {
                        if (chanPolarity[j] == 1)
                            chanPolarityCounter[j] = clockSpeed/((double)131072/(2048-chanFreq[j]))*(((double)1/8));
                        else
                            chanPolarityCounter[j] = clockSpeed/((double)131072/(2048-chanFreq[j]))*(((double)7/8));
                    }
                    else if (chanDuty[j] == 1)
                    {
                        if (chanPolarity[j] == 1)
                            chanPolarityCounter[j] = clockSpeed/((double)131072/(2048-chanFreq[j]))*(((double)1/4));
                        else
                            chanPolarityCounter[j] = clockSpeed/((double)131072/(2048-chanFreq[j]))*(((double)3/4));
                    }
                    else if (chanDuty[j] == 2)
                        chanPolarityCounter[j] = clockSpeed/((double)131072/(2048-chanFreq[j]))/2;
                    else if (chanDuty[j] == 3)
                    {
                        if (chanPolarity[j] == 1)
                            chanPolarityCounter[j] = clockSpeed/((double)131072/(2048-chanFreq[j]))*(((double)3/4));
                        else
                            chanPolarityCounter[j] = clockSpeed/((double)131072/(2048-chanFreq[j]))*(((double)1/4));
                    }
                    chanPolarityCounter[j] += oldPolarityCounter;
                }

                if (chanToOut1[j])
                    tone1 += chanPolarity[j] * chanVol[j];
                if (chanToOut2[j])
                    tone2 += chanPolarity[j] * chanVol[j];
            }
        }

        if (chanOn[2]) {
            chanPolarityCounter[2] -= c;
            while (chanPolarityCounter[2] <= 0)
            {
                chanPolarityCounter[2] = clockSpeed/(65536/(2048-chanFreq[2])*32) + chanPolarityCounter[2];
                //chanPolarityCounter[2] = clockSpeed/((131072/(2048-chanFreq[2]))*16);
                chanFreqClocks[2] = chanPolarityCounter[2];

                chan3WavPos++;
                if (chan3WavPos >= 32)
                    chan3WavPos = 0;
            }

            static double analog[] = { -1, -0.8667, -0.7334, -0.6, -0.4668, -0.3335, -0.2, -0.067, 0.0664, 0.2, 0.333, 0.4668, 0.6, 0.7334, 0.8667, 1  } ;

            if (chanVol[2] >= 0)
            {
                int wavTone = gameboy->ioRam[0x30+(chan3WavPos/2)];
                wavTone = chan3WavPos%2? wavTone&0xF : wavTone>>4;
                if (chanToOut1[2])
                    tone1 += (analog[wavTone])*(0xF >> chanVol[2]);
                if (chanToOut2[2])
                    tone2 += (analog[wavTone])*(0xF >> chanVol[2]);
            }

        }
        if (chanOn[3]) {
            int polarityLen = clockSpeed/((int)(524288 / chan4FreqRatio) >> (chanFreq[3]+1));
            chanPolarityCounter[3] -= c;
            int flips = -(chanPolarityCounter[3] - polarityLen) / polarityLen;
            chanPolarityCounter[3] += flips*polarityLen;

            lfsr += flips;
            lfsr &= 32768-1; // TODO: double-check this

            if (lfsr15NoiseSample[lfsr] == 0xa0)
                chanPolarity[3] = 1;
            else
                chanPolarity[3] = -1;

            if (chanToOut1[3])
                tone1 += chanPolarity[3]*chanVol[3];
            if (chanToOut2[3])
                tone2 += chanPolarity[3]*chanVol[3];
        }

        tone1 *= SO1Vol;
        tone2 *= SO2Vol;

        tone = tone1 + tone2;

        addSample(tone*0x10);
    }
}


void SoundEngine::setSoundEventCycles(int cycles) {
    if (cyclesToSoundEvent > cycles) {
        cyclesToSoundEvent = cycles;
    }
}

void SoundEngine::soundUpdateVBlank() {
    swapBuffers();
}

void SoundEngine::updateSoundSample() {
}


void SoundEngine::handleSoundRegister(u8 ioReg, u8 val)
{
	switch (ioReg)
	{
		// CHANNEL 1
		// Sweep
		case 0x10:
			//if (val&7 != 0)
			//	printf("sweep\n");
			chan1SweepTime = (val>>4)&0x7;
			if (chan1SweepTime != 0)
				chan1SweepCounter = clockSpeed/(128/chan1SweepTime);
			chan1SweepDir = (val&0x8) ? -1 : 1;
			chan1SweepAmount = (val&0x7);
			break;
		// Length / Duty
		case 0x11:
			chanLen[0] = val&0x3F;
			chanLenCounter[0] = (64-chanLen[0])*clockSpeed/256;
			chanDuty[0] = val>>6;
			break;
		// Envelope
		case 0x12:
			chanVol[0] = val>>4;
			if (val & 0x8)
				chanEnvDir[0] = 1;
			else
				chanEnvDir[0] = -1;
			chanEnvSweep[0] = val&0x7;
			break;
		// Frequency (low)
		case 0x13:
			chanFreq[0] &= 0x700;
			chanFreq[0] |= val;
			break;
		// Frequency (high)
		case 0x14:
			chanFreq[0] &= 0xFF;
			chanFreq[0] |= (val&0x7)<<8;
			if (val & 0x80)
			{
				chanLenCounter[0] = (64-chanLen[0])*clockSpeed/256;
				chanOn[0] = 1;
				chanVol[0] = gameboy->ioRam[0x12]>>4;
				if (chan1SweepTime != 0)
					chan1SweepCounter = clockSpeed/(128/chan1SweepTime);
				gameboy->setSoundChannel(CHAN_1);

                //CSND_playsound(0, true, CSND_ENCODING_PSG, convertFreq(freq), NULL, NULL, 0, 0, 0);
			}
			if (val & 0x40)
				chanUseLen[0] = 1;
			else
				chanUseLen[0] = 0;
			break;
		// CHANNEL 2
		// Length / Duty
		case 0x16:
			chanLen[1] = val&0x3F;
			chanLenCounter[1] = (64-chanLen[1])*clockSpeed/256;
			chanDuty[1] = val>>6;
			break;
		// Envelope
		case 0x17:
			chanVol[1] = val>>4;
			if (val & 0x8)
				chanEnvDir[1] = 1;
			else
				chanEnvDir[1] = -1;
			chanEnvSweep[1] = val&0x7;
			break;
		// Frequency (low)
		case 0x18:
			chanFreq[1] &= 0x700;
			chanFreq[1] |= val;
			break;
		// Frequency (high)
		case 0x19:
			chanFreq[1] &= 0xFF;
			chanFreq[1] |= (val&0x7)<<8;
			if (val & 0x80)
			{
				chanLenCounter[1] = (64-chanLen[1])*clockSpeed/256;
				chanOn[1] = 1;
				chanVol[1] = gameboy->ioRam[0x17]>>4;
				gameboy->setSoundChannel(CHAN_2);
			}
			if (val & 0x40)
				chanUseLen[1] = 1;
			else
				chanUseLen[1] = 0;
			break;
		// CHANNEL 3
		// On/Off
		case 0x1A:
            if ((val & 0x80) == 0)
            {
                chanOn[2] = 0;
                gameboy->clearSoundChannel(CHAN_4);
                gameboy->cyclesToExecute = -1; // Pi-Ka-Chu
                //buf.clear();
                //printf("chan3off\n");
            }
            else
            {
                //chanOn[2] = 1;
                //printf("chan3on?\n");
            }
			break;
		// Length
		case 0x1B:
			chanLen[2] = val;
			break;
		// Volume
		case 0x1C:
            if (chanVol[2] != ((val>>5)&3)) {
                chanVol[2] = (val>>5)&3;
                chanVol[2]--;
            }
            break;
		// Frequency (low)
		case 0x1D:
			chanFreq[2] &= 0xFF00;
			chanFreq[2] |= val;
			break;
		// Frequency (high)
		case 0x1E:
			chanFreq[2] &= 0xFF;
			chanFreq[2] |= (val&7)<<8;
			if ((val & 0x80) && (gameboy->ioRam[0x1A] & 0x80))
			{
				//buf.clear();
				chanOn[2] = 1;
				chanLenCounter[2] = (256-chanLen[2])*clockSpeed/256;
				gameboy->setSoundChannel(CHAN_3);
                gameboy->cyclesToExecute = -1; // Pi-Ka-Chu
			}
			if (val & 0x40)
			{
				chanUseLen[2] = 1;
				//printf("useLen\n");
			}
			else
			{
				chanUseLen[2] = 0;
			}
			break;
		// CHANNEL 4
		// Length
		case 0x20:
			chanLen[3] = val&0x1F;
			break;
		// Volume
		case 0x21:
			chanVol[3] = val>>4;
			if (val & 0x8)
				chanEnvDir[3] = 1;
			else
				chanEnvDir[3] = -1;
			chanEnvSweep[3] = val&0x7;
			break;
		// Frequency
		case 0x22:
			chanFreq[3] = val>>4;
			chan4FreqRatio = val&0x7;
			if (chan4FreqRatio == 0)
				chan4FreqRatio = 0.5;
			chan4Width = !!(val&0x8);
			break;
		// Start
		case 0x23:
			if (val&0x80)
			{
				chanLenCounter[3] = (64-chanLen[3])*clockSpeed/256;
				chanVol[3] = gameboy->ioRam[0x21]>>4;
				chanOn[3] = 1;
                lfsr = 0;
			}
			chanUseLen[3] = !!(val&0x40);
			break;
		case 0x24:
			//printf("Access volume\n");
			SO1Vol = val&0x7;
			SO2Vol = (val>>4)&0x7;
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
			break;
		case 0x26:
			if (!(val&0x80))
			{
				chanOn[0] = 0;
				chanOn[1] = 0;
				chanOn[2] = 0;
				chanOn[3] = 0;
				gameboy->clearSoundChannel(CHAN_1);
				gameboy->clearSoundChannel(CHAN_2);
				gameboy->clearSoundChannel(CHAN_3);
				gameboy->clearSoundChannel(CHAN_4);
			}
			break;
		default:
			break;
	}
}

// Global functions

void muteSND() {

}
void unmuteSND() {

}
void enableChannel(int i) {

}
void disableChannel(int i) {

}

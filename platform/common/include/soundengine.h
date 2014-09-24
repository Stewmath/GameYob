#ifdef DS
#include "common.h" // ARM7/ARM9 common stuff
#endif

#ifdef SDL
#include "Sync_Audio.h"
#include "Blip_Buffer.h"

#define BUFFERSIZE 2048
#define FREQUENCY 44100
#endif

class Gameboy;

#define CHAN_1 1
#define CHAN_2 2
#define CHAN_3 4
#define CHAN_4 8


class SoundEngine {
    public:
        SoundEngine(Gameboy* g);
        ~SoundEngine();
        void setGameboy(Gameboy* g);

        void init();
        void refresh();
        void mute();
        void unmute();

        void updateSound(int cycles)
#ifdef DS
            ITCM_CODE
#endif
            ;

        void setSoundEventCycles(int cycles); // Should be moved out of here
        void soundUpdateVBlank();
        void updateSoundSample();
        void handleSoundRegister(u8 ioReg, u8 val);

        int cyclesToSoundEvent;

    private:
        void synchronizeSound();
        void sendStartMessage(int i);
        void sendUpdateMessage(int i);
        void sendGlobalVolumeMessage();

        void refreshSoundPan(int i);
        void refreshSoundVolume(int i, bool send=false);
        void refreshSoundFreq(int i);
        void refreshSoundDuty(int i);
        void updateSoundSample(int byte);


#ifdef DS
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
        u8* sampleData;

        volatile SharedData* sharedPtr;
#endif
#ifdef SDL
        u16 lfsr;
        int noiseVal;
        double chan4FreqRatio;
        int chan4Width;
        int chan3WavPos;
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
        int chanFreqClocks[4];
        int chanOn[4];
        int chanPolarity[4];
        int chanVol[4];
        int chanEnvDir[4];
        int chanPolarityCounter[4];
        int chanEnvCounter[4];
        int chanEnvSweep[4];
        int chanToOut1[4];
        int chanToOut2[4];

        int SO1Vol;
        int SO2Vol;

        int bufferPosition;
        float updateBufferLimit;
        int updateBufferCount;
        int panic;
        int timePassed;

        Sync_Audio audio;
        Blip_Buffer buf;
        Blip_Synth<blip_low_quality,20> synth;
#endif

        bool muted;

        Gameboy* gameboy;
};

// Global functions
void muteSND();
void unmuteSND();
void enableChannel(int i);
void disableChannel(int i);

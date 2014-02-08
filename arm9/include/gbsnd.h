#ifdef DS
#include <nds.h>
#endif
#include "common.h"

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

        // Should be moved out of here
        void setSoundEventCycles(int cycles);
        void updateSound(int cycles) ITCM_CODE;

        void soundUpdateVBlank();
        void handleSoundRegister(u8 ioReg, u8 val);
        void updateSoundSample();

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

        bool muted;

        volatile SharedData* sharedPtr;

        Gameboy* gameboy;
};

// Global functions
void muteSND();
void unmuteSND();
void enableChannel(int i);
void disableChannel(int i);

#include "global.h"

#define FREQUENCY 44100
#define BUFFERSIZE 1024

extern float updateBufferLimit;
extern bool soundDisabled;
extern bool soundDisabledLid;

void initSND();
void enableChannel(int i);
void disableChannel(int i);
void updateSound(int cycles);
void handleSoundRegister(u8 ioReg, u8 val);
void updateSoundSample();
void handleSDLCallback(void* userdata, u8* buffer, int len);

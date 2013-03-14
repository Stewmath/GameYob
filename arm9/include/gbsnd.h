#include "global.h"

extern bool soundDisabled;

void initSND();
void refreshSND();
void enableChannel(int i);
void disableChannel(int i);
void updateSound(int cycles);
void handleSoundRegister(u8 ioReg, u8 val);
void updateSoundSample();
void handleSDLCallback(void* userdata, u8* buffer, int len);

#include "global.h"

extern bool soundDisabled;
extern bool hyperSound;

extern int cyclesToSoundEvent;

void initSND();
void refreshSND();
void enableChannel(int i);
void disableChannel(int i);
void updateSound(int cycles);
void soundUpdateVBlank(); // Sound is send to arm7 each vblank, if basicSound == true
void handleSoundRegister(u8 ioReg, u8 val);
void updateSoundSample();
void handleSDLCallback(void* userdata, u8* buffer, int len);

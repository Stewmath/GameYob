#include "global.h"

extern bool gbsMode;
extern u8 gbsHeader[0x70];

extern u8 gbsNumSongs;
extern u16 gbsLoadAddress;
extern u16 gbsInitAddress;
extern u16 gbsPlayAddress;

void readGBSHeader();
void initGBS();
void updateGBSInput();

/*---------------------------------------------------------------------------------

  This is file derived from:

	default ARM7 core

		Copyright (C) 2005 - 2010
		Michael Noland (joat)
		Jason Rogers (dovoto)
		Dave Murphy (WinterMute)

	This software is provided 'as-is', without any express or implied
	warranty.  In no event will the authors be held liable for any
	damages arising from the use of this software.

	Permission is granted to anyone to use this software for any
	purpose, including commercial applications, and to alter it and
	redistribute it freely, subject to the following restrictions:

	1.	The origin of this software must not be misrepresented; you
		must not claim that you wrote the original software. If you use
		this software in a product, an acknowledgment in the product
		documentation would be appreciated but is not required.

	2.	Altered source versions must be plainly marked as such, and
		must not be misrepresented as being the original software.

	3.	This notice may not be removed or altered from any source
		distribution.

---------------------------------------------------------------------------------*/
#include <nds.h>
#include <dswifi7.h>
#include "common.h"

void installGameboySoundFIFO();

void sdmmcMsgHandler(int bytes, void *user_data);
void sdmmcValueHandler(u32 value, void* user_data);
extern bool sleepIsEnabled;

volatile SharedData* sharedData;

void updateChannel(int c);

bool lidClosed = false;

void VblankHandler(void) {
    sharedData->frameFlip_DS = !sharedData->frameFlip_DS;
    sharedData->dsCycles = 0;

    if (sharedData->scalingOn) {
        // Copy from vram bank D to C
        dmaCopyWords(3, (u16*)0x06000000+24*256, (u16*)0x06020000, 256*144*2);
        sharedData->scaleTransferReady = false;
    }

scaling_end:
    Wifi_Update();
}

void VcountHandler() {
    inputGetAndSend();
}

volatile bool exitflag = false;

void powerButtonCB() {
    exitflag = true;
}

//---------------------------------------------------------------------------------
int main() {
    //---------------------------------------------------------------------------------
	// clear sound registers
	dmaFillWords(0, (void*)0x04000400, 0x100);

	REG_SOUNDCNT |= SOUND_ENABLE;
	writePowerManagement(PM_CONTROL_REG, ( readPowerManagement(PM_CONTROL_REG) & ~PM_SOUND_MUTE ) | PM_SOUND_AMP );
	powerOn(POWER_SOUND);

    readUserSettings();
    ledBlink(0);

    irqInit();
    // Start the RTC tracking IRQ
    //initClockIRQ(); //windwakr: Doesn't seem to work on 3DS.
    resyncClock();

    fifoInit();

    while (!fifoCheckValue32(FIFO_USER_03));
    sharedData = (SharedData*)(fifoGetValue32(FIFO_USER_03) | 0x02000000);

    SetYtrigger(80);

    installWifiFIFO();
    installSoundFIFO();

    installSystemFIFO();

    irqSet(IRQ_VCOUNT, VcountHandler);
    irqSet(IRQ_VBLANK, VblankHandler);

    irqEnable( IRQ_VBLANK | IRQ_VCOUNT | IRQ_NETWORK);

    setPowerButtonCB(powerButtonCB);   


    installGameboySoundFIFO();


    while (!exitflag) {
        if ( 0 == (REG_KEYINPUT & (KEY_SELECT | KEY_START | KEY_L | KEY_R))) {
            exitflag = true;
        }
        swiWaitForVBlank();
    }
    return 0;
}

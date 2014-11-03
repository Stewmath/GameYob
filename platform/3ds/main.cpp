#include <3ds.h>
#include <stdio.h>
#include <cwchar>
#include "gbgfx.h"
#include "soundengine.h"
#include "inputhelper.h"
#include "gameboy.h"
#include "console.h"
#include "gbs.h"
#include "romfile.h"
#include "menu.h"
#include "gbmanager.h"
#include "printconsole.h"

u32 srv_handle;

Result registerService(char* name) {
    u32* cmdBuf = getThreadCommandBuffer();

    cmdBuf[0] = 0x30100;
    strcpy((char*)&cmdBuf[1], name);
    cmdBuf[3] = strlen(name);
    cmdBuf[4] = 0;

    svcSendSyncRequest(srv_handle);

    return cmdBuf[1];
}

int main(int argc, char* argv[])
{
	srvInit();	
	aptInit();
	hidInit(NULL);
	gfxInit();
    fsInit();

    /*
    // Delay before aptSetupEventHandler
    for (int i=0; i<10; i++)
        system_waitForVBlank();

	aptSetupEventHandler();
    */

    consoleInitBottom();

    fs_init();

    mgr_init();

	initInput();
    setMenuDefaults();
    readConfigFile();

    printf("GameYob 3DS\n\n");

    printAndWait("Haxors\n");

    Handle irHandle;
    Result res = svcConnectToPort(&srv_handle, "srv:pm");

    printAndWait("srv:pm Result %.8x\n", res);

    res = registerService("ir:u");
    printAndWait("Register result: %.8x\n", res);

    res = srvGetServiceHandle(&irHandle, "ir:u");

    printAndWait("ir:u Result %.8x\n", res);

    mgr_selectRom();

    for (;;) {
        mgr_runFrame();
        mgr_updateVBlank();
    }

	return 0;
}

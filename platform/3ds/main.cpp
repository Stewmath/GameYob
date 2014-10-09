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

int main(int argc, char* argv[])
{
	srvInit();	
	aptInit();
	hidInit(NULL);
	gfxInit();
    fsInit();

    fs_init(); // nice naming scheme

    // Dirty hack to delay before aptSetupEventHandler
    for (int i=0; i<20; i++)
        drawScreen();

	aptSetupEventHandler();

    consoleInitBottom();

    mgr_init();

	initInput();
    setMenuDefaults();
    readConfigFile();

    printf("GameYob 3DS\n\n");

    mgr_selectRom();

    for (;;) {
        mgr_runFrame();
        mgr_updateVBlank();
    }

	return 0;
}

#include <SDL/SDL.h>
#include <stdio.h>
#include "global.h"
#include "gbgfx.h"
#include "soundengine.h"
#include "inputhelper.h"
#include "gameboy.h"
#include "console.h"
#include "gbs.h"
#include "romfile.h"
#include "menu.h"

//#ifndef WX_PRECOMP
//#include "wx-2.8/wx/wx.h"
//#endif

//int _main();

time_t rawTime;

extern int scale;

extern int quit;
SDL_Surface* screen;

Gameboy* gameboy;
RomFile* romFile = NULL;


int main(int argc, char* argv[]);
void updateVBlank();


int main(int argc, char* argv[])//(int argc, char* argv[])
{
	if (SDL_Init(SDL_INIT_EVERYTHING) == -1)
		return 1;

	SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 5 );
    SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 5 );
    SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 5 );
    SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 16 );
	SDL_GL_SetAttribute( SDL_GL_SWAP_CONTROL, 1 );
    SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
	SDL_ShowCursor(SDL_DISABLE);

	screen = SDL_SetVideoMode(160*scale, 144*scale, 32, SDL_SWSURFACE | SDL_OPENGL);// | SDL_FULLSCREEN);
	if (screen == NULL)
		return 1;
	

	SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 255, 255, 255));

	SDL_WM_SetCaption("GameYob", NULL);

    gameboy = new Gameboy();

	initInput();

    setMenuDefaults();
    readConfigFile();

    if (argc >= 2) {
        char* filename = argv[1];
        romFile = new RomFile(filename);
        gameboy->setRomFile(romFile);
    }
    else {
        printf("Give me a gameboy rom pls\n");
        return 1;
    }

    gameboy->init();

	for (;;)
	{
        if (!gameboy->isGameboyPaused())
            gameboy->runEmul();
        updateVBlank();
	}

	//fclose(logFile);
	SDL_Quit();
	return 0;
}

void updateVBlank() {
    readKeys();
    if (isMenuOn())
        updateMenu();
    else {
        gameboy->gameboyCheckInput();
        if (gbsMode)
            gbsCheckInput();
    }

    if (gbsMode) {
        drawScreen(); // Just because it syncs with vblank...
    }
    else {
        drawScreen();
    }
}

void selectRom() {

}

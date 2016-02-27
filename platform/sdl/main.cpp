#include <SDL/SDL.h>
#include <stdio.h>
#include "gbgfx.h"
#include "soundengine.h"
#include "inputhelper.h"
#include "gameboy.h"
#include "console.h"
#include "gbs.h"
#include "romfile.h"
#include "menu.h"
#include "gbmanager.h"

extern int scale;

SDL_Surface* screen;


int main(int argc, char* argv[])
{
	if (SDL_Init(SDL_INIT_EVERYTHING) == -1)
		return 1;

	SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 5 );
    SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 5 );
    SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 5 );
    SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 16 );
	SDL_GL_SetAttribute( SDL_GL_SWAP_CONTROL, 0 );
    SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 0 );
	SDL_ShowCursor(SDL_DISABLE);

	screen = SDL_SetVideoMode(160*scale, 144*scale, 32, SDL_SWSURFACE);// | SDL_FULLSCREEN);
	if (screen == NULL)
		return 2;
	

	SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 255, 255, 255));

	SDL_WM_SetCaption("GameYob", NULL);

    mgr_init();

	initInput();
    setMenuDefaults();
    readConfigFile();
    initGFX();

    if (argc >= 2) {
        char* filename = argv[1];
        mgr_loadRom(filename);
    }
    else {
        printf("Give me a gameboy rom pls\n");
        return 3;
    }

    for (;;) {
        mgr_runFrame();
        mgr_updateVBlank();
    }

	return 0;
}

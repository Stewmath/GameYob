#include <SDL/SDL.h>
#include <stdio.h>
#include "global.h"
#include "GBGFX.h"
#include "GBSND.h"
#include "GBCPU.h"
#include "InputHelper.h"
#include "Gameboy.h"
#include "Timer.h"
#include "MMU.h"

//#ifndef WX_PRECOMP
//#include "wx-2.8/wx/wx.h"
//#endif

//int _main();

extern int quit;
SDL_Surface* screen;

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

	SDL_WM_SetCaption("Gameboy Emulator", NULL);

	initInput();
	loadProgram(argv[1]);

	FILE* file;
	file = fopen("gbc_bios.bin", "rb");
	//file = fopen("DMG_ROM.bin", "rb");
	fread(bios, 1, 0x900, file);
	Timer* fpsTimer = (Timer*)malloc(sizeof(Timer));

	loadSave();
    initMMU();
	initCPU();
	initLCD();
	initGFX();
	initSND();
	startTimer(fpsTimer);
	for (;;)
	{
		runEmul();
		//printf("%d\n", val);
		if (handleEvents() == 1 || quit)		// Input mostly
		{
			break;
		}
		if (getTimerTicks(fpsTimer) >= 1000)
		{
			//fprintf(stdout, "FPS: %d\n", fps);
			fps = 0;
			startTimer(fpsTimer);
		}
	}

	//fclose(logFile);
	SDL_Quit();
	return 0;
}

void printLog(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
}

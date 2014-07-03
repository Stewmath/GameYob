#include "SDL/SDL.h"
#include "InputHelper.h"
#include "MMU.h"
#include "Gameboy.h"
#include "GBGFX.h"
#include "Debugger.h"

extern int drawVram;

SDL_Event event;
SDL_Joystick* joystick;
char filename[100];

FILE* logFile;

int keysPressed[10];

void initInput()
{
	int i;
	for (i=0; i<8; i++)
		keysPressed[i] = 0;
	SDL_JoystickEventState(SDL_ENABLE);
	joystick = SDL_JoystickOpen(0);
#ifdef LOG
	logFile = fopen("LOG", "w");
#endif
}

int loadProgram(char* _filename)
{
	//fatInitDefault();

	FILE* file;
	file = fopen(_filename, "rb");
	if (file == NULL)
	{
		printf("Error opening %s.\n", _filename);
		return 1;
	}

	// First calculate the size
	fseek(file, 0, SEEK_END);
	numRomBanks = ftell(file)/0x4000;
	//printf("\n%d\n", numRomBanks);

	// Allocate memory for the rom && load rom into memory
	rewind(file);
	int i;
	for (i=0; i<numRomBanks; i++)
	{
		rom[i] = (u8*)malloc(0x4000*sizeof(u8));
		fread(rom[i], 1, 0x4000, file);
	}
	
	fclose(file);

	strcpy(filename, _filename);
	return 0;
}
void loadRomBank() {
}

int loadSave()
{
	// Get the game's external memory size and allocate the memory
	numRamBanks = readMemory(0x149);
	switch(numRamBanks)
	{
		// Case 0 should actually have 0 banks.
		// This just makes errors easier to handle.
		case 0:
		case 1:
		case 2:
			numRamBanks = 1;
			break;
		case 3:
			numRamBanks = 4;
			break;
		case 4:
			numRamBanks = 16;
			break;
	}

	externRam = (u8**)malloc(numRamBanks*sizeof(u8*));
	int i;
	for (i=0; i<numRamBanks; i++)
	{
		externRam[i] = (u8*)malloc(0x2000*sizeof(u8));
	}

	// Now load the data.
	FILE* file;
	char savename[100];
	strcpy(savename, filename);
    *(strrchr(savename, '.')) = 0;
	strcat(savename, ".sav");
	file = fopen(savename, "r");
	if (file != NULL)
	{
		for (i=0; i<numRamBanks; i++)
			fread(externRam[i], 1, 0x2000, file);
		if (MBC == 3)
		{
			latchClock();
			fread(&gbClock.clockSeconds, 1, sizeof(int)*5+sizeof(time_t), file);
		}
		fclose(file);
	}
	else
	{
		fprintf(stderr, "Couldn't open file \"%s\".\n", savename);
		return 1;
	}
	return 0;
}

int saveGame()
{
	FILE* file;
	char savename[103];
	strcpy(savename, filename);
    *(strrchr(savename, '.')) = 0;
	strcat(savename, ".sav");
	file = fopen(savename, "w");
	if (file != NULL)
	{
		int i;
		for (i=0; i<numRamBanks; i++)
			fwrite(externRam[i], 1, 0x2000, file);
		if (MBC == 3)
		{
			latchClock();
			fwrite(&gbClock.clockSeconds, 1, sizeof(int)*5+sizeof(time_t), file);
		}
		fclose(file);
	}
	else
	{
		fprintf(stderr, "Error saving to file.\n");
		return 1;
	}
	return 0;
}

int posy=0;
void readKeys() {
}
int handleEvents()
{
	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
			case SDL_QUIT:
				saveGame();
#ifdef LOG
				fclose(logFile);
#endif
                exit(0);
				break;
			case SDL_KEYDOWN:
				switch (event.key.keysym.sym)
				{
					case SDLK_s:
						saveGame();
						break;
					case SDLK_UP:
						buttonsPressed &= (0xFF ^ UP);
						requestInterrupt(JOYPAD);
						break;
					case SDLK_DOWN:
						buttonsPressed &= (0xFF ^ DOWN);
						requestInterrupt(JOYPAD);
						break;
					case SDLK_LEFT:
						buttonsPressed &= (0xFF ^ LEFT);
						requestInterrupt(JOYPAD);
						break;
					case SDLK_RIGHT:
						buttonsPressed &= (0xFF ^ RIGHT);
						requestInterrupt(JOYPAD);
						break;
					case SDLK_SEMICOLON:
						buttonsPressed &= (0xFF ^ BUTTONA);
						requestInterrupt(JOYPAD);
						break;
					case SDLK_q:
						buttonsPressed &= (0xFF ^ BUTTONB);
						requestInterrupt(JOYPAD);
						break;
					case SDLK_RETURN:
						buttonsPressed &= (0xFF ^ START);
						requestInterrupt(JOYPAD);
						break;
					case SDLK_RSHIFT:
						buttonsPressed &= (0xFF ^ SELECT);
						requestInterrupt(JOYPAD);
						break;
					case SDLK_ESCAPE:
						event.type = SDL_QUIT;
						SDL_PushEvent(&event);
						break;
					case SDLK_k:
						drawVram++;
						if (drawVram > 2)
							drawVram = 0;
						break;
					case SDLK_d:
						debugMode = 1;
						break;
					default:
						break;
				}
				break;
			case SDL_KEYUP:
				switch (event.key.keysym.sym)
				{
					case SDLK_UP:
						buttonsPressed |= UP;
						break;
					case SDLK_DOWN:
						buttonsPressed |= DOWN;
						break;
					case SDLK_LEFT:
						buttonsPressed |= LEFT;
						break;
					case SDLK_RIGHT:
						buttonsPressed |= RIGHT;
						break;
					case SDLK_SEMICOLON:
						buttonsPressed |= BUTTONA;
						break;
					case SDLK_q:
						buttonsPressed |= BUTTONB;
						break;
					case SDLK_RETURN:
						buttonsPressed |= START;
						break;
					case SDLK_RSHIFT:
						buttonsPressed |= SELECT;
						break;
					default:
						break;
				}
				break;
			case SDL_JOYBUTTONDOWN:
				switch (event.jbutton.button+1)
				{
					case 2:
						buttonsPressed &= (0xFF ^ BUTTONA);
						requestInterrupt(JOYPAD);
						break;
					case 1:
						buttonsPressed &= (0xFF ^ BUTTONB);
						requestInterrupt(JOYPAD);
						break;
					case 10:
						buttonsPressed &= (0xFF ^ START);
						requestInterrupt(JOYPAD);
						break;
					case 9:
						buttonsPressed &= (0xFF ^ SELECT);
						requestInterrupt(JOYPAD);
						break;
					case 6:
						enableTurbo();
					default:
						break;
				}
				break;
			case SDL_JOYBUTTONUP:
				switch(event.jbutton.button+1)
				{
					case 2:
						buttonsPressed |= BUTTONA;
						break;
					case 1:
						buttonsPressed |= BUTTONB;
						break;
					case 10:
						buttonsPressed |= START;
						break;
					case 9:
						buttonsPressed |= SELECT;
						break;
					case 6:
						disableTurbo();
						break;

				}
				break;
			case SDL_JOYHATMOTION:
				if (event.jhat.value & SDL_HAT_UP)
				{
					buttonsPressed &= (0xFF ^ UP);
					requestInterrupt(JOYPAD);
				}
				else
					buttonsPressed |= UP;
				if (event.jhat.value & SDL_HAT_DOWN)
				{
					buttonsPressed &= (0xFF ^ DOWN);
					requestInterrupt(JOYPAD);
				}
				else
					buttonsPressed |= DOWN;
				if (event.jhat.value & SDL_HAT_RIGHT)
				{
					buttonsPressed &= (0xFF ^ RIGHT);
					requestInterrupt(JOYPAD);
				}
				else
					buttonsPressed |= RIGHT;
				if (event.jhat.value & SDL_HAT_LEFT)
				{
					buttonsPressed &= (0xFF ^ LEFT);
					requestInterrupt(JOYPAD);
				}
				else
					buttonsPressed |= LEFT;
				break;
		}
	}
	return 0;
}

void enableTurbo()
{
	turbo = 1;
//	SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, 0);
}

void disableTurbo()
{
	turbo = 0;
//	SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, 1);
}

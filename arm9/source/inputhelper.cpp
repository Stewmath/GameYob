#include <nds.h>
#include <fat.h>

#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>

#include "inputhelper.h"
#include "mmu.h"
#include "gameboy.h"
#include "main.h"
#include "console.h"

FILE* romFile=NULL;
char filename[100];
char savename[100];
char romTitle[20];
int keysPressed=0;
int lastKeysPressed=0;
int keysForceReleased=0;
int repeatStartTimer=0;
int repeatTimer=0;

bool advanceFrame;

u8 romBankSlots[0x80][0x4000];
int bankSlotIDs[MAX_ROM_BANKS];
std::vector<int> lastBanksUsed;

void initInput()
{
	fatInitDefault();
    advanceFrame=false;
}

// File chooser variables
const int filesPerPage = 23;
const int MAX_FILENAME_LEN = 100;
int numFiles;
int scrollY=0;
int fileSelection=0;

void updateScrollDown() {
    if (fileSelection >= numFiles)
        fileSelection = numFiles-1;
    if (fileSelection == numFiles-1 && numFiles > filesPerPage)
        scrollY = fileSelection-filesPerPage+1;
    else if (fileSelection-scrollY >= filesPerPage-1)
        scrollY = fileSelection-filesPerPage+2;
}
void updateScrollUp() {
    if (fileSelection < 0)
        fileSelection = 0;
    if (fileSelection == 0)
        scrollY = 0;
    else if (fileSelection == scrollY)
        scrollY--;
    else if (fileSelection < scrollY)
        scrollY = fileSelection-1;

}
char* startFileChooser() {
    char* retval;
    char cwd[256];
    getcwd(cwd, 256);
    DIR* dp = opendir(cwd);
    struct dirent *entry;
    if (dp == NULL) {
        printf("Error opening directory.\n");
        return 0;
    }
    while (true) {
        numFiles=0;
        std::vector<char*> filenames;
        std::vector<bool> directory;
        while ((entry = readdir(dp)) != NULL) {
            char* ext = strrchr(entry->d_name, '.')+1;
            if (entry->d_type & DT_DIR || strcmp(ext, "cgb") == 0 || strcmp(ext, "gbc") == 0 || strcmp(ext, "gb") == 0 || strcmp(ext, "sgb") == 0) {
                if (!(strcmp(".", entry->d_name) == 0)) {
                    directory.push_back(entry->d_type & DT_DIR);
                    char *name = (char*)malloc(sizeof(char)*256);
                    strcpy(name, entry->d_name);
                    filenames.push_back(name);
                    numFiles++;
                }
            }
        }
        // start sorting
        for (int i=1; i<numFiles; i++) {
            int j;
            for (j=0; j<i; j++) {
                if (strcmp(filenames[i], filenames[j]) <= 0) {
                    break;
                }
            }
            char name[MAX_FILENAME_LEN];
            strcpy(name, filenames[i]);
            bool dir = directory[i];
            for (int k=i; k>j; k--) {
                strcpy(filenames[k], filenames[k-1]);
                directory[k] = directory[k-1];
            }
            strcpy(filenames[j], name);
            directory[j] = dir;
        }
        // sorting done

        scrollY=0;
        updateScrollDown();
        bool readDirectory = false;
        while (!readDirectory) {
            consoleClear();
            for (int i=scrollY; i<scrollY+filesPerPage && i<numFiles; i++) {
                if (i == fileSelection)
                    printf("* ");
                else if (i == scrollY && i != 0)
                    printf("^ ");
                else if (i == scrollY+filesPerPage-1 && scrollY+filesPerPage-1 != numFiles-1)
                    printf("v ");
                else
                    printf("  ");
                char outBuffer[32];

                int maxLen = 29;
                if (directory[i])
                    maxLen--;
                strncpy(outBuffer, filenames[i], maxLen);
                outBuffer[maxLen] = '\0';
                if (directory[i])
                    printf("%s/\n", outBuffer);
                else
                    printf("%s\n", outBuffer);
            }

            while (true) {
                swiWaitForVBlank();
                readKeys();
                if (keyJustPressed(KEY_A)) {
                    if (directory[fileSelection]) {
                        closedir(dp);
                        dp = opendir(filenames[fileSelection]);
                        chdir(filenames[fileSelection]);
                        readDirectory = true;
                        break;
                    }
                    else {
                        retval = filenames[fileSelection];
                        goto end;
                    }
                }
                else if (keyJustPressed(KEY_B)) {
                    if (numFiles >= 1 && strcmp(filenames[0], "..") == 0) {
                        closedir(dp);
                        dp = opendir("..");
                        chdir("..");
                        readDirectory = true;
                        break;
                    }
                }
                else if (keyPressedAutoRepeat(KEY_UP)) {
                    if (fileSelection > 0) {
                        fileSelection--;
                        updateScrollUp();
                        break;
                    }
                }
                else if (keyPressedAutoRepeat(KEY_DOWN)) {
                    if (fileSelection < numFiles-1) {
                        fileSelection++;
                        updateScrollDown();
                        break;
                    }
                }
                else if (keyJustPressed(KEY_RIGHT)) {
                    fileSelection += filesPerPage/2;
                    updateScrollDown();
                    break;
                }
                else if (keyJustPressed(KEY_LEFT)) {
                    fileSelection -= filesPerPage/2;
                    updateScrollUp();
                    break;
                }
            }
        }
        // free memory used for filenames
        for (int i=0; i<numFiles; i++) {
            free(filenames[i]);
        }
        fileSelection = 0;
    }
end:
    closedir(dp);
    consoleClear();
    return retval;
}

int loadProgram(char* f)
{
    if (romFile != NULL)
        fclose(romFile);
	strcpy(filename, f);

	romFile = fopen(filename, "rb");
	if (romFile == NULL)
	{
		printLog("Error opening %s.\n", filename);
		return 1;
	}

	// First calculate the size
	fseek(romFile, 0, SEEK_END);
	numRomBanks = ftell(romFile)/0x4000;

	rewind(romFile);

    for (int i=0; i<numRomBanks; i++) {
        bankSlotIDs[i] = -1;
    }

    int banksToLoad = numRomBanks;
    if (numRomBanks > 0x80)
        banksToLoad = 0x80;

    lastBanksUsed = std::vector<int>();
	int i;
	for (i=0; i<banksToLoad; i++)
	{
		rom[i] = romBankSlots[i];
        bankSlotIDs[i] = i;
		fread(rom[i], 1, 0x4000, romFile);
        if (i != 0)
            lastBanksUsed.push_back(i);
	}
	
	strcpy(savename, filename);
    *(strrchr(savename, '.')) = '\0';
	strcat(savename, ".sav");

    for (int i=0; i<0xe; i++) {
        romTitle[i] = (char)rom[0][i+0x134];
    }
    romTitle[0xe] = '\0';

    loadSave();
	return 0;
}

void loadRomBank() {
    if (numRomBanks <= 0x80 || bankSlotIDs[currentRomBank] != -1)
        return;
    int bankToUnload = lastBanksUsed.back();
    lastBanksUsed.pop_back();
    int slot = bankSlotIDs[bankToUnload];
    bankSlotIDs[bankToUnload] = -1;
    bankSlotIDs[currentRomBank] = slot;
    rom[currentRomBank] = romBankSlots[slot];

    fseek(romFile, 0x4000*currentRomBank, SEEK_SET);
    fread(rom[currentRomBank], 1, 0x4000, romFile);

    lastBanksUsed.insert(lastBanksUsed.begin(), currentRomBank);
}

int loadSave()
{
    // unload previous save
    if (externRam != NULL) {
        for (int i=0; i<numRamBanks; i++) {
            free(externRam[i]);
        }
        free(externRam);
    }
    externRam = NULL;

	// Get the game's external memory size and allocate the memory
	numRamBanks = readMemory(0x149);
	switch(numRamBanks)
	{
		case 0:
            numRamBanks = 0;
            break;
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
		default:
			printLog("Invalid RAM bank number: %d", numRamBanks);
			numRamBanks = 1;
			break;
	}

    if (numRamBanks == 0)
        return 0;

	externRam = (u8**)malloc(numRamBanks*sizeof(u8*));
	int i;
	for (i=0; i<numRamBanks; i++)
	{
		externRam[i] = (u8*)malloc(0x2000*sizeof(u8));
	}

	// Now load the data.
	FILE* file;
	file = fopen(savename, "r");
	if (file != NULL)
	{
		for (i=0; i<numRamBanks; i++)
			fread(externRam[i], 1, 0x2000, file);
        u8 mapper = readMemory(0x147);
        if (mapper == 0x10 || mapper == 0x12 || mapper == 0x13)
		{
			fread(&gbClock.clockSeconds, 1, sizeof(int)*10+sizeof(time_t), file);
		}
		fclose(file);
	}
	else
	{
		printLog("Couldn't open file \"%s\".\n", savename);
		return 1;
	}
	return 0;
}

int saveGame()
{
    if (numRamBanks == 0)
        return 0;
	FILE* file;
	file = fopen(savename, "w");
	if (file != NULL)
	{
		int i;
		for (i=0; i<numRamBanks; i++)
			fwrite(externRam[i], 1, 0x2000, file);
		if (MBC == 3)
		{
			fwrite(&gbClock.clockSeconds, 1, sizeof(int)*10+sizeof(time_t), file);
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

bool keyPressed(int key) {
    return keysPressed&key;
}
bool keyPressedAutoRepeat(int key) {
    if (keyJustPressed(key)) {
        repeatStartTimer = 14;
        return true;
    }
    if (keyPressed(key) && repeatStartTimer == 0 && repeatTimer == 0) {
        repeatTimer = 2;
        return true;
    }
    return false;
}
bool keyJustPressed(int key) {
    return ((keysPressed^lastKeysPressed)&keysPressed) & key;
}

void readKeys() {
	scanKeys();

    lastKeysPressed = keysPressed;
    keysPressed = keysHeld();
    for (int i=0; i<16; i++) {
        if (keysForceReleased & (1<<i)) {
            if (!(keysPressed & (1<<i)))
                keysForceReleased &= ~(1<<i);
        }
    }
    keysPressed &= ~keysForceReleased;
    if (repeatStartTimer > 0)
        repeatStartTimer--;
    if (repeatTimer > 0)
        repeatTimer--;
}

void forceReleaseKey(int key) {
    keysForceReleased |= key;
    keysPressed &= ~key;
}

char* getRomTitle() {
    return romTitle;
}

int handleEvents()
{
    int keys = keysPressed;
	if (keys & KEY_UP)
	{
		buttonsPressed &= (0xFF ^ UP);
		requestInterrupt(JOYPAD);
	}
	else
	{
		buttonsPressed |= UP;
	}
	if (keys & KEY_DOWN)
	{
		buttonsPressed &= (0xFF ^ DOWN);
		requestInterrupt(JOYPAD);
	}
	else
	{
		buttonsPressed |= DOWN;
	}
	if (keys & KEY_LEFT)
	{
		buttonsPressed &= (0xFF ^ LEFT);
		requestInterrupt(JOYPAD);
	}
	else
	{
		buttonsPressed |= LEFT;
	}
	if (keys & KEY_RIGHT)
	{
		buttonsPressed &= (0xFF ^ RIGHT);
		requestInterrupt(JOYPAD);
	}
	else
	{
		buttonsPressed |= RIGHT;
	}
	if (keys & KEY_A)
	{
		buttonsPressed &= (0xFF ^ BUTTONA);
		requestInterrupt(JOYPAD);
	}
	else
	{
		buttonsPressed |= BUTTONA;
	}
	if (keys & KEY_B)
	{
		buttonsPressed &= (0xFF ^ BUTTONB);
		requestInterrupt(JOYPAD);
	}
	else
	{
		buttonsPressed |= BUTTONB;
	}
	if (keys & KEY_START)
	{
		buttonsPressed &= (0xFF ^ START);
		requestInterrupt(JOYPAD);
	}
	else
	{
		buttonsPressed |= START;
	}
	if (keys & KEY_SELECT)
	{
		buttonsPressed &= (0xFF ^ SELECT);
		requestInterrupt(JOYPAD);
	}
	else
	{
		buttonsPressed |= SELECT;
	}

	if (keyJustPressed(KEY_X))
		saveGame();
    if (advanceFrame || keyJustPressed(KEY_R)) {
        advanceFrame = 0;
        return displayConsole();
    }

	return 0;
}

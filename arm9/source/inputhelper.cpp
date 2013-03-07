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

int GB_KEY_A, GB_KEY_B;

bool advanceFrame;

u8 romBankSlots[MAX_LOADED_ROM_BANKS][0x4000];
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

int nameSortFunction(char*& a, char*& b)
{
	// ".." sorts before everything except itself.
	bool aIsParent = strcmp(a, "..") == 0;
	bool bIsParent = strcmp(b, "..") == 0;

	if (aIsParent && bIsParent)
		return 0;
	else if (aIsParent) // Sorts before
		return -1;
	else if (bIsParent) // Sorts after
		return 1;
	else
		return strcmp(a, b);
}

/*
 * Determines whether a portion of a vector is sorted.
 * Input assertions: 'from' and 'to' are valid indices into data. 'to' can be
 *   the maximum value for the type 'unsigned int'.
 * Input: 'data', data vector, possibly sorted.
 *        'sortFunction', function determining the sort order of two elements.
 *        'from', index of the first element in the range to test.
 *        'to', index of the last element in the range to test.
 * Output: true if, for any valid index 'i' such as from <= i < to,
 *   data[i] < data[i + 1].
 *   true if the range is one or no elements, or if from > to.
 *   false otherwise.
 */
template <class Data> bool isSorted(std::vector<Data>& data, int (*sortFunction) (Data&, Data&), const unsigned int from, const unsigned int to)
{
	if (from >= to)
		return true;

	Data* prev = &data[from];
	for (int i = from + 1; i < to; i++)
	{
		if ((*sortFunction)(*prev, data[i]) > 0)
			return false;
		prev = &data[i];
	}
	if ((*sortFunction)(*prev, data[to]) > 0)
		return false;

	return true;
}

/*
 * Chooses a pivot for Quicksort. Uses the median-of-three search algorithm
 * first proposed by Robert Sedgewick.
 * Input assertions: 'from' and 'to' are valid indices into data. 'to' can be
 *   the maximum value for the type 'unsigned int'.
 * Input: 'data', data vector.
 *        'sortFunction', function determining the sort order of two elements.
 *        'from', index of the first element in the range to be sorted.
 *        'to', index of the last element in the range to be sorted.
 * Output: a valid index into data, between 'from' and 'to' inclusive.
 */
template <class Data> unsigned int choosePivot(std::vector<Data>& data, int (*sortFunction) (Data&, Data&), const unsigned int from, const unsigned int to)
{
	// The highest of the two extremities is calculated first.
	unsigned int highest = ((*sortFunction)(data[from], data[to]) > 0)
		? from
		: to;
	// Then the lowest of that highest extremity and the middle
	// becomes the pivot.
	return ((*sortFunction)(data[from + (to - from) / 2], data[highest]) < 0)
		? (from + (to - from) / 2)
		: highest;
}

/*
 * Partition function for Quicksort. Moves elements such that those that are
 * less than the pivot end up before it in the data vector.
 * Input assertions: 'from', 'to' and 'pivotIndex' are valid indices into data.
 *   'to' can be the maximum value for the type 'unsigned int'.
 * Input: 'data', data vector.
 *        'metadata', data describing the values in 'data'.
 *        'sortFunction', function determining the sort order of two elements.
 *        'from', index of the first element in the range to sort.
 *        'to', index of the last element in the range to sort.
 *        'pivotIndex', index of the value chosen as the pivot.
 * Output: the index of the value chosen as the pivot after it has been moved
 *   after all the values that are less than it.
 */
template <class Data, class Metadata> unsigned int partition(std::vector<Data>& data, std::vector<Metadata>& metadata, int (*sortFunction) (Data&, Data&), const unsigned int from, const unsigned int to, const unsigned int pivotIndex)
{
	Data pivotValue = data[pivotIndex];
	data[pivotIndex] = data[to];
	data[to] = pivotValue;
	{
		const Metadata tM = metadata[pivotIndex];
		metadata[pivotIndex] = metadata[to];
		metadata[to] = tM;
	}

	unsigned int storeIndex = from;
	for (unsigned int i = from; i < to; i++)
	{
		if ((*sortFunction)(data[i], pivotValue) < 0)
		{
			const Data tD = data[storeIndex];
			data[storeIndex] = data[i];
			data[i] = tD;
			const Metadata tM = metadata[storeIndex];
			metadata[storeIndex] = metadata[i];
			metadata[i] = tM;
			++storeIndex;
		}
	}

	{
		const Data tD = data[to];
		data[to] = data[storeIndex];
		data[storeIndex] = tD;
		const Metadata tM = metadata[to];
		metadata[to] = metadata[storeIndex];
		metadata[storeIndex] = tM;
	}
	return storeIndex;
}

/*
 * Sorts an array while keeping metadata in sync.
 * This sort is unstable and its average performance is
 *   O(data.size() * log2(data.size()).
 * Input assertions: for any valid index 'i' in data, index 'i' is valid in
 *   metadata. 'from' and 'to' are valid indices into data. 'to' can be
 *   the maximum value for the type 'unsigned int'.
 * Invariant: index 'i' in metadata describes index 'i' in data.
 * Input: 'data', data to sort.
 *        'metadata', data describing the values in 'data'.
 *        'sortFunction', function determining the sort order of two elements.
 *        'from', index of the first element in the range to sort.
 *        'to', index of the last element in the range to sort.
 */
template <class Data, class Metadata> void quickSort(std::vector<Data>& data, std::vector<Metadata>& metadata, int (*sortFunction) (Data&, Data&), const unsigned int from, const unsigned int to)
{
	if (isSorted(data, sortFunction, from, to))
		return;

	unsigned int pivotIndex = choosePivot(data, sortFunction, from, to);
	unsigned int newPivotIndex = partition(data, metadata, sortFunction, from, to, pivotIndex);
	if (newPivotIndex > 0)
		quickSort(data, metadata, sortFunction, from, newPivotIndex - 1);
	if (newPivotIndex < to)
		quickSort(data, metadata, sortFunction, newPivotIndex + 1, to);
}

/*
 * Prompts the user for a file to load.
 * Returns a pointer to a newly-allocated string. The caller is responsible
 * for free()ing it.
 */
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
            if (entry->d_type & DT_DIR || strcmpi(ext, "cgb") == 0 || strcmpi(ext, "gbc") == 0 || strcmpi(ext, "gb") == 0 || strcmpi(ext, "sgb") == 0) {
                if (!(strcmp(".", entry->d_name) == 0)) {
                    directory.push_back(entry->d_type & DT_DIR);
                    char *name = (char*)malloc(sizeof(char)*(strlen(entry->d_name)+1));
                    strcpy(name, entry->d_name);
                    filenames.push_back(name);
                    numFiles++;
                }
            }
        }

	quickSort(filenames, directory, nameSortFunction, 0, numFiles - 1);

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
                        // Copy the result to a new allocation, as the
                        // filename would become unavailable when freed.
                        retval = (char*) malloc(sizeof(char*) * (strlen(filenames[fileSelection]) + 1));
                        strcpy(retval, filenames[fileSelection]);
                        // free memory used for filenames in this dir
                        for (int i=0; i<numFiles; i++) {
                            free(filenames[i]);
                        }
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
                else if (keyPressedAutoRepeat(KEY_RIGHT)) {
                    fileSelection += filesPerPage/2;
                    updateScrollDown();
                    break;
                }
                else if (keyPressedAutoRepeat(KEY_LEFT)) {
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
    if (numRomBanks > MAX_LOADED_ROM_BANKS)
        banksToLoad = MAX_LOADED_ROM_BANKS;

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

    // Little hack to preserve "quickread" from gbcpu.cpp.
    if (biosEnabled) {
        for (int i=0x100; i<0x150; i++)
            bios[i] = rom[0][i];
    }
	return 0;
}

void loadRomBank() {
    if (numRomBanks <= MAX_LOADED_ROM_BANKS || bankSlotIDs[currentRomBank] != -1)
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
			printLog("Invalid RAM bank number: %d\nDefaulting to 4 banks\n", numRamBanks);
			numRamBanks = 4;
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
	if (keys & GB_KEY_A)
	{
		buttonsPressed &= (0xFF ^ BUTTONA);
		requestInterrupt(JOYPAD);
	}
	else
	{
		buttonsPressed |= BUTTONA;
	}
	if (keys & GB_KEY_B)
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

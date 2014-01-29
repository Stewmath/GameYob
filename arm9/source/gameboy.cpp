#include <stdlib.h>
#include <stdio.h>
#include "gbprinter.h"
#include "gameboy.h"
#include "common.h"
#include "gbcpu.h"
#include "gbgfx.h"
#include "gbsnd.h"
#include "mmu.h"
#include "timer.h"
#include "main.h"
#include "inputhelper.h"
#include "nifi.h"
#include "console.h"
#include "cheats.h"
#include "sgb.h"
#include "gbs.h"


time_t rawTime;
time_t lastRawTime;
const int maxWaitCycles=1000000;


bool fpsOutput=true;
bool timeOutput=true;
bool fastForwardMode = false; // controlled by the toggle hotkey
bool fastForwardKey = false;  // only while its hotkey is pressed

int cyclesSinceVblank=0;
bool probingForBorder=false;

Gameboy::Gameboy() {
    fpsOutput=true;
    timeOutput=true;

    fastForwardMode = false;
    fastForwardKey = false;

    cyclesSinceVblank=0;
    probingForBorder=false;

    // private
    resettingGameboy = false;
    wroteToSramThisFrame = false;
    framesSinceAutosaveStarted=0;

    externRam = NULL;
    saveModified = false;
    numSaveWrites = 0;
    autosaveStarted = false;

}

void Gameboy::setEventCycles(int cycles) {
    if (cycles < cyclesToEvent) {
        cyclesToEvent = cycles;
        /*
        if (cyclesToEvent <= 0) {
           cyclesToEvent = 1;
        }
        */
    }
}

void Gameboy::gameboyCheckInput() {
    static int autoFireCounterA=0, autoFireCounterB=0;

    buttonsPressed = 0xff;

    if (probingForBorder)
        return;

    if (keyPressed(mapGbKey(KEY_GB_UP))) {
        buttonsPressed &= (0xFF ^ UP);
        if (!(ioRam[0x00] & 0x10))
            requestInterrupt(JOYPAD);
    }
    if (keyPressed(mapGbKey(KEY_GB_DOWN))) {
        buttonsPressed &= (0xFF ^ DOWN);
        if (!(ioRam[0x00] & 0x10))
            requestInterrupt(JOYPAD);
    }
    if (keyPressed(mapGbKey(KEY_GB_LEFT))) {
        buttonsPressed &= (0xFF ^ LEFT);
        if (!(ioRam[0x00] & 0x10))
            requestInterrupt(JOYPAD);
    }
    if (keyPressed(mapGbKey(KEY_GB_RIGHT))) {
        buttonsPressed &= (0xFF ^ RIGHT);
        if (!(ioRam[0x00] & 0x10))
            requestInterrupt(JOYPAD);
    }
    if (keyPressed(mapGbKey(KEY_GB_A))) {
        buttonsPressed &= (0xFF ^ BUTTONA);
        if (!(ioRam[0x00] & 0x20))
            requestInterrupt(JOYPAD);
    }
    if (keyPressed(mapGbKey(KEY_GB_B))) {
        buttonsPressed &= (0xFF ^ BUTTONB);
        if (!(ioRam[0x00] & 0x20))
            requestInterrupt(JOYPAD);
    }
    if (keyPressed(mapGbKey(KEY_GB_START))) {
        buttonsPressed &= (0xFF ^ START);
        if (!(ioRam[0x00] & 0x20))
            requestInterrupt(JOYPAD);
    }
    if (keyPressed(mapGbKey(KEY_GB_SELECT))) {
        buttonsPressed &= (0xFF ^ SELECT);
        if (!(ioRam[0x00] & 0x20))
            requestInterrupt(JOYPAD);
    }

    if (keyPressed(mapGbKey(KEY_GB_AUTO_A))) {
        if (autoFireCounterA <= 0) {
            buttonsPressed &= (0xFF ^ BUTTONA);
            if (!(ioRam[0x00] & 0x20))
                requestInterrupt(JOYPAD);
            autoFireCounterA = 2;
        }
        autoFireCounterA--;
    }
    if (keyPressed(mapGbKey(KEY_GB_AUTO_B))) {
        if (autoFireCounterB <= 0) {
            buttonsPressed &= (0xFF ^ BUTTONB);
            if (!(ioRam[0x00] & 0x20))
                requestInterrupt(JOYPAD);
            autoFireCounterB = 2;
        }
        autoFireCounterB--;
    }

#ifdef SPEEDHAX
    refreshP1();
#endif

    if (keyJustPressed(mapGbKey(KEY_SAVE))) {
        if (!autoSavingEnabled) {
            saveGame();
        }
    }

    fastForwardKey = keyPressed(mapGbKey(KEY_FAST_FORWARD));
    if (keyJustPressed(mapGbKey(KEY_FAST_FORWARD_TOGGLE)))
        fastForwardMode = !fastForwardMode;

    if (advanceFrame || keyJustPressed(mapGbKey(KEY_MENU) | mapGbKey(KEY_MENU_PAUSE) | KEY_TOUCH)) {
        if (keyJustPressed(mapGbKey(KEY_MENU_PAUSE)))
            pauseGameboy();

        advanceFrame = 0;
        forceReleaseKey(0xffff);
        fastForwardKey = false;
        fastForwardMode = false;
        buttonsPressed = 0xff;
        displayMenu();
    }

    if (keyJustPressed(mapGbKey(KEY_SCALE))) {
        setMenuOption("Scaling", !getMenuOption("Scaling"));
    }

    if (fastForwardKey || fastForwardMode) {
        sharedData->hyperSound = false;
    }
    else {
        sharedData->hyperSound = hyperSound;
    }

    if (keyJustPressed(mapGbKey(KEY_RESET)))
        resetGameboy();
}

// This is called 60 times per gameboy second, even if the lcd is off.
void Gameboy::gameboyUpdateVBlank() {
    gameboyFrameCounter++;

    readKeys();
    if (isMenuOn())
        updateMenu();
    else {
        gameboyCheckInput();
        if (gbsMode)
            gbsCheckInput();
    }

    if (gameboyPaused) {
        muteSND();
        while (gameboyPaused) {
            swiWaitForVBlank();
            readKeys();
            updateMenu();
        }
        unmuteSND();
    }

	if (gbsMode) {
        drawScreen(); // Just because it syncs with vblank...
	}
	else {
		drawScreen();
		soundUpdateVBlank();

		if (resettingGameboy) {
			initializeGameboy();
			resettingGameboy = false;
		}

        if (probingForBorder) {
            if (gameboyFrameCounter >= 450) {
                // Give up on finding a sgb border.
                probingForBorder = false;
                sgbBorderLoaded = false;
                resetGameboy();
            }
			return;
        }

        updateAutosave();

		if (cheatsEnabled)
			applyGSCheats();

        updateGbPrinter();
	}

    if (isConsoleOn() && !isMenuOn() && !consoleDebugOutput && (rawTime > lastRawTime))
    {
        setPrintConsole(menuConsole);
        consoleClear();
        int line=0;
        if (fpsOutput) {
            consoleClear();
            iprintf("FPS: %d\n", fps);
            line++;
        }
        fps = 0;
        if (timeOutput) {
            for (; line<23-1; line++)
                iprintf("\n");
            char *timeString = ctime(&rawTime);
            for (int i=0;; i++) {
                if (timeString[i] == ':') {
                    timeString += i-2;
                    break;
                }
            }
            char s[50];
            strncpy(s, timeString, 50);
            s[5] = '\0';
            int spaces = 31-strlen(s);
            for (int i=0; i<spaces; i++)
                iprintf(" ");
            iprintf("%s\n", s);
        }
        lastRawTime = rawTime;
    }
}

// This function can be called from weird contexts, so just set a flag to deal 
// with it later.
void Gameboy::resetGameboy() {
    resettingGameboy = true;
}

void Gameboy::pauseGameboy() {
    if (!gameboyPaused) {
        gameboyPaused = true;
    }
}
void Gameboy::unpauseGameboy() {
    if (gameboyPaused) {
        gameboyPaused = false;
    }
}
bool Gameboy::isGameboyPaused() {
    return gameboyPaused;
}

int soundCycles=0;
int extraCycles;
void Gameboy::runEmul()
{
    for (;;)
    {
        cyclesToEvent -= extraCycles;
        int cycles;
        if (halt)
            cycles = cyclesToEvent;
        else
            cycles = runOpcode(cyclesToEvent);

        bool opTriggeredInterrupt = cyclesToExecute == -1;
        cyclesToExecute = -1;

        cycles += extraCycles;

        cyclesToEvent = maxWaitCycles;
        extraCycles=0;

        cyclesSinceVblank += cycles;

        if (serialCounter > 0) {
            serialCounter -= cycles;
            if (serialCounter <= 0) {
                serialCounter = 0;
                linkReceivedData = 0xff;
                transferReady = true;
            }
            else
                setEventCycles(serialCounter);
        }
        if (transferReady) {
            if (nifiEnabled) {
                if (!(ioRam[0x02] & 1)) {
                    sendPacketByte(56, linkSendData);
                    timerStop(2);
                }
            }
            else if (printerEnabled) {
                sendGbPrinterByte(ioRam[0x01]);
            }
            else
                linkReceivedData = 0xff;
            ioRam[0x01] = linkReceivedData;
            requestInterrupt(SERIAL);
            ioRam[0x02] &= ~0x80;
            linkReceivedData = -1;
            transferReady = false;
        }

        updateTimers(cycles);

        soundCycles += cycles>>doubleSpeed;
        if (soundCycles >= cyclesToSoundEvent) {
            cyclesToSoundEvent = 10000;
            updateSound(soundCycles);
            soundCycles = 0;
        }
        setEventCycles(cyclesToSoundEvent);

        updateLCD(cycles);

        int interruptTriggered = ioRam[0x0F] & ioRam[0xFF];
        if (interruptTriggered) {
            /* Hack to fix Robocop 2 and LEGO Racers, possibly others. 
             * Interrupts can occur in the middle of an opcode. The result of 
             * this is that said opcode can read the resulting state - most 
             * importantly, it can read LY=144 before the vblank interrupt takes 
             * over. This is a decent approximation of that effect.
             * This has been known to break Megaman V boss intros, that's fixed 
             * by the "opTriggeredInterrupt" stuff.
             */
            if (!halt && !opTriggeredInterrupt)
                extraCycles += runOpcode(4);

            extraCycles += handleInterrupts(interruptTriggered);
        }
    }
}

void Gameboy::initLCD()
{
    gameboyPaused = false;
    setDoubleSpeed(0);

    scanlineCounter = 456*(doubleSpeed?2:1);
    phaseCounter = 456*153;
    timerCounter = 0;
    dividerCounter = 256;
    serialCounter = 0;

    initGbPrinter();

    // Timer stuff
    periods[0] = clockSpeed/4096;
    periods[1] = clockSpeed/262144;
    periods[2] = clockSpeed/65536;
    periods[3] = clockSpeed/16384;
    timerPeriod = periods[0];

    timerStop(2);
}

// Called either from startup, or when the BIOS writes to FF50.
void Gameboy::initGameboyMode() {
    gbRegs.af.b.l = 0xB0;
    gbRegs.bc.w = 0x0013;
    gbRegs.de.w = 0x00D8;
    gbRegs.hl.w = 0x014D;
    switch(resultantGBMode) {
        case 0: // GB
            gbRegs.af.b.h = 0x01;
            gbMode = GB;
            if (romSlot0[0x143] == 0x80 || romSlot1[0x143] == 0xC0)
                // Init the palette in case the bios overwrote it, since it 
                // assumed it was starting in GBC mode.
                initGFXPalette();
            break;
        case 1: // GBC
            gbRegs.af.b.h = 0x11;
            if (gbaModeOption)
                gbRegs.bc.b.h |= 1;
            gbMode = CGB;
            break;
        case 2: // SGB
            sgbMode = true;
            gbRegs.af.b.h = 0x01;
            gbMode = GB;
            initSGB();
            break;
    }
}

void Gameboy::checkLYC() {
    if (ioRam[0x44] == ioRam[0x45])
    {
        ioRam[0x41] |= 4;
        if (ioRam[0x41]&0x40)
            requestInterrupt(LCD);
    }
    else
        ioRam[0x41] &= ~4;
}

inline void Gameboy::updateLCD(int cycles)
{
    if (!(ioRam[0x40] & 0x80))		// If LCD is off
    {
        scanlineCounter = 456*(doubleSpeed?2:1);
        ioRam[0x44] = 0;
        ioRam[0x41] &= 0xF8;
        // Normally timing is synchronized with gameboy's vblank. If the screen 
        // is off, this code kicks in. The "phaseCounter" is a counter until the 
        // ds should check for input and whatnot.
        phaseCounter -= cycles;
        if (phaseCounter <= 0) {
            fps++;
            phaseCounter += 456*153*(doubleSpeed?2:1);
            cyclesSinceVblank=0;
            // Though not technically vblank, this is a good time to check for 
            // input and whatnot.
            gameboyUpdateVBlank();
        }
        return;
    }

    scanlineCounter -= cycles;

    if (scanlineCounter > 0) {
        setEventCycles(scanlineCounter);
        return;
    }

    switch(ioRam[0x41]&3)
    {
        case 2:
            {
                ioRam[0x41]++; // Set mode 3
                scanlineCounter += 172<<doubleSpeed;
                drawScanline(ioRam[0x44]);
            }
            break;
        case 3:
            {
                ioRam[0x41] &= ~3; // Set mode 0

                if (ioRam[0x41]&0x8)
                    requestInterrupt(LCD);

                scanlineCounter += 204<<doubleSpeed;

                drawScanline_P2(ioRam[0x44]);
                if (updateHblankDMA()) {
                    extraCycles += 50;
                }
            }
            break;
        case 0:
            {
                // fall through to next case
            }
        case 1:
            if (ioRam[0x44] == 0 && (ioRam[0x41]&3) == 1) { // End of vblank
                ioRam[0x41]++; // Set mode 2
                scanlineCounter += 80<<doubleSpeed;
            }
            else {
                ioRam[0x44]++;

                if (ioRam[0x44] < 144 || ioRam[0x44] >= 153) { // Not in vblank
                    if (ioRam[0x41]&0x20)
                    {
                        requestInterrupt(LCD);
                    }

                    if (ioRam[0x44] >= 153)
                    {
                        // Don't change the mode. Scanline 0 is twice as 
                        // long as normal - half of it identifies as being 
                        // in the vblank period.
                        ioRam[0x44] = 0;
                        scanlineCounter += 456<<doubleSpeed;
                    }
                    else { // End of hblank
                        ioRam[0x41] &= ~3;
                        ioRam[0x41] |= 2; // Set mode 2
                        if (ioRam[0x41]&0x20)
                            requestInterrupt(LCD);
                        scanlineCounter += 80<<doubleSpeed;
                    }
                }

                checkLYC();

                if (ioRam[0x44] >= 144) { // In vblank
                    scanlineCounter += 456<<doubleSpeed;

                    if (ioRam[0x44] == 144) // Beginning of vblank
                    {
                        ioRam[0x41] &= ~3;
                        ioRam[0x41] |= 1;   // Set mode 1

                        requestInterrupt(VBLANK);
                        if (ioRam[0x41]&0x10)
                            requestInterrupt(LCD);

                        fps++;
                        cyclesSinceVblank = scanlineCounter - (456<<doubleSpeed);
                        gameboyUpdateVBlank();
                    }
                }
            }

            break;
    }

    setEventCycles(scanlineCounter);
}

inline void Gameboy::updateTimers(int cycles)
{
    if (ioRam[0x07] & 0x4) // Timers enabled
    {
        timerCounter -= cycles;
        while (timerCounter <= 0)
        {
            int clocksAdded = (-timerCounter)/timerPeriod+1;
            timerCounter += timerPeriod*clocksAdded;
            int sum = ioRam[0x05]+clocksAdded;
            if (sum > 0xff) {
                requestInterrupt(TIMER);
                ioRam[0x05] = ioRam[0x06];
            }
            else
                ioRam[0x05] = (u8)sum;
        }
        // Set cycles until the timer will trigger an interrupt.
        // Reads from [0xff05] may be inaccurate.
        // However Castlevania and Alone in the Dark are extremely slow 
        // if this is updated each time [0xff05] is changed.
        setEventCycles(timerCounter+timerPeriod*(255-ioRam[0x05]));
    }
    dividerCounter -= cycles;
    if (dividerCounter <= 0) {
        int divsAdded = -dividerCounter/256+1;
        dividerCounter += divsAdded*256;
        ioRam[0x04] += divsAdded;
    }
    //setEventCycles(dividerCounter);
}


void Gameboy::requestInterrupt(int id)
{
    ioRam[0x0F] |= id;
    if (ioRam[0x0F] & ioRam[0xFF])
        cyclesToExecute = -1;
}

void Gameboy::setDoubleSpeed(int val) {
    if (val == 0) {
        if (doubleSpeed)
            scanlineCounter >>= 1;
        doubleSpeed = 0;
        ioRam[0x4D] &= ~0x80;
    }
    else {
        if (!doubleSpeed)
            scanlineCounter <<= 1;
        doubleSpeed = 1;
        ioRam[0x4D] |= 0x80;
    }
}


int loadRom(char* f)
{
    if (romFile != NULL)
        fclose(romFile);
    strcpy(filename, f);

    // Check if this is a GBS file
    gbsMode = (strcmpi(strrchr(filename, '.'), ".gbs") == 0);

    romFile = fopen(filename, "rb");
    if (romFile == NULL)
    {
        printLog("Error opening %s.\n", filename);
        return 1;
    }

    if (gbsMode) {
        fread(gbsHeader, 1, 0x70, romFile);
        gbsReadHeader();
        fseek(romFile, 0, SEEK_END);
        numRomBanks = (ftell(romFile)-0x70+0x3fff)/0x4000; // Get number of banks, rounded up
    }
    else {
        fseek(romFile, 0, SEEK_END);
        numRomBanks = (ftell(romFile)+0x3fff)/0x4000; // Get number of banks, rounded up
    }

    // Round numRomBanks to a power of 2
    int n=1;
    while (n < numRomBanks) n*=2;
    numRomBanks = n;

    //int rawRomSize = ftell(romFile);
    rewind(romFile);


    if (numRomBanks <= maxLoadedRomBanks)
        numLoadedRomBanks = numRomBanks;
    else
        numLoadedRomBanks = maxLoadedRomBanks;

    romSlot0 = romBankSlots;
    romSlot1 = romBankSlots + 0x4000;

    for (int i=0; i<numRomBanks; i++) {
        bankSlotIDs[i] = -1;
    }

    // Load rom banks and initialize all those "bank" arrays
    lastBanksUsed = std::vector<int>();
    // Read bank 0
    if (gbsMode) {
        bankSlotIDs[0] = 0;
        fseek(romFile, 0x70, SEEK_SET);
        fread(romBankSlots+gbsLoadAddress, 1, 0x4000-gbsLoadAddress, romFile);
    }
    else {
        bankSlotIDs[0] = 0;
        fseek(romFile, 0, SEEK_SET);
        fread(romBankSlots, 1, 0x4000, romFile);
    }
    // Read the rest of the banks
    for (int i=1; i<numLoadedRomBanks; i++) {
        bankSlotIDs[i] = i;
        fread(romBankSlots+0x4000*i, 1, 0x4000, romFile);
        lastBanksUsed.push_back(i);
    }

    strcpy(basename, filename);
    *(strrchr(basename, '.')) = '\0';
    strcpy(savename, basename);
    strcat(savename, ".sav");

    cgbFlag = romSlot0[0x143];
    romSize = romSlot0[0x148];
    ramSize = romSlot0[0x149];
    mapper  = romSlot0[0x147];

    int nameLength = 16;
    if (cgbFlag == 0x80 || cgbFlag == 0xc0)
        nameLength = 15;
    for (int i=0; i<nameLength; i++) 
        romTitle[i] = (char)romSlot0[i+0x134];
    romTitle[nameLength] = '\0';

    hasRumble = false;

    if (gbsMode) {
        MBC = MBC5;
        loadCheats(""); // Unloads previous cheats
    }
    else {
        switch (mapper) {
            case 0: case 8: case 9:
                MBC = MBC0; 
                break;
            case 1: case 2: case 3:
                MBC = MBC1;
                break;
            case 5: case 6:
                MBC = MBC2;
                break;
                //case 0xb: case 0xc: case 0xd:
                //MBC = MMM01;
                //break;
            case 0xf: case 0x10: case 0x11: case 0x12: case 0x13:
                MBC = MBC3;
                break;
                //case 0x15: case 0x16: case 0x17:
                //MBC = MBC4;
                //break;
            case 0x19: case 0x1a: case 0x1b: 
                MBC = MBC5;
                break;
            case 0x1c: case 0x1d: case 0x1e:
                MBC = MBC5;
                hasRumble = true;
                break;
            case 0x22:
                MBC = MBC7;
                break;
            case 0xea: /* Hack for SONIC5 */
                MBC = MBC1;
                break;
            case 0xfe:
                MBC = HUC3;
                break;
            case 0xff:
                MBC = HUC1;
                break;
            default:
                printLog("Unsupported MBC %02x\n", mapper);
                return 1;
        }

        // Little hack to preserve "quickread" from gbcpu.cpp.
        if (biosExists) {
            for (int i=0x100; i<0x150; i++)
                bios[i] = romSlot0[i];
        }

        suspendStateExists = checkStateExists(-1);

        // Load cheats
        char nameBuf[100];
        siprintf(nameBuf, "%s.cht", basename);
        loadCheats(nameBuf);

    } // !gbsMode

    // If we've loaded everything, close the rom file
    if (numRomBanks <= numLoadedRomBanks) {
        fclose(romFile);
        romFile = NULL;
    }

    loadSave();

    return 0;
}

void unloadRom() {
    doAtVBlank(clearGFX);

    gameboySyncAutosave();
    if (saveFile != NULL)
        fclose(saveFile);
    saveFile = NULL;
    // unload previous save
    if (externRam != NULL) {
        free(externRam);
        externRam = NULL;
    }
}

int loadSave()
{
    if (gbsMode)
        numRamBanks = 1;
    else {
        // Get the game's external memory size and allocate the memory
        switch(ramSize)
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
                printLog("Invalid RAM bank number: %x\nDefaulting to 4 banks\n", ramSize);
                numRamBanks = 4;
                break;
        }
        if (MBC == MBC2)
            numRamBanks = 1;
    }

    if (numRamBanks == 0)
        return 0;

    externRam = (u8*)malloc(numRamBanks*0x2000);

    if (gbsMode)
        return 0; // GBS files don't get to save.

    // Now load the data.
    saveFile = fopen(savename, "r+b");
    int neededFileSize = numRamBanks*0x2000;
    if (MBC == MBC3 || MBC == HUC3)
        neededFileSize += sizeof(clockStruct);

    int fileSize = 0;
    if (saveFile) {
        fseek(saveFile, 0, SEEK_END);
        fileSize = ftell(saveFile);
        fseek(saveFile, 0, SEEK_SET);
    }

    if (!saveFile || fileSize < neededFileSize) {
        fclose(saveFile);

        // Extend the size of the file, or create it
        if (!saveFile) {
            saveFile = fopen(savename, "wb");
            fseek(saveFile, neededFileSize-1, SEEK_SET);
            fputc(0, saveFile);
        }
        else {
            saveFile = fopen(savename, "ab");
            for (; fileSize<neededFileSize; fileSize++)
                fputc(0, saveFile);
        }
        fclose(saveFile);

        saveFile = fopen(savename, "r+b");
    }

    fread(externRam, 1, 0x2000*numRamBanks, saveFile);

    switch (MBC) {
        case MBC3:
        case HUC3:
            fread(&gbClock, 1, sizeof(gbClock), saveFile);
            break;
    }

    // Get the save file's sectors on the sd card.
    // I do this by writing a byte, then finding the area of the cache marked dirty.

    flushFatCache();
    devoptab_t* devops = (devoptab_t*)GetDeviceOpTab ("sd");
    PARTITION* partition = (PARTITION*)devops->deviceData;
    CACHE* cache = partition->cache;

    memset(saveFileSectors, -1, sizeof(saveFileSectors));
    for (int i=0; i<numRamBanks*0x2000/512; i++) {
        fseek(saveFile, i*512, SEEK_SET);
        fputc(externRam[i*512], saveFile);
        bool found=false;
        for (int j=0; j<FAT_CACHE_SIZE; j++) {
            if (cache->cacheEntries[j].dirty) {
                saveFileSectors[i] = cache->cacheEntries[j].sector + (i%(cache->sectorsPerPage));
                cache->cacheEntries[j].dirty = false;
                found = true;
                break;
            }
        }
        if (!found) {
            printLog("couldn't find save file sector\n");
        }
    }

    if (numRamBanks == 1) {
        writeSaveFileSectors(0, 8);
        writeSaveFileSectors(8, 8);
    }

    return 0;
}

int saveGame()
{
    if (numRamBanks == 0 || saveFile == NULL)
        return 0;

    fseek(saveFile, 0, SEEK_SET);

    fwrite(externRam, 1, 0x2000*numRamBanks, saveFile);

    switch (MBC) {
        case MBC3:
        case HUC3:
            fwrite(&gbClock, 1, sizeof(gbClock), saveFile);
            break;
    }

    flushFatCache();

    return 0;
}

void gameboySyncAutosave() {
    if (!autosaveStarted)
        return;

    numSaveWrites = 0;
    wroteToSramThisFrame = false;

    int numSectors = 0;
    // iterate over each 512-byte sector
    for (int i=0; i<numRamBanks*0x2000/512; i++) {
        if (dirtySectors[i]) {
            dirtySectors[i] = false;

            // If only 1 bank, it seems more efficient to write multiple sectors 
            // at once - at least, on the Acekard 2i.
            if (numRamBanks == 1) {
                writeSaveFileSectors(i/8*8, 8);
                for (int j=i; j<(i/8*8)+8; j++)
                    dirtySectors[j] = false;
            }
            else {
                // For bigger saves, writing one sector at a time seems to work better.
                writeSaveFileSectors(i, 1);
                numSectors++;
            }
        }
    }
    printLog("SAVE %d sectors\n", numSectors);
    flushFatCache(); // This should do nothing, unless the RTC was written to.

    framesSinceAutosaveStarted = 0;
    autosaveStarted = false;
}

void updateAutosave() {
    if (autosaveStarted)
        framesSinceAutosaveStarted++;

    if (framesSinceAutosaveStarted >= 120 ||     // Executes when sram is written to for 120 consecutive frames, or
        (!saveModified && wroteToSramThisFrame)) { // when a full frame has passed since sram was last written to.
        gameboySyncAutosave();
    }
    if (saveModified) {
        wroteToSramThisFrame = true;
        autosaveStarted = true;
        saveModified = false;
    }
}




const int STATE_VERSION = 5;

struct StateStruct {
    // version
    // bg/sprite PaletteData
    // vram
    // wram
    // hram
    // sram
    Registers regs;
    int halt, ime;
    bool doubleSpeed, biosOn;
    int gbMode;
    int romBank, ramBank, wramBank, vramBank;
    int memoryModel;
    clockStruct clock;
    int scanlineCounter, timerCounter, phaseCounter, dividerCounter;
    // v2
    int serialCounter;
    // v3
    bool ramEnabled;
    // MBC-specific stuff
    // v4
    //  bool sgbMode;
    //  If sgbMode == true:
    //   int sgbPacketLength;
    //   int sgbPacketsTransferred;
    //   int sgbPacketBit;
    //   u8 sgbCommand;
    //   u8 gfxMask;
    //   u8[20*18] sgbMap;
};

void Gameboy::saveState(int stateNum) {
    FILE* outFile;
    StateStruct state;
    char statename[100];

    if (stateNum == -1)
        siprintf(statename, "%s.yss", basename);
    else
        siprintf(statename, "%s.ys%d", basename, stateNum);
    outFile = fopen(statename, "w");

    if (outFile == 0) {
        printMenuMessage("Error opening file for writing.");
        return;
    }

    state.regs = gbRegs;
    state.halt = halt;
    state.ime = ime;
    state.doubleSpeed = doubleSpeed;
    state.biosOn = biosOn;
    state.gbMode = gbMode;
    state.romBank = romBank;
    state.ramBank = currentRamBank;
    state.wramBank = wramBank;
    state.vramBank = vramBank;
    state.memoryModel = memoryModel;
    state.clock = gbClock;
    state.scanlineCounter = scanlineCounter;
    state.timerCounter = timerCounter;
    state.phaseCounter = phaseCounter;
    state.dividerCounter = dividerCounter;
    state.serialCounter = serialCounter;
    state.ramEnabled = ramEnabled;

    fwrite(&STATE_VERSION, sizeof(int), 1, outFile);
    fwrite((char*)bgPaletteData, 1, sizeof(bgPaletteData), outFile);
    fwrite((char*)sprPaletteData, 1, sizeof(sprPaletteData), outFile);
    fwrite((char*)vram, 1, sizeof(vram), outFile);
    fwrite((char*)wram, 1, sizeof(wram), outFile);
    fwrite((char*)hram, 1, 0x200, outFile);
    fwrite((char*)externRam, 1, 0x2000*numRamBanks, outFile);

    fwrite((char*)&state, 1, sizeof(StateStruct), outFile);

    switch (MBC) {
        case HUC3:
            fwrite(&HuC3Mode,  1, sizeof(u8), outFile);
            fwrite(&HuC3Value, 1, sizeof(u8), outFile);
            fwrite(&HuC3Shift, 1, sizeof(u8), outFile);
            break;
    }

    fwrite(&sgbMode, 1, sizeof(bool), outFile);
    if (sgbMode) {
        fwrite(&sgbPacketLength, 1, sizeof(int), outFile);
        fwrite(&sgbPacketsTransferred, 1, sizeof(int), outFile);
        fwrite(&sgbPacketBit, 1, sizeof(int), outFile);
        fwrite(&sgbCommand, 1, sizeof(u8), outFile);
        fwrite(&gfxMask, 1, sizeof(u8), outFile);
        fwrite(sgbMap, 1, sizeof(sgbMap), outFile);
    }

    fclose(outFile);
}

int Gameboy::loadState(int stateNum) {
    FILE *inFile;
    StateStruct state;
    char statename[256];
    int version;

    memset(&state, 0, sizeof(StateStruct));

    if (stateNum == -1)
        siprintf(statename, "%s.yss", basename);
    else
        siprintf(statename, "%s.ys%d", basename, stateNum);
    inFile = fopen(statename, "r");

    if (inFile == 0) {
        printMenuMessage("State doesn't exist.");
        return 1;
    }

    fread(&version, sizeof(int), 1, inFile);

    if (version == 0 || version > STATE_VERSION) {
        printMenuMessage("State is from an incompatible version.");
        return 1;
    }

    fread((char*)bgPaletteData, 1, sizeof(bgPaletteData), inFile);
    fread((char*)sprPaletteData, 1, sizeof(sprPaletteData), inFile);
    fread((char*)vram, 1, sizeof(vram), inFile);
    fread((char*)wram, 1, sizeof(wram), inFile);
    fread((char*)hram, 1, 0x200, inFile);

    if (version <= 4 && ramSize == 0x04)
        // Value "0x04" for ram size wasn't interpreted correctly before
        fread((char*)externRam, 1, 0x2000*4, inFile);
    else
        fread((char*)externRam, 1, 0x2000*numRamBanks, inFile);

    fread((char*)&state, 1, sizeof(StateStruct), inFile);

    /* MBC-specific values have been introduced in v3 */
    if (version >= 3) {
        switch (MBC) {
            case MBC3:
                if (version == 3) {
                    u8 rtcReg;
                    fread(&rtcReg, 1, sizeof(u8), inFile);
                    if (rtcReg != 0)
                        currentRamBank = rtcReg;
                }
                break;
            case HUC3:
                fread(&HuC3Mode,  1, sizeof(u8), inFile);
                fread(&HuC3Value, 1, sizeof(u8), inFile);
                fread(&HuC3Shift, 1, sizeof(u8), inFile);
                break;
        }

        fread(&sgbMode, 1, sizeof(bool), inFile);
        if (sgbMode) {
            fread(&sgbPacketLength, 1, sizeof(int), inFile);
            fread(&sgbPacketsTransferred, 1, sizeof(int), inFile);
            fread(&sgbPacketBit, 1, sizeof(int), inFile);
            fread(&sgbCommand, 1, sizeof(u8), inFile);
            fread(&gfxMask, 1, sizeof(u8), inFile);
            fread(sgbMap, 1, sizeof(sgbMap), inFile);
        }
    }
    else
        sgbMode = false;


    fclose(inFile);
    if (stateNum == -1) {
        unlink(statename);
        suspendStateExists = false;
    }

    gbRegs = state.regs;
    halt = state.halt;
    ime = state.ime;
    doubleSpeed = state.doubleSpeed;
    biosOn = state.biosOn;
    if (!biosExists)
        biosOn = false;
    gbMode = state.gbMode;
    romBank = state.romBank;
    currentRamBank = state.ramBank;
    wramBank = state.wramBank;
    vramBank = state.vramBank;
    memoryModel = state.memoryModel;
    gbClock = state.clock;
    scanlineCounter = state.scanlineCounter;
    timerCounter = state.timerCounter;
    phaseCounter = state.phaseCounter;
    dividerCounter = state.dividerCounter;
    serialCounter = state.serialCounter;
    ramEnabled = state.ramEnabled;
    if (version < 3)
        ramEnabled = true;

    transferReady = false;
    timerPeriod = periods[ioRam[0x07]&0x3];
    cyclesToEvent = 1;

    mapMemory();
    setDoubleSpeed(doubleSpeed);


    if (autoSavingEnabled && stateNum != -1)
        saveGame(); // Synchronize save file on sd with file in ram

    refreshGFX();
    refreshSND();

    return 0;
}

void Gameboy::deleteState(int stateNum) {
    if (!checkStateExists(stateNum))
        return;

    char statename[256];

    if (stateNum == -1)
        siprintf(statename, "%s.yss", basename);
    else
        siprintf(statename, "%s.ys%d", basename, stateNum);
    unlink(statename);
}

bool Gameboy::checkStateExists(int stateNum) {
    char statename[256];

    if (stateNum == -1)
        siprintf(statename, "%s.yss", basename);
    else
        siprintf(statename, "%s.ys%d", basename, stateNum);
    return access(statename, R_OK) == 0;
    /*
    file = fopen(statename, "r");

    if (file == 0) {
        return false;
    }
    fclose(file);
    return true;
    */
}

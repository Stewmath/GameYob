#include <nds.h>
#include <stdlib.h>
#include <stdio.h>
#include "gameboy.h"
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

int scanlineCounter;
int doubleSpeed;

time_t rawTime;
time_t lastRawTime;

int fps;
bool fpsOutput=true;
bool timeOutput=true;
bool fastForwardMode = false; // controlled by the menu
bool fastForwardKey = false;  // only while its hotkey is pressed

bool yellowHack;

// ...what is phase? I think I made that up. Used for timing when the gameboy 
// screen is off.
int phaseCounter;
int dividerCounter;
int serialCounter;

int timerCounter;
int timerPeriod;
long periods[4];

int gbMode;
bool sgbMode;

int cyclesToEvent;
int maxWaitCycles;

bool resettingGameboy = false;

bool probingForBorder=false;


inline void setEventCycles(int cycles) {
    if (cycles < cyclesToEvent) {
        cyclesToEvent = cycles;
        /*
        if (cyclesToEvent <= 0) {
           cyclesToEvent = 1;
        }
        */
    }
}

// Called once every gameboy vblank
void updateInput() {
    if (resettingGameboy) {
        initializeGameboy();
        resettingGameboy = false;
    }

    if (probingForBorder)
        return;

    if (cheatsEnabled)
        applyGSCheats();

    readKeys();
    handleEvents();		// Input mostly
    if (!consoleDebugOutput && (rawTime > lastRawTime))
    {
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
void resetGameboy() {
    resettingGameboy = true;
}

int soundCycles=0;
int extraCycles;
void runEmul()
{
    for (;;)
    {
        cyclesToEvent -= extraCycles;
        int cycles;
        if (halt)
            cycles = cyclesToEvent;
        else
            cycles = runOpcode(cyclesToEvent);
        cycles += extraCycles;

        cyclesToEvent = maxWaitCycles;
        extraCycles=0;

        if (serialCounter > 0) {
            serialCounter -= cycles;
            if (serialCounter <= 0) {
                serialCounter = 0;
                packetData = 0xff;
                transferReady = true;
            }
            else
                setEventCycles(serialCounter);
        }
        if (transferReady) {
            if (!(ioRam[0x02] & 1)) {
                sendPacketByte(56, sendData);
            }
            timerStop(2);
            ioRam[0x01] = packetData;
            requestInterrupt(SERIAL);
            ioRam[0x02] &= ~0x80;
            packetData = -1;
            transferReady = false;
        }
        updateTimers(cycles);
        soundCycles += cycles;
        if (soundCycles >= 6666) {
            updateSound(soundCycles);
            soundCycles = 0;
        }

        updateLCD(cycles);

        int interruptTriggered = ioRam[0x0F] & ioRam[0xFF];
        if (interruptTriggered)
            extraCycles += handleInterrupts(interruptTriggered);
    }
}

void initLCD()
{
    // Pokemon Yellow hack: I need to intentionally SLOW DOWN emulation for 
    // Pikachu's pitch to sound right...
    yellowHack = strcmp(getRomTitle(), "POKEMON YELLOW") == 0;
    if (yellowHack && !(fastForwardMode || fastForwardKey))
        maxWaitCycles = 50;
    else
        maxWaitCycles = 800;

    setDoubleSpeed(0);

    scanlineCounter = 456*(doubleSpeed?2:1);
    phaseCounter = 456*153;
    timerCounter = 0;
    dividerCounter = 256;
    serialCounter = 0;

    // Timer stuff
    periods[0] = clockSpeed/4096;
    periods[1] = clockSpeed/262144;
    periods[2] = clockSpeed/65536;
    periods[3] = clockSpeed/16384;
    timerPeriod = periods[0];

    timerStop(2);
}

// Called either from startup, or when the BIOS writes to FF50.
void initGameboyMode() {
    gbRegs.af.b.l = 0xB0;
    gbRegs.bc.w = 0x0013;
    gbRegs.de.w = 0x00D8;
    gbRegs.hl.w = 0x014D;
    switch(resultantGBMode) {
        case 0: // GB
            gbRegs.af.b.h = 0x01;
            gbMode = GB;
            if (rom[0][0x143] == 0x80 || rom[0][0x143] == 0xC0)
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


inline void updateLCD(int cycles)
{
    if (!(ioRam[0x40] & 0x80))		// If LCD is off
    {
        scanlineCounter = 456*(doubleSpeed?2:1);;
        ioRam[0x44] = 0;
        ioRam[0x41] &= 0xF8;
        // Normally timing is synchronized with gameboy's vblank. If the screen 
        // is off, this code kicks in. The "phaseCounter" is a counter until the 
        // ds should check for input and whatnot.
        phaseCounter -= cycles;
        if (phaseCounter <= 0) {
            fps++;
            phaseCounter += 456*153*(doubleSpeed?2:1);
            drawScreen();   // drawScreen recognizes the screen is disabled and makes it all white.
            updateInput();
        }
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
                scanlineCounter += 204<<doubleSpeed;

                if (ioRam[0x41]&0x8)
                {
                    requestInterrupt(LCD);
                }

                drawScanlinePalettes(ioRam[0x44]);
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
            if (ioRam[0x44] == 0 && (ioRam[0x41]&3) == 1) {
                ioRam[0x41]++; // Set mode 2
                scanlineCounter += 80<<doubleSpeed;
            }
            else {
                ioRam[0x44]++;

                if (ioRam[0x44] < 144 || ioRam[0x44] >= 153) {
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
                    else {
                        ioRam[0x41] &= ~3;
                        ioRam[0x41] |= 2; // Set mode 2
                        scanlineCounter += 80<<doubleSpeed;
                    }
                }
                else if (ioRam[0x44] == 144)
                {
                    ioRam[0x41] &= ~3;
                    ioRam[0x41] |= 1;

                    requestInterrupt(VBLANK);
                    if (ioRam[0x41]&0x10)
                    {
                        requestInterrupt(LCD);
                    }

                    fps++;
                    drawScreen();
                    updateInput();
                }
                if (ioRam[0x44] >= 144) {
                    scanlineCounter += 456<<doubleSpeed;
                }

                // LYC check
                if (ioRam[0x44] == ioRam[0x45])
                {
                    ioRam[0x41] |= 4;
                    if (ioRam[0x41]&0x40)
                        requestInterrupt(LCD);
                }
                else
                    ioRam[0x41] &= ~4;
            }

            break;
    }

    setEventCycles(scanlineCounter);
}

inline void updateTimers(int cycles)
{
    if (ioRam[0x07] & 0x4)
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


void requestInterrupt(int id)
{
    ioRam[0x0F] |= id;
    if (ioRam[0x0F] & ioRam[0xFF])
        cyclesToExecute = 0;
}

void setDoubleSpeed(int val) {
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

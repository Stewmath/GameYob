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
#include "gbs.h"

int scanlineCounter;
int doubleSpeed;

time_t rawTime;
time_t lastRawTime;

int fps;
bool fpsOutput=true;
bool timeOutput=true;
bool fastForwardMode = false; // controlled by the toggle hotkey
bool fastForwardKey = false;  // only while its hotkey is pressed

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

const int maxWaitCycles=1000000;
int cyclesToEvent;
int cyclesSinceVblank=0;

bool resettingGameboy = false;

bool probingForBorder=false;

bool wroteToSramThisFrame=false;


void setEventCycles(int cycles) {
    if (cycles < cyclesToEvent) {
        cyclesToEvent = cycles;
        /*
        if (cyclesToEvent <= 0) {
           cyclesToEvent = 1;
        }
        */
    }
}

// This is called 60 times per second, even if the lcd is off.
void gameboyUpdateVBlank() {
	if (gbsMode) {
        drawScreen(); // Just because it syncs with vblank...
		updateGBSInput();
	}
	else {
		drawScreen();
		soundUpdateVBlank();
		updateInput();

		if (resettingGameboy) {
			initializeGameboy();
			resettingGameboy = false;
		}

		if (probingForBorder)
			return;

		// Check autosaving stuff
		if (saveModified) {
			wroteToSramThisFrame = true;
			saveModified = false;
		}
		else if (wroteToSramThisFrame) { // This executes when a full frame has passed since sram was last written to.
			printLog("SAVE %d\n", numSaveWrites);
			numSaveWrites = 0;
			wroteToSramThisFrame = false;

			fseek(saveFile, autosaveStart, SEEK_SET);

			for (int i=autosaveStart/0x2000; i<=autosaveEnd/0x2000; i++) {
				int start = (i == autosaveStart/0x2000 ? autosaveStart : i*0x2000);
				int end = (i == autosaveEnd/0x2000 ? autosaveEnd+1 : (i+1)*0x2000);
				fwrite(externRam[i]+(start&0x1fff), 1, end-start, saveFile);
			}

			flushFatCache();

			autosaveStart = -1;
			autosaveEnd = -1;
		}

		if (cheatsEnabled)
			applyGSCheats();

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

void initLCD()
{
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

void checkLYC() {
    if (ioRam[0x44] == ioRam[0x45])
    {
        ioRam[0x41] |= 4;
        if (ioRam[0x41]&0x40)
            requestInterrupt(LCD);
    }
    else
        ioRam[0x41] &= ~4;
}

inline void updateLCD(int cycles)
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

inline void updateTimers(int cycles)
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


void requestInterrupt(int id)
{
    ioRam[0x0F] |= id;
    if (ioRam[0x0F] & ioRam[0xFF])
        cyclesToExecute = -1;
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

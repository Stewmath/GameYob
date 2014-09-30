// Bits & pieces adapted from libnds's console.c

#include <3ds.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/iosupport.h>
#include "default_font_bin.h"
#include "3dsgfx.h"

struct PrintConsole {
    int cursorX, cursorY;
    int prevCursorX, prevCursorY;
    int consoleWidth, consoleHeight;
    u8* framebuffer;
};

const u32 TEXT_COLOR = RGB24(0xff, 0xff, 0xff);
const u32 BG_COLOR = RGB24(0x00, 0x00, 0x00);

const int CHAR_WIDTH = 8;
const int CHAR_HEIGHT = 8;

PrintConsole* currentConsole = NULL;


void newRow() {
    currentConsole->cursorY++;

    if (currentConsole->cursorY >= currentConsole->consoleHeight) {
        currentConsole->cursorY--;

        for (int j=currentConsole->consoleHeight-2; j>=0; j--) {
            for (int y=0; y<CHAR_HEIGHT; y++) {
                for (int x=0; x<currentConsole->consoleWidth*CHAR_WIDTH; x++) {
                    drawPixel(currentConsole->framebuffer, x, j*CHAR_HEIGHT+y,
                            getPixel(currentConsole->framebuffer, x, (j+1)*CHAR_HEIGHT+y));
                }
            }
        }
    }
}

void consoleDrawChar(char c) {
    int x = currentConsole->cursorX*CHAR_WIDTH;
    int y = currentConsole->cursorY*CHAR_HEIGHT;

    for (int cy=0; cy<CHAR_HEIGHT; cy++) {
        for (int cx=0; cx<CHAR_WIDTH; cx+=2) {
            u8 byte = default_font_bin[c*CHAR_WIDTH*CHAR_HEIGHT/2 + cy*4 + cx/2];
            if (byte&0xf)
                drawPixel(currentConsole->framebuffer, x+cx, y+cy, TEXT_COLOR);
            else
                drawPixel(currentConsole->framebuffer, x+cx, y+cy, BG_COLOR);
            if (byte>>4)
                drawPixel(currentConsole->framebuffer, x+cx+1, y+cy, TEXT_COLOR);
            else
                drawPixel(currentConsole->framebuffer, x+cx+1, y+cy, BG_COLOR);
        }
    }
}
void consolePrintChar(char c) {
	if (c == 0)
        return;

	if(currentConsole->cursorX >= currentConsole->consoleWidth) {
		currentConsole->cursorX  = 0;

		newRow();
	}

	switch(c) {
		/*
		The only special characters we will handle are tab (\t), carriage return (\r), line feed (\n)
		and backspace (\b).
		Carriage return & line feed will function the same: go to next line and put cursor at the beginning.
		For everything else, use VT sequences.

		Reason: VT sequences are more specific to the task of cursor placement.
		The special escape sequences \b \f & \v are archaic and non-portable.
		*/
        // Backspace
		case 8:
			currentConsole->cursorX--;

			if(currentConsole->cursorX < 0) {
				if(currentConsole->cursorY > 0) {
					currentConsole->cursorX = currentConsole->consoleWidth - 1;
					currentConsole->cursorY--;
				} else {
					currentConsole->cursorX = 0;
				}
			}

            consoleDrawChar(' ');
			break;

            // Tab
		case 9:
			currentConsole->cursorX += 4 - (currentConsole->cursorX)%4;
			break;
            // Newline
		case 10:
			newRow();
            // Carriage return
		case 13:
			currentConsole->cursorX = 0;
			break;
		default:
            consoleDrawChar(c);
			currentConsole->cursorX++;
			break;
	}
}

static void consoleCls(char mode) {

	int i = 0;
	int colTemp,rowTemp;

	switch (mode)
	{
	case '0':
		{
			colTemp = currentConsole->cursorX ;
			rowTemp = currentConsole->cursorY ;

			while(i++ < ((currentConsole->consoleHeight * currentConsole->consoleWidth) - (rowTemp * currentConsole->consoleWidth + colTemp)))
				consolePrintChar(' ');

			currentConsole->cursorX  = colTemp;
			currentConsole->cursorY  = rowTemp;
			break;
		}
	case '1':
		{
			colTemp = currentConsole->cursorX ;
			rowTemp = currentConsole->cursorY ;

			currentConsole->cursorY  = 0;
			currentConsole->cursorX  = 0;

			while (i++ < (rowTemp * currentConsole->consoleWidth + colTemp))
				consolePrintChar(' ');

			currentConsole->cursorX  = colTemp;
			currentConsole->cursorY  = rowTemp;
			break;
		}
	case '2':
		{
			currentConsole->cursorY  = 0;
			currentConsole->cursorX  = 0;

			while(i++ < currentConsole->consoleHeight * currentConsole->consoleWidth)
				consolePrintChar(' ');

			currentConsole->cursorY  = 0;
			currentConsole->cursorX  = 0;
			break;
		}
	}
}

static void consoleClearLine(char mode) {
	int i = 0;
	int colTemp;

	switch (mode)
	{
	case '0':
		{
			colTemp = currentConsole->cursorX ;

			while(i++ < (currentConsole->consoleWidth - colTemp)) {
				consolePrintChar(' ');
			}

			currentConsole->cursorX  = colTemp;

			break;
		}
	case '1':
		{
			colTemp = currentConsole->cursorX ;

			currentConsole->cursorX  = 0;

			while(i++ < ((currentConsole->consoleWidth - colTemp)-2)) {
				consolePrintChar(' ');
			}

			currentConsole->cursorX  = colTemp;

			break;
		}
	case '2':
		{
			colTemp = currentConsole->cursorX ;

			currentConsole->cursorX  = 0;

			while(i++ < currentConsole->consoleWidth) {
				consolePrintChar(' ');
			}

			currentConsole->cursorX  = colTemp;

			break;
		}
	default:
		{
			colTemp = currentConsole->cursorX ;

			while(i++ < (currentConsole->consoleWidth - colTemp)) {
				consolePrintChar(' ');
			}

			currentConsole->cursorX  = colTemp;

			break;
		}
	}
}

ssize_t con_write(struct _reent *r,int fd,const char *ptr, size_t len) {

	char chr;

	uint i, count = 0;
	char *tmp = (char*)ptr;
	int intensity = 0;

	if(!tmp || len<=0) return -1;

	i = 0;

	while(i<len) {

		chr = *(tmp++);
		i++; count++;

		if ( chr == 0x1b && *tmp == '[' ) {
			bool escaping = true;
			char *escapeseq	= tmp;
			int escapelen = 0;

			do {
				chr = *(tmp++);
				i++; count++; escapelen++;
				int parameter;

				switch (chr) {
					/////////////////////////////////////////
					// Cursor directional movement
					/////////////////////////////////////////
					case 'A':
						siscanf(escapeseq,"[%dA", &parameter);
						currentConsole->cursorY  =  (currentConsole->cursorY  - parameter) < 0 ? 0 : currentConsole->cursorY  - parameter;
						escaping = false;
						break;
					case 'B':
						siscanf(escapeseq,"[%dB", &parameter);
						currentConsole->cursorY  =  (currentConsole->cursorY  + parameter) > currentConsole->consoleHeight - 1 ? currentConsole->consoleHeight - 1 : currentConsole->cursorY  + parameter;
						escaping = false;
						break;
					case 'C':
						siscanf(escapeseq,"[%dC", &parameter);
						currentConsole->cursorX  =  (currentConsole->cursorX  + parameter) > currentConsole->consoleWidth - 1 ? currentConsole->consoleWidth - 1 : currentConsole->cursorX  + parameter;
						escaping = false;
						break;
					case 'D':
						siscanf(escapeseq,"[%dD", &parameter);
						currentConsole->cursorX  =  (currentConsole->cursorX  - parameter) < 0 ? 0 : currentConsole->cursorX  - parameter;
						escaping = false;
						break;
						/////////////////////////////////////////
						// Cursor position movement
						/////////////////////////////////////////
					case 'H':
					case 'f':
						siscanf(escapeseq,"[%d;%df", &currentConsole->cursorY , &currentConsole->cursorX );
						escaping = false;
						break;
						/////////////////////////////////////////
						// Screen clear
						/////////////////////////////////////////
					case 'J':
						consoleCls(escapeseq[escapelen-2]);
						escaping = false;
						break;
						/////////////////////////////////////////
						// Line clear
						/////////////////////////////////////////
					case 'K':
						consoleClearLine(escapeseq[escapelen-2]);
						escaping = false;
						break;
						/////////////////////////////////////////
						// Save cursor position
						/////////////////////////////////////////
					case 's':
						currentConsole->prevCursorX  = currentConsole->cursorX ;
						currentConsole->prevCursorY  = currentConsole->cursorY ;
						escaping = false;
						break;
						/////////////////////////////////////////
						// Load cursor position
						/////////////////////////////////////////
					case 'u':
						currentConsole->cursorX  = currentConsole->prevCursorX ;
						currentConsole->cursorY  = currentConsole->prevCursorY ;
						escaping = false;
						break;
						/////////////////////////////////////////
						// Color scan codes
						/////////////////////////////////////////
					case 'm':
						siscanf(escapeseq,"[%d;%dm", &parameter, &intensity);

						//only handle 30-37,39 and intensity for the color changes
						parameter -= 30;

						//39 is the reset code
						if(parameter == 9){
							parameter = 15;
						}
						else if(parameter > 8){
							parameter -= 2;
						}
						else if(intensity){
							parameter += 8;
						}
                        /*
						if(parameter < 16 && parameter >= 0){
							currentConsole->fontCurPal = parameter << 12;
						}
                        */

						escaping = false;
						break;
				}
			} while (escaping);
			continue;
		}

		consolePrintChar(chr);
	}

	return count;
}

static const devoptab_t dotab_stdout = {
	"con",
	0,
	NULL,
	NULL,
	con_write,
	NULL,
	NULL,
	NULL
};

void consoleInitBottom() {
    if (currentConsole == NULL)
        currentConsole = (PrintConsole*)malloc(sizeof(PrintConsole));
    currentConsole->cursorX = 0;
    currentConsole->cursorY = 0;
    currentConsole->consoleWidth = BOTTOM_SCREEN_WIDTH / CHAR_WIDTH;
    currentConsole->consoleHeight = BOTTOM_SCREEN_HEIGHT / CHAR_HEIGHT;

    currentConsole->framebuffer = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);

    devoptab_list[STD_OUT] = &dotab_stdout;
    devoptab_list[STD_ERR] = &dotab_stdout;

    setvbuf(stdout, NULL , _IONBF, 0);
    setvbuf(stderr, NULL , _IONBF, 0);
}

/*
void myprintf(const char* s) {
    while (*s != '\0') {
        printCharacter(*s++);
    }
}
*/

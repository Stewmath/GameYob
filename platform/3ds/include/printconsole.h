#pragma once

struct PrintConsole {
    int cursorX, cursorY;
    int prevCursorX, prevCursorY;
    int consoleWidth, consoleHeight;
    u8* framebuffer;
};

void consoleInitBottom();
void consoleSetScreen(int screen);

const int CHAR_WIDTH = 8;
const int CHAR_HEIGHT = 8;


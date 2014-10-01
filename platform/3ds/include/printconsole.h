#pragma once

struct PrintConsole {
    int cursorX, cursorY;
    int prevCursorX, prevCursorY;
    int consoleWidth, consoleHeight;
    u8* framebuffer;
};

void consoleInitBottom();
void consoleSetScreen(int screen);

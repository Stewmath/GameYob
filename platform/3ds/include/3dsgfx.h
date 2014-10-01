#pragma once

u32 getPixel(u8* framebuffer, int x, int y);
void drawPixel(u8* framebuffer, int x, int y, u32 color);

inline u32 RGB24(int red, int green, int blue) {
    return red<<16 | green<<8 | blue;
}

const int TOP_SCREEN_WIDTH = 400;
const int TOP_SCREEN_HEIGHT = 240;

const int BOTTOM_SCREEN_WIDTH = 320;
const int BOTTOM_SCREEN_HEIGHT = 240;

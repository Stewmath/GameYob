#include "3dsgfx.h"

u32 getPixel(u8* framebuffer, int x, int y) {
    u8* ptr = framebuffer + (x*TOP_SCREEN_HEIGHT + y)*3;
    return *ptr | *(ptr+1)<<8 | *(ptr+2)<<16;
}
void drawPixel(u8* framebuffer, int x, int y, u32 color) {
    y = TOP_SCREEN_HEIGHT - y - 1;

    u8* ptr = framebuffer + (x*TOP_SCREEN_HEIGHT + y)*3;

    *(u16*)ptr = color;
    *(ptr+2) = color>>16;
}


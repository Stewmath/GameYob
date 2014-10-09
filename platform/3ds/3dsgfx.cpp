#include <3ds.h>
#include "3dsgfx.h"

u8 gfxBuffer = 0;

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

u8* gfxGetActiveFramebuffer(gfxScreen_t screen, gfx3dSide_t side) {
    u8** buf;
    if (screen == GFX_TOP)
        buf = gfxTopLeftFramebuffers;
    else
        buf = gfxBottomFramebuffers;
    return buf[gfxBuffer];
}

u8* gfxGetInactiveFramebuffer(gfxScreen_t screen, gfx3dSide_t side) {
    u8** buf;
    if (screen == GFX_TOP)
        buf = gfxTopLeftFramebuffers;
    else
        buf = gfxBottomFramebuffers;
    return buf[!gfxBuffer];
}

void gfxMySwapBuffers() {
    gfxBuffer ^= 1;
    gfxSwapBuffers();
}

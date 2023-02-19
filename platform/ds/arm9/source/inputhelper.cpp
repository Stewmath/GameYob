extern "C" {
#include "libfat/cache.h"
#include "libfat/partition.h"
}

#include <nds.h>
#include <fat.h>
#include "common.h"

#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>

#include "inputhelper.h"
#include "gameboy.h"
#include "main.h"
#include "console.h"
#include "menu.h"
#include "nifi.h"
#include "gbgfx.h"
#include "soundengine.h"
#include "cheats.h"
#include "gbs.h"
#include "filechooser.h"
#include "romfile.h"
#include "gbmanager.h"
#include "camera.h"

int keysPressed=0;
int lastKeysPressed=0;
int keysForceReleased=0;
int repeatStartTimer=0;
int repeatTimer=0;

u8 buttonsPressed;

bool fastForwardMode = false; // controlled by the toggle hotkey
bool fastForwardKey = false;  // only while its hotkey is pressed

bool biosExists = false;
int rumbleInserted = 0;
int camActive = 0;
bool camInit = false;
u16* camData;

touchPosition touchData;

void initInput()
{
    fatInit(FAT_CACHE_SIZE, true);
    //fatInitDefault();
}

void flushFatCache() {
    // This involves things from libfat which aren't normally visible
    devoptab_t* devops = (devoptab_t*)GetDeviceOpTab ("sd");
    PARTITION* partition = (PARTITION*)devops->deviceData;
    _FAT_cache_flush(partition->cache); // Flush the cache manually
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

void forceReleaseKey(int key) {
    keysForceReleased |= key;
    keysPressed &= ~key;
}

int readKeysLastFrameCounter=0;
void inputUpdateVBlank() {
    scanKeys();
    touchRead(&touchData);

    lastKeysPressed = keysPressed;
    keysPressed = keysHeld();
    for (int i=0; i<16; i++) {
        if (keysForceReleased & (1<<i)) {
            if (!(keysPressed & (1<<i)))
                keysForceReleased &= ~(1<<i);
        }
    }
    keysPressed &= ~keysForceReleased;

    if (dsFrameCounter != readKeysLastFrameCounter) { // Double-check that it's been 1/60th of a second
        if (repeatStartTimer > 0)
            repeatStartTimer--;
        if (repeatTimer > 0)
            repeatTimer--;
        readKeysLastFrameCounter = dsFrameCounter;
    }
}

void system_doRumble(bool rumbleVal)
{
    if (rumbleInserted == 1)
    {
        setRumble(rumbleVal);
    }
    else if (rumbleInserted == 2)
    {
        GBA_BUS[0x1FE0000/2] = 0xd200;
        GBA_BUS[0x0000000/2] = 0x1500;
        GBA_BUS[0x0020000/2] = 0xd200;
        GBA_BUS[0x0040000/2] = 0x1500;
        GBA_BUS[0x1E20000/2] = rumbleVal ? (0xF0 + rumbleStrength) : 0x08;
        GBA_BUS[0x1FC0000/2] = 0x1500;
    }
}

#define MOTION_SENSOR_RANGE 128

int system_getMotionSensorX() {
    int px = touchData.px;
    if (!keyPressed(KEY_TOUCH))
        px = 128;

    double val = (128 - px) * ((double)MOTION_SENSOR_RANGE / 256)
        + MOTION_SENSOR_MID;
    /*
    if (val < 0)
        return (-(int)val) | 0x8000;
        */
    return (int)val + 0x8000;
}

int system_getMotionSensorY() {
    int py = touchData.py;
    if (!keyPressed(KEY_TOUCH))
        py = 96;

    double val = (96 - py) * ((double)MOTION_SENSOR_RANGE / 192)
        + MOTION_SENSOR_MID - 80;
    return (int)val;
}

void system_enableCamera(int index) {
    if (index == camActive) return;

    if (!camInit) {
        cameraInit();
        camData = (u16*)malloc(CAM_BUFFER_SIZE);
        camInit = true;
    }

    switch (index) {
        case 1:
            cameraActivate(CAM_INNER);
            break;
        case 2:
            cameraActivate(CAM_OUTER);
            break;
    }
    camActive = index;
}

void system_disableCamera(void) {
    cameraDeactivateAny();
    free(camData);
    camInit = false;
}

// The actual sensor image is 128x126 or so.
#define GBCAM_SENSOR_EXTRA_LINES (8)
#define GBCAM_SENSOR_W (128)
#define GBCAM_SENSOR_H (112+GBCAM_SENSOR_EXTRA_LINES) 

#define GBCAM_W (128)
#define GBCAM_H (112)

// Image processed by sensor chip
static s16 gb_cam_retina_output_buf[GBCAM_SENSOR_W][GBCAM_SENSOR_H];
static s16 temp_buf[GBCAM_SENSOR_W][GBCAM_SENSOR_H]; 

//--------------------------------------------------------------------

static s16 gb_clamp_int(s16 min, s16 value, s16 max)
{
    if(value < min) return min;
    if(value > max) return max;
    return value;
}

static s16 gb_min_int(s16 a, s16 b) { return (a < b) ? a : b; }

static s16 gb_max_int(s16 a, s16 b) { return (a > b) ? a : b; }

//--------------------------------------------------------------------

static u8 gb_cam_matrix_process(u8 value, u8 x, u8 y, const u8* CAM_REG)
{
    u16 base = 6 + ((y&3)*4 + (x&3)) * 3;

    u8 r0 = CAM_REG[base+0];
    u8 r1 = CAM_REG[base+1];
    u8 r2 = CAM_REG[base+2];

    if(value < r0) return 3;
    else if(value < r1) return 2;
    else if(value < r2) return 1;
    return 0;
}

void system_getCamera(u8* memory, const u8* camRegisters)
{
    if (camInit) {
        if(!cameraTransferActive()) {
            cameraTransferStart(camData, CAPTURE_MODE_PREVIEW);

            while(cameraTransferActive())
                system_waitForVBlank();

            cameraTransferStop();
        }

        const u8* CAM_REG = camRegisters;
        
        // Register 0
        u8 P_bits = 0;
        u8 M_bits = 0;

        switch( (CAM_REG[0]>>1)&3 )
        {
            case 0: P_bits = 0x00; M_bits = 0x01; break;
            case 1: P_bits = 0x01; M_bits = 0x00; break;
            case 2: case 3: P_bits = 0x01; M_bits = 0x02; break;
            default: break;
        }

        // Register 1
        u8 N_bit = (CAM_REG[1] & BIT(7)) >> 7;
        u8 VH_bits = (CAM_REG[1] & (BIT(6)|BIT(5))) >> 5;

        // Registers 2 and 3
        u16 EXPOSURE_bits = CAM_REG[3] | (CAM_REG[2]<<8);

        // Register 4
        const float edge_ratio_lut[8] = { 0.50, 0.75, 1.00, 1.25, 2.00, 3.00, 4.00, 5.00 };

        float EDGE_alpha = edge_ratio_lut[(CAM_REG[4] & 0x70)>>4];

        u8 E3_bit = (CAM_REG[4] & BIT(7)) >> 7;
        u8 I_bit = (CAM_REG[4] & BIT(3)) >> 3;

        //------------------------------------------------

        // Sensor handling
        // ---------------

        u8 i, j;

        //Copy webcam buffer to sensor buffer applying color correction and exposure time
        for(i = 0; i < GBCAM_SENSOR_W; i++) for(j = 0; j < GBCAM_SENSOR_H; j++)
        {
            u8 x = (i * 1.6);
            u8 y = (j * 1.6);
            s16 value = (camData[(y*256) + (x+16)] & 0xff);
            value = ( (value * EXPOSURE_bits ) / 0x0300 ); // 0x0300 could be other values
            value = 128 + (((value-128) * 1)/8); // "adapt" to "3.1"/5.0 V
            gb_cam_retina_output_buf[i][j] = gb_clamp_int(0,value,255);
        }

        if(I_bit) // Invert image
        {
            for(i = 0; i < GBCAM_SENSOR_W; i++) for(j = 0; j < GBCAM_SENSOR_H; j++)
            {
                gb_cam_retina_output_buf[i][j] = 255-gb_cam_retina_output_buf[i][j];
            }
        }

        // Make signed
        for(i = 0; i < GBCAM_SENSOR_W; i++) for(j = 0; j < GBCAM_SENSOR_H; j++)
        {
            gb_cam_retina_output_buf[i][j] = gb_cam_retina_output_buf[i][j]-128;
        }

        s16 px, mn, ms, mw, me;

        u8 filtering_mode = (N_bit<<3) | (VH_bits<<1) | E3_bit;
        
        switch(filtering_mode)
        {
            case 0x0: // 1-D filtering
            {
                for(i = 0; i < GBCAM_SENSOR_W; i++) for(j = 0; j < GBCAM_SENSOR_H; j++)
                {
                    temp_buf[i][j] = gb_cam_retina_output_buf[i][j];
                }
                for(i = 0; i < GBCAM_SENSOR_W; i++) for(j = 0; j < GBCAM_SENSOR_H; j++)
                {
                    ms = temp_buf[i][gb_min_int(j+1,GBCAM_SENSOR_H-1)];
                    px = temp_buf[i][j];

                    int value = 0;
                    if(P_bits&BIT(0)) value += px;
                    if(P_bits&BIT(1)) value += ms;
                    if(M_bits&BIT(0)) value -= px;
                    if(M_bits&BIT(1)) value -= ms;
                    gb_cam_retina_output_buf[i][j] = gb_clamp_int(-128,value,127);
                }
                break;
            }
            case 0x2: //1-D filtering + Horiz. enhancement : P + {2P-(MW+ME)} * alpha
            {
                for(i = 0; i < GBCAM_SENSOR_W; i++) for(j = 0; j < GBCAM_SENSOR_H; j++)
                {
                    mw = gb_cam_retina_output_buf[gb_max_int(0,i-1)][j];
                    me = gb_cam_retina_output_buf[gb_min_int(i+1,GBCAM_SENSOR_W-1)][j];
                    px = gb_cam_retina_output_buf[i][j];

                    temp_buf[i][j] = gb_clamp_int(0,px+((2*px-mw-me)*EDGE_alpha),255);
                }
                for(i = 0; i < GBCAM_SENSOR_W; i++) for(j = 0; j < GBCAM_SENSOR_H; j++)
                {
                    ms = temp_buf[i][gb_min_int(j+1,GBCAM_SENSOR_H-1)];
                    px = temp_buf[i][j];

                    int value = 0;
                    if(P_bits&BIT(0)) value += px;
                    if(P_bits&BIT(1)) value += ms;
                    if(M_bits&BIT(0)) value -= px;
                    if(M_bits&BIT(1)) value -= ms;
                    gb_cam_retina_output_buf[i][j] = gb_clamp_int(-128,value,127);
                }
                break;
            }
            case 0xE: //2D enhancement : P + {4P-(MN+MS+ME+MW)} * alpha
            {
                for(i = 0; i < GBCAM_SENSOR_W; i++) for(j = 0; j < GBCAM_SENSOR_H; j++)
                {
                    ms = gb_cam_retina_output_buf[i][gb_min_int(j+1,GBCAM_SENSOR_H-1)];
                    mn = gb_cam_retina_output_buf[i][gb_max_int(0,j-1)];
                    mw = gb_cam_retina_output_buf[gb_max_int(0,i-1)][j];
                    me = gb_cam_retina_output_buf[gb_min_int(i+1,GBCAM_SENSOR_W-1)][j];
                    px = gb_cam_retina_output_buf[i][j];

                    temp_buf[i][j] = gb_clamp_int(-128,px+((4*px-mw-me-mn-ms)*EDGE_alpha),127);
                }
                /*for(i = 0; i < GBCAM_SENSOR_W; i++) for(j = 0; j < GBCAM_SENSOR_H; j++)
                {
                    gb_cam_retina_output_buf[i][j] = temp_buf[i][j];
                }*/
                memcpy(gb_cam_retina_output_buf, temp_buf, sizeof(gb_cam_retina_output_buf));
                break;
            }
            case 0x1:
            {
                // In my GB Camera cartridge this is always the same color. The datasheet of the
                // sensor doesn't have this configuration documented. Maybe this is a bug?
                for(i = 0; i < GBCAM_SENSOR_W; i++) for(j = 0; j < GBCAM_SENSOR_H; j++)
                {
                    gb_cam_retina_output_buf[i][j] = 0;
                }
                break;
            }
            default:
            {
                // Ignore filtering
                break;
            }
        }
    
        // Make unsigned
        for(i = 0; i < GBCAM_SENSOR_W; i++) for(j = 0; j < GBCAM_SENSOR_H; j++)
        {
            gb_cam_retina_output_buf[i][j] = gb_clamp_int(0,gb_cam_retina_output_buf[i][j]+128,255);
        }

        // Convert to tiles
        for(i = 0; i < GBCAM_W; i++) for(j = 0; j < GBCAM_H; j++)
        {
            // Convert to Game Boy colors using the controller matrix
            u8 outcolor = gb_cam_matrix_process(
                gb_cam_retina_output_buf[i][j+(GBCAM_SENSOR_EXTRA_LINES/2)],i,j,CAM_REG) & 3;

            u16 coord = (((i >> 3) & 0xF) * 8 + (j & 0x7)) * 2 + (j & ~0x7) * 0x20;
            u8 * tile_base = memory+coord;

            if(outcolor & 1) tile_base[0] |= 1<<(7-(7&i));
            if(outcolor & 2) tile_base[1] |= 1<<(7-(7&i));
        }
    }
}

void system_checkPolls() {

}

void system_waitForVBlank() {
    swiWaitForVBlank();
}

void system_cleanup() {
    mgr_save();
    mgr_exit();
}

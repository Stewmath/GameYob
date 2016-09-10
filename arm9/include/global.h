#pragma once

// Interrupt definitions (used with requestInterrupt())
#define VBLANK		1
#define LCD			2
#define TIMER		4
#define SERIAL      8
#define JOYPAD		0x10

// u8/u16/u32 definitions
#include <nds/ndstypes.h>

extern const int clockSpeed; 

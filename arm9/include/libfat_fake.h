#pragma once

#include <sys/iosupport.h>

// This file declares a few things from libfat that aren't supposed to be 
// visible. I need to manually flush the cache for autosaving.


struct CACHE;

typedef struct {
    const int* filler;
	CACHE* cache;
} PARTITION;
// PARTITION has more entries but only cache is important for our purposes

extern "C" bool _FAT_cache_flush (CACHE* cache);

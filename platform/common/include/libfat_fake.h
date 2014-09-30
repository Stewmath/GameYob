#pragma once

#include <sys/iosupport.h>
#include <nds/disc_io.h>

// This file declares a few things from libfat that aren't supposed to be 
// visible. I need to manually flush the cache for autosaving.


typedef struct {
	sec_t        sector;
	unsigned int count;
	unsigned int last_access;
	bool         dirty;
	uint8_t*     cache;
} CACHE_ENTRY;

typedef struct {
	const DISC_INTERFACE* disc;
	sec_t		          endOfPartition;
	unsigned int          numberOfPages;
	unsigned int          sectorsPerPage;
	unsigned int          bytesPerSector;
	CACHE_ENTRY*          cacheEntries;
} CACHE;

typedef struct {
    const int* filler;
	CACHE* cache;
} PARTITION;
// PARTITION has more entries but only cache is important for our purposes


extern int fat_bytesPerSector;
extern int fat_sectorsPerPage;

extern "C" bool _FAT_cache_flush (CACHE* cache);
static inline bool _FAT_disc_writeSectors (const DISC_INTERFACE* disc, sec_t sector, sec_t numSectors, const void* buffer) {
	return disc->writeSectors (sector, numSectors, buffer);
}

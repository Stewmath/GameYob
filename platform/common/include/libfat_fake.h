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
	sec_t    fatStart;
	uint32_t sectorsPerFat;
	uint32_t lastCluster;
	uint32_t firstFree;
	uint32_t numberFreeCluster;
	uint32_t numberLastAllocCluster;
} FAT;

typedef struct {
	const DISC_INTERFACE* disc;
	CACHE*                cache;
	// Info about the partition
	FS_TYPE               filesysType;
	uint64_t              totalSize;
	sec_t                 rootDirStart;
	uint32_t              rootDirCluster;
	uint32_t              numberOfSectors;
	sec_t                 dataStart;
	uint32_t              bytesPerSector;
	uint32_t              sectorsPerCluster;
	uint32_t              bytesPerCluster;
	uint32_t              fsInfoSector;
	FAT                   fat;
	// Values that may change after construction
	uint32_t              cwdCluster;			// Current working directory cluster
	int                   openFileCount;
	struct _FILE_STRUCT*  firstOpenFile;		// The start of a linked list of files
	mutex_t               lock;					// A lock for partition operations
	bool                  readOnly;				// If this is set, then do not try writing to the disc
	char                  label[12];			// Volume label
} PARTITION;
// PARTITION has more entries but only cache is important for our purposes


extern int fat_bytesPerSector;
extern int fat_sectorsPerPage;

extern "C" bool _FAT_cache_flush (CACHE* cache);
extern "C" void _FAT_cache_invalidate (CACHE* cache);

extern "C" bool _FAT_cache_readPartialSector (CACHE* cache, void* buffer, sec_t sector, unsigned int offset, size_t size);

static inline bool _FAT_disc_readSectors (const DISC_INTERFACE* disc, sec_t sector, sec_t numSectors, void* buffer) {
	return disc->readSectors (sector, numSectors, buffer);
}

static inline bool _FAT_disc_writeSectors (const DISC_INTERFACE* disc, sec_t sector, sec_t numSectors, const void* buffer) {
	return disc->writeSectors (sector, numSectors, buffer);
}

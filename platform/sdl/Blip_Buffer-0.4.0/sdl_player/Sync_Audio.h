
// Simple synchronous sound interface for SDL multimedia library

#ifndef SYNC_AUDIO_H
#define SYNC_AUDIO_H

#include "SDL.h"

// Simple SDL sound wrapper that has a synchronous interface
class Sync_Audio {
	enum { gain_bits = 16 };
public:
	// Initialize with specified sample rate, channel count, and latency.
	// Returns NULL on success, otherwise error string.
	const char* start( long sample_rate, int chan_count = 1, int latency_msec = 200 );
	
	// Set gain, where 1.0 leaves sound unaltered
	void set_gain( double g ) { gain = (long) (g * (1L << gain_bits)); }
	
	// Number of samples in buffer waiting to be played
	int sample_count() const;
	
	// Write samples to buffer, first waiting until enough space is available
	typedef short sample_t;
	void write( const sample_t*, int count );
	
	// Stop audio output
	void stop();
	
	Sync_Audio();
	~Sync_Audio();
	
private:
	enum { buf_size = 1024 };
	sample_t* volatile bufs;
	SDL_sem* volatile free_sem;
	int volatile buf_count;
	int volatile read_buf;
	int write_buf;
	int write_pos;
	int latency;
	long gain;
	int sound_open;
	
	sample_t* buf( int index );
	void fill_buffer( Uint8*, int );
	static void fill_buffer_( void*, Uint8*, int );
};

#endif


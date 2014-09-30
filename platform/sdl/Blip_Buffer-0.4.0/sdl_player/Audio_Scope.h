
// Simple audio waveform scope in a window, using SDL multimedia library

#ifndef AUDIO_SCOPE_H
#define AUDIO_SCOPE_H

#include "SDL.h"

class Audio_Scope {
public:
	typedef const char* error_t;
	
	// Initialize scope window of specified size
	error_t init( int width, int height );
	
	// Draw at most 'count' samples from 'in', skipping 'step' samples after
	// each sample drawn. Step can be less than 1.0.
	error_t draw( const short* in, long count, double step = 1.0 );
	
	Audio_Scope();
	~Audio_Scope();
	
private:
	typedef unsigned char byte;
	SDL_Surface* screen;
	SDL_Surface* surface;
	byte* buf;
	int buf_size;
	int sample_shift;
	int low_y;
	int high_y;
	void render( short const* in, long count, long step );
};

#endif


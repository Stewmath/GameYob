
// Framework for using Sync_Audio with SDL for sound output
// (remove player.cpp since it will conflict with this)

#include "Blip_Buffer.h"
#include "sdl_player/Sync_Audio.h"
#include <stdlib.h>

const long sample_rate = 44100; // 44.1 kHz sample rate
const long clock_rate = 1000000;// 1 MHz clock rate
const int frame_rate = 60;      // 60 frames of sound per second

static Sync_Audio audio;
static Blip_Buffer buf;
static Blip_Synth<blip_good_quality,20> synth;

// temporary buffer to read samples into
const int buf_size = 10000;
static blip_sample_t samples [buf_size];

int init_audio()
{
	// Initialize SDL
	if ( SDL_Init( SDL_INIT_AUDIO ) < 0 )
		return 1;
	atexit( SDL_Quit );
	
	// Setup buffer
	buf.clock_rate( clock_rate );
	if ( buf.set_sample_rate( sample_rate, 1000 / frame_rate ) )
		return 1; // out of memory
	
	// Setup synth
	synth.volume( 0.50 );
	synth.output( &buf );
	
	// Start audio
	if ( audio.start( sample_rate ) )
		return 1;
	
	return 0;
}

int main( int argc, char** argv )
{
	if ( init_audio() )
		return EXIT_FAILURE;
	
	// Generate sound and immediately play it
	long time = 0;
	int amplitude = 5;
	int period = 1000;
	for ( int n = frame_rate * 2; n--; )
	{
		period++; // slowly lower pitch
		
		// Fill buffer with 1/60 second of sound
		long length = clock_rate / frame_rate;
		while ( time < length )
		{
			amplitude = -amplitude;
			synth.update( time, amplitude );
			time += period;
		}
		buf.end_frame( length );
		time -= length;
		
		// Read and play samples
		long count = buf.read_samples( samples, buf_size );
		audio.write( samples, count );
	}
	
	audio.stop();
	
	return 0;
}


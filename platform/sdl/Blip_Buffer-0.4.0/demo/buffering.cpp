
// Shows three ways of buffering samples in a Blip_Buffer

#include "Blip_Buffer.h"

#include "player/player.h"

// Plays 'count' samples through computer speaker
void play_samples( const blip_sample_t* samples, long count );

// buffer to read samples into
const int buf_size = 10000;
static blip_sample_t samples [buf_size];

static Blip_Buffer buf;
static Blip_Synth<blip_low_quality,20> synth;

int main()
{
	setup_demo( buf, synth );
	int amplitude = 5;
	
	// Read samples from buffer immediately
	set_caption( "Immediate" );
	while ( !button_pressed() )
	{
		// Add square wave
		for ( int time = 0; time < 1000; time += 50, amplitude = -amplitude )
			synth.update( time, amplitude );
		buf.end_frame( 1000 );
		
		// Read samples from buffer and play them
		int count = buf.read_samples( samples, buf_size );
		play_samples( samples, count );
	}
	
	// Wait for samples to accumulate before reading
	set_caption( "Accumulate" );
	while ( !button_pressed() )
	{
		// Add square wave
		for ( int time = 0; time < 1000; time += 50, amplitude = -amplitude )
			synth.update( time, amplitude );
		buf.end_frame( 1000 );
		
		// Wait for 4000 samples to accumulate before playing them
		if ( buf.samples_avail() >= 4000 )
		{
			int count = buf.read_samples( samples, 4000 );
			play_samples( samples, count );
		}
	}
	
	// Generate samples on demand
	set_caption( "On Demand" );
	while ( !button_pressed() )
	{
		// We want this many samples
		int samples_needed = 2000;
		
		// Determine how many clocks until sufficient samples will be generated
		int length = buf.count_clocks( samples_needed );
		for ( int time = 0; time < length; time += 50, amplitude = -amplitude )
			synth.update( time, amplitude );
		buf.end_frame( length );
		
		// Now the needed samples should be available
		buf.read_samples( samples, samples_needed );
		play_samples( samples, samples_needed );
	}
	
	return 0;
}


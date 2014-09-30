
// How to generate a continuous square wave and read the samples

#include "Blip_Buffer.h"

#include "player/player.h"

// Plays 'count' samples through computer speaker
void play_samples( const blip_sample_t* samples, long count );

static Blip_Buffer buf;
static Blip_Synth<blip_good_quality,20> synth;

int main()
{
	// Setup buffer
	buf.clock_rate( 44100 );
	if ( buf.set_sample_rate( 44100 ) )
		return 1; // out of memory
	
	// Setup synth
	synth.volume( 0.50 );
	synth.output( &buf );

	int time = 0;
	int sign = 1;
	
	while ( !button_pressed() )
	{
		// mouse sets frequency and volume
		int period = mouse_x() * 100 + 10;
		int volume = mouse_y() * 9 + 1;
		
		// Generate 1000 time units of square wave
		int length = 1000;
		while ( time < length )
		{
			sign = -sign;
			synth.update( time, sign * volume );
			time += period;
		}
		buf.end_frame( length );
		time -= length; // adjust time to new frame
		
		// Read samples from buffer and play them
		blip_sample_t samples [1024];
		int count = buf.read_samples( samples, 1024 );
		play_samples( samples, count );
	}
	
	return 0;
}


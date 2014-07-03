
// Generates a sine wave and mixes that into a Blip_Buffer

#include "Blip_Buffer.h"

#include "player/player.h"
#include <math.h>

static Blip_Buffer buf;
static Blip_Synth<blip_good_quality,20> synth;

int main()
{
	setup_demo( buf, synth );
	
	// generate 1000 clocks of square wave
	int length = 1000;
	int amplitude = 1;
	for ( int time = 0; time < length; time += 10 )
	{
		synth.update( time, amplitude );
		amplitude = -amplitude;
	}
	
	// find out how many samples of sine wave to generate
	int count = buf.count_samples( length );
	blip_sample_t temp [4096];
	for ( int i = 0; i < count; i++ )
	{
		double y = sin( i * (3.14159 / 100) );
		temp [i] = y * 0.30 * blip_sample_max; // convert to blip_sample_t's range
	}
	
	// mix sine wave's samples into Blip_Buffer
	buf.mix_samples( temp, count );
	
	// end frame and show samples
	buf.end_frame( length );
	show_buffer( buf );
	wait_button();
	 
	return 0;
}


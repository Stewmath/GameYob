
// Multiple waves can be added into buffer

#include "Blip_Buffer.h"

#include "player/player.h"

static Blip_Buffer buf;
static Blip_Synth<blip_good_quality,20> synth;
static Blip_Synth<blip_good_quality,20> synth2;

int main()
{
	// Setup buffer
	buf.bass_freq( 0 );
	buf.clock_rate( 44100 );
	if ( buf.set_sample_rate( 44100 ) )
		return 1; // out of memory
	
	// Setup waves
	synth.volume( 0.50 );
	synth.output( &buf );
	
	synth2.volume( 0.50 );
	synth2.output( &buf );
	
	int time;
	
	// Add first low-frequency square wave
	int amplitude = 5;
	for ( time = 0; time < 1000; time += 100, amplitude = -amplitude )
		synth.update( time, amplitude );
	
	// Add second high-frequency square wave
	amplitude = 2;
	for ( time = 0; time < 1000; time += 15, amplitude = -amplitude )
		synth2.update( time, amplitude );
	
	// Display result
	buf.end_frame( 1000 );
	show_buffer( buf );
	wait_button();
	 
	return 0;
}


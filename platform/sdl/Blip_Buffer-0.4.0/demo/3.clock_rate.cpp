
// How the clock rate and sample rate work

#include "Blip_Buffer.h"

#include "player/player.h"
#include <stdlib.h>

static Blip_Buffer buf;
static Blip_Synth<blip_low_quality,20> synth;

int main()
{
	long sample_rate = 44100;
	
	// Sample rate sets how many samples are generated per second
	if ( buf.set_sample_rate( sample_rate ) )
		return 1; // out of memory
	buf.bass_freq( 0 ); // keep waveforms perfectly flat
	
	// Setup synth
	synth.output( &buf );
	synth.volume( 0.50 );
	
	while ( !button_pressed() )
	{
		// Mouse sets clock rate, higher to the right. The higher the clock
		// rate, the more packed the waveform becomes.
		long rate = sample_rate * (mouse_x() * 10 + 1);
		
		// Clock rate sets how many time units there are per second
		buf.clock_rate( rate );
		
		// Generate random waveform, with each transition spaced 50 clocks apart.
		srand( 1 );
		buf.clear();
		for ( int time = 0; time < 500; time += 50 )
			synth.update( time, rand() % 20 - 10 );
		
		buf.end_frame( 600 );
		
		show_buffer_unscaled( buf );
	}
	
	return 0;
}


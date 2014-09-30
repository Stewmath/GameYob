
// How to make a square wave

#include "Blip_Buffer.h"

#include "player/player.h"

static Blip_Buffer buf;
static Blip_Synth<blip_low_quality,20> synth;

int main()
{
	while ( !button_pressed() )
	{
		setup_demo( buf, synth );
		
		// base frequency and amplitude on mouse position
		int period = mouse_x() * 100 + 10;
		int amplitude = mouse_y() * 9 + 1;
		
		// generate alternating signs of square wave, spaced by period
		int time = 0;
		while ( time < 1000 )
		{
			amplitude = -amplitude;
			synth.update( time, amplitude );
			time += period;
		}
		buf.end_frame( 1000 );
		
		show_buffer( buf );
	}
	
	return 0;
}


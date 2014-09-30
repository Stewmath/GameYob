
// How to use low-level delta mode of Blip_Synth

#include "Blip_Buffer.h"

#include "player/player.h"

static Blip_Buffer buf;
static Blip_Synth<blip_low_quality,20> synth;

int main()
{
	// Setup buffer and synth
	setup_demo( buf, synth );
	
	// 10                  ___
	//  5      ___        |   |
	//  0 ____|   |___    |   |________
	// -5             |   |
	//-10             |___|
	//    0  100 200 300 400 500 600 700
	
	// Generate the above waveform by specifying the *changes*
	// in amplitude at each point.
	synth.offset( 100,  +5 );
	synth.offset( 200,  -5 );
	synth.offset( 300, -10 );
	synth.offset( 400, +20 );
	synth.offset( 500, -10 );
	buf.end_frame( 700 );
	
	// Display waveform
	show_buffer( buf );
	set_caption( "Waveform" );
	wait_button();
	
	
	// When used like this, Blip_Synth doesn't keep track of the
	// waveform's current amplitude, so the offsets can be made
	// in any order (and thus used to add multiple waveforms):
	synth.offset( 500, -10 );
	synth.offset( 100,  +5 );
	synth.offset( 300, -10 );
	synth.offset( 400, +20 );
	synth.offset( 200,  -5 );
	buf.end_frame( 700 );
	
	// Display waveform
	show_buffer( buf );
	set_caption( "Random order" );
	wait_button();
	
	return 0;
}



// How to generate a simple waveform

#include "Blip_Buffer.h"

#include "player/player.h"

static Blip_Buffer buf;
static Blip_Synth<blip_low_quality,20> synth;

int main()
{
	// Setup buffer and synth (explained in later demo)
	setup_demo( buf, synth );
	
	// 10                  ___
	//  5      ___        |   |
	//  0 ____|   |___    |   |________
	// -5             |   |
	//-10             |___|
	//    0  100 200 300 400 500 600 700
	
	// Generate the above waveform by updating the waveform's
	// amplitude at each time point where it changes
	synth.update( 100,   5 );
	synth.update( 200,   0 );
	synth.update( 300, -10 );
	synth.update( 400,  10 );
	synth.update( 500,   0 );
	buf.end_frame( 700 );
	
	// Display waveform
	show_buffer( buf );
	set_caption( "Waveform" );
	wait_button();
	
	
	// All times are relative to the beginning of the current "time frame".
	// This generates the same waveform, split by a time frame:
	synth.update( 100,   5 );
	synth.update( 200,   0 );
	buf.end_frame( 200 ); // what was time 200 is now time 0
	synth.update( 100, -10 );
	synth.update( 200,  10 );
	synth.update( 300,   0 );
	buf.end_frame( 500 );
	
	// Display waveform
	show_buffer( buf );
	set_caption( "Time Frames" );
	wait_button();
	
	return 0;
}


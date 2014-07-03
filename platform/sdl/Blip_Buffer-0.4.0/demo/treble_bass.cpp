
// How treble and bass affect waveform

#include "Blip_Buffer.h"

#include "player/player.h"

// buffer to read samples into
const int buf_size = 10000;
blip_sample_t samples [buf_size];

static Blip_Buffer buf;
static Blip_Synth<blip_high_quality,20> synth;

int main()
{
	setup_demo( buf, synth );
	
	// Sweep treble from -200 to 5.0
	set_caption( "Treble" );
	for ( double treble = -200.0; treble < 5.0; treble += (6.0 - treble) / 30.0 )
	{
		synth.treble_eq( treble );
		
		// square wave
		for ( int i = 0; i < 10; i++ )
			synth.update( i * 100, (i & 1) * 20 - 10 );
		buf.end_frame( 1000 );
		
		// read and play samples
		int count = buf.read_samples( samples, buf_size );
		play_samples( samples, count );
	}
	synth.treble_eq( -8 );
	
	// Sweep bass from 1 to 10000
	set_caption( "Bass" );
	for ( double bass = 1; bass < 22000; bass = bass * 1.03 + 0.6 )
	{
		buf.bass_freq( (int) bass );
		
		// square wave
		for ( int i = 0; i < 10; i++ )
			synth.update( i * 100, (i & 1) * 20 - 10 );
		buf.end_frame( 1000 );
		
		// read and play samples
		int count = buf.read_samples( samples, buf_size );
		play_samples( samples, count );
	}
	return 0;
}


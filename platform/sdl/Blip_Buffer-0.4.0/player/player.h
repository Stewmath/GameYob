
// Common functions used by demos

#ifndef DEMO_PLAYER_H
#define DEMO_PLAYER_H

#include <stdlib.h>

// Since SDL pulls shenanigans with redefining main(), so do we
int demo_main();
#ifndef main
	#define main demo_main
#endif

#include "../Blip_Buffer.h"

// Set window title
void set_caption( const char* );

// True if mouse or keyboard was pressed since last call
int button_pressed();

// Wait until mouse or keyboard is pressed
void wait_button();

// Current mouse X position, from 0.0 to 1.0
double mouse_x();

// Current mouse Y position, from 0.0 to 1.0
double mouse_y();

// Set wave to output to buffer
template<class Wave>
inline void setup_demo( Blip_Buffer& buf, Wave& wave )
{
	// use one-second buffer
	if ( buf.set_sample_rate( 44100, 1000 ) )
		exit( EXIT_FAILURE );
	buf.clock_rate( buf.sample_rate() );
	buf.bass_freq( 0 ); // makes waveforms perfectly flat
	wave.volume( 0.50 );
	wave.output( &buf );
}

// Play and display samples
void play_samples( const short* samples, long count );
void play_stereo_samples( const short* samples, long count );

// Read samples from Blip_Buffer and display them, optionally without any scaling
// to fit them all in the waveform window
void show_buffer( Blip_Buffer& );
void show_buffer_unscaled( Blip_Buffer& );

#endif


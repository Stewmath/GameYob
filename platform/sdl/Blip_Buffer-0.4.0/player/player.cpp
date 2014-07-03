
// For use when SDL isn't available. Outputs all sound to wave file

// Blip_Buffer 0.4.0. http://www.slack.net/~ant/

#define main main
#include "player.h"

#include <stdlib.h>

#include "../Blip_Buffer.h"
#include "Wave_Writer.h"

/* Copyright (C) 2005 by Shay Green. Permission is hereby granted, free of
charge, to any person obtaining a copy of this software module and associated
documentation files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use, copy, modify,
merge, publish, distribute, sublicense, and/or sell copies of the Software, and
to permit persons to whom the Software is furnished to do so, subject to the
following conditions: The above copyright notice and this permission notice
shall be included in all copies or substantial portions of the Software. THE
SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

long const sample_rate = 44100;

int const poll_max = 20;
static int poll_count = 0;
static long sample_count = 0;

int main()
{
	return demo_main();
}

void set_caption( const char* )
{
	poll_count = 0;
	sample_count = 0;
}

int button_pressed()
{
	poll_count++;
	if ( poll_count > poll_max * 100 )
		return 1;
	return poll_count > poll_max && sample_count > sample_rate * 2;
}

void wait_button() { }

double mouse_x() { return 1.0 / poll_max * (poll_count % poll_max); }

double mouse_y() { return 1.0 / poll_max * (poll_count % poll_max); }

void play_samples( const short* samples, long count )
{
	static Wave_Writer wave( sample_rate );
	sample_count += count;
	wave.write( samples, count );
}

void play_stereo_samples( const short* samples, long count )
{
	static Wave_Writer wave( sample_rate );
	wave.stereo( 1 );
	sample_count += count / 2;
	wave.write( samples, count );
}

void show_buffer( Blip_Buffer& in )
{
	int const buf_size = 512;
	blip_sample_t buf [buf_size];
	int count;
	while ( (count = in.read_samples( buf, buf_size )) > 0 )
		play_samples( buf, count );
}

void show_buffer_unscaled( Blip_Buffer& in )
{
	int const buf_size = 512;
	blip_sample_t buf [buf_size];
	for ( int i = 0; i < buf_size; i++ )
		buf [i] = 0;
	in.read_samples( buf, buf_size );
	play_samples( buf, buf_size );
}


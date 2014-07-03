
// Blip_Buffer 0.4.0. http://www.slack.net/~ant/

#include "Sync_Audio.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

/* Copyright (C) 2005-2006 by Shay Green. Permission is hereby granted, free of
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

// Return current SDL_GetError() string, or str if SDL didn't have a string
static const char* sdl_error( const char* str )
{
	const char* sdl_str = SDL_GetError();
	if ( sdl_str && *sdl_str )
		str = sdl_str;
	return str;
}

Sync_Audio::Sync_Audio()
{
	buf_count = 2;
	bufs = 0;
	free_sem = 0;
	sound_open = 0;
	set_gain( 1.0 );
}

Sync_Audio::~Sync_Audio()
{
	stop();
}

const char* Sync_Audio::start( long sample_rate, int chan_count, int latency )
{
	stop();
	
	write_buf = 0;
	write_pos = 0;
	read_buf = 0;
	
	long sample_latency = latency * sample_rate * chan_count / 1000;
	buf_count = sample_latency / buf_size;
	if ( buf_count < 2 )
		buf_count = 2;
	
	bufs = (sample_t*) malloc( (long) buf_size * buf_count * sizeof *bufs );
	if ( !bufs )
		return "Out of memory";
	
	free_sem = SDL_CreateSemaphore( buf_count - 1 );
	if ( !free_sem )
		return sdl_error( "Couldn't create semaphore" );
	
	SDL_AudioSpec as;
	as.freq = sample_rate;
	as.format = AUDIO_S16SYS;
	as.channels = chan_count;
	as.silence = 0;
	as.samples = buf_size / chan_count;
	as.size = 0;
	as.callback = fill_buffer_;
	as.userdata = this;
	if ( SDL_OpenAudio( &as, 0 ) < 0 )
		return sdl_error( "Couldn't open SDL audio" );
	SDL_PauseAudio( 0 );
	sound_open = 1;
	
	return 0; // success
}

void Sync_Audio::stop()
{
	if ( sound_open )
	{
		sound_open = 0;
		SDL_PauseAudio( 1 );
		SDL_CloseAudio();
	}
	
	if ( free_sem )
	{
		SDL_DestroySemaphore( free_sem );
		free_sem = 0;
	}
	
	free( bufs );
	bufs = 0;
}

int Sync_Audio::sample_count() const
{
	if ( !free_sem )
		return 0;
	
	int buf_free = SDL_SemValue( free_sem ) * buf_size + (buf_size - write_pos);
	return buf_size * buf_count - buf_free;
}

inline Sync_Audio::sample_t* Sync_Audio::buf( int index )
{
	assert( (unsigned) index < buf_count );
	return bufs + (long) index * buf_size;
}

void Sync_Audio::write( const sample_t* in, int remain )
{
	while ( remain )
	{
		int count = buf_size - write_pos;
		if ( count > remain )
			count = remain;
		
		sample_t* out = buf( write_buf ) + write_pos;
		if ( gain != (1L << gain_bits) )
		{
			register long gain = this->gain;
			for ( int n = count; n--; )
				*out++ = (*in++ * gain) >> gain_bits;
		}
		else
		{
			memcpy( out, in, count * sizeof (sample_t) );
			in += count;
		}
		
		write_pos += count;
		remain -= count;
		
		if ( write_pos >= buf_size )
		{
			write_pos = 0;
			write_buf = (write_buf + 1) % buf_count;
			SDL_SemWait( free_sem );
		}
	}
}

void Sync_Audio::fill_buffer( Uint8* out, int byte_count )
{
	if ( SDL_SemValue( free_sem ) < buf_count - 1 )
	{
		memcpy( out, buf( read_buf ), byte_count );
		read_buf = (read_buf + 1) % buf_count;
		SDL_SemPost( free_sem );
	}
	else
	{
		memset( out, 0, byte_count );
	}
}

void Sync_Audio::fill_buffer_( void* user_data, Uint8* out, int byte_count )
{
	((Sync_Audio*) user_data)->fill_buffer( out, byte_count );
}


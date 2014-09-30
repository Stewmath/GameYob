
// Blip_Buffer 0.4.0. http://www.slack.net/~ant/

#define main main
#include "player.h"
#undef main

#include "Blip_Buffer.h"
#include "Audio_Scope.h"
#include "Sync_Audio.h"
#include <stdlib.h>

// Requires SDL multimedia library: http://www.libsdl.org/
#include "SDL.h"

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

long const sample_rate = 44100;

int const scope_width = 512;
int const scope_height = 256;

static Audio_Scope scope;
static Sync_Audio audio;
static int chan_count = 1;
static int button_pressed_;
static int audio_started;
int const buf_size = 64 * 1024L;
static blip_sample_t* sample_buf;

static void handle_error( const char* str )
{
	if ( str )
	{
		fprintf( stderr, "Error: %s\n", str );
		exit( EXIT_FAILURE );
	}
}

int playerInit()
{
	sample_buf = (blip_sample_t*) malloc( buf_size * sizeof *sample_buf );
	if ( !sample_buf )
		handle_error( "Out of memory" );
	
	// video must be initialized for events to work
	/*if ( SDL_Init( SDL_INIT_VIDEO | SDL_INIT_AUDIO ) < 0 )
		handle_error( "Couldn't initialize SDL multimedia library" );
	atexit( SDL_Quit );
	handle_error( scope.init( scope_width, scope_height ) );
	
	*/
	int result = 0;//demo_main();
	
	while ( audio.sample_count() > 1024 ) { }
	audio.stop();
	free( sample_buf );
	
	return result;
}

void set_caption( const char* str )
{
	SDL_WM_SetCaption( str, str );
}

static void poll_events()
{
	SDL_Event e;
	while ( SDL_PollEvent( &e ) )
	{
		if ( e.type == SDL_MOUSEBUTTONDOWN )
			button_pressed_ = 1;
		
		if ( e.type == SDL_KEYDOWN )
		{
			button_pressed_ = 1;
			if ( e.key.keysym.sym == SDLK_ESCAPE || e.key.keysym.sym == SDLK_q )
				e.type = SDL_QUIT;
		}
		
		if ( e.type == SDL_QUIT )
		{
			audio.stop();
			exit( EXIT_SUCCESS );
		}
	}
}

int button_pressed()
{
	if ( !button_pressed_ )
		poll_events();
	
	int result = button_pressed_;
	button_pressed_ = 0;
	return result;
}

void wait_button()
{
	while ( !button_pressed() ) { }
}

double mouse_x()
{
	int x;
	SDL_GetMouseState( &x, 0 );
	return (double) x / (scope_width - 1);
}

double mouse_y()
{
	int y;
	SDL_GetMouseState( 0, &y );
	return (double) y / (scope_height - 1);
}

static void start_audio( int cc = 1 )
{
	if ( !audio_started )
	{
		chan_count = cc;
		audio_started = 1;
		handle_error( audio.start( sample_rate, chan_count, 100 ) );
		audio.set_gain( 0.25 );
	}
}

void play_samples( const short* samples, long count )
{
	if ( count )
	{
		start_audio();
		//if ( audio.sample_count() >= 1024 )
		//	scope.draw( samples, scope_width + 1, chan_count ); // assumes count >= scope_width
		audio.write( samples, count );
	}
	
	//poll_events();
}

void play_stereo_samples( const short* samples, long count )
{
	start_audio( 2 );
	play_samples( samples, count );
}

void show_buffer( Blip_Buffer& in )
{
	long count = in.read_samples( sample_buf, buf_size );
	scope.draw( sample_buf, scope_width, (double) count / scope_width );
}

void show_buffer_unscaled( Blip_Buffer& in )
{
	blip_sample_t buf [scope_width];
	long count = in.read_samples( buf, scope_width );
	while ( count < scope_width )
		buf [count++] = 0;
	
	scope.draw( buf, scope_width );
	
	// remove remaining samples
	while ( in.read_samples( buf, scope_width ) ) { }
}


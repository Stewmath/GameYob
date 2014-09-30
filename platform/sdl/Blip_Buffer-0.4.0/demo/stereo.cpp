
// How to generate stereo sound using two Blip_Buffers

#include "Blip_Buffer.h"

#include "player/player.h"

static Blip_Buffer left;
static Blip_Buffer right;
static Blip_Synth<blip_good_quality,20> left_synth;
static Blip_Synth<blip_good_quality,20> right_synth;

int main()
{
	// Setup left buffer and wave
	int left_time = 0;
	int left_amp = 0;
	setup_demo( left, left_synth );
	left.clock_rate( left.sample_rate() * 100 );
	
	// Setup right buffer and wave
	int right_time = 0;
	int right_amp = 0;
	setup_demo( right, right_synth );
	right.clock_rate( right.sample_rate() * 100 );
	
	while ( !button_pressed() )
	{
		blip_time_t length = 100000;
		
		// mouse sets frequency of left wave
		int period = 1000 + 6 * mouse_x();
		
		// Add saw wave to left buffer
		do {
			left_synth.update( left_time, left_amp = (left_amp + 1) % 10 );
		} while ( (left_time += period) < length );
		left.end_frame( length );
		left_time -= length;
		
		// Add saw wave of slightly lower pitch to right buffer
		do {
			right_synth.update( right_time, right_amp = (right_amp + 1) % 10 );
		} while ( (right_time += 1000) < length );
		right.end_frame( length );
		right_time -= length;
		
		// buffer to read samples into
		int const buf_size = 2048;
		static blip_sample_t samples [buf_size];
		
		// Read left channel into even samples, right channel into odd samples:
		// LRLRLRLRLR...
		long count = left.read_samples( samples, buf_size / 2, 1 );
		right.read_samples( samples + 1, count, 1 );
		
		play_stereo_samples( samples, count * 2 );
	}
	
	return 0;
}


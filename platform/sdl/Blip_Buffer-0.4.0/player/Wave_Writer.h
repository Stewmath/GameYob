
// WAVE sound file writer for recording 16-bit output during program development

#ifndef WAVE_WRITER_H
#define WAVE_WRITER_H

#include <stddef.h>
#include <stdio.h>

class Wave_Writer {
public:
	typedef short sample_t;
	
	// Create sound file with given sample rate (in Hz) and filename.
	// Exits program if there's an error.
	Wave_Writer( long sample_rate, char const* filename = "out.wav" );
	
	// Enable stereo output
	void stereo( int );
	
	// Append 'count' samples to file. Use every 'skip'th source sample; allows
	// one channel of stereo sample pairs to be written by specifying a skip of 2.
	void write( const sample_t*, long count, int skip = 1 );
	
	// Append 'count' floating-point samples to file. Use every 'skip'th source sample;
	// allows one channel of stereo sample pairs to be written by specifying a skip of 2.
	//void write( const float*, long count, int skip = 1 );
	
	// Number of samples written so far
	long sample_count() const;
	
	// Write sound file header and close file. If no samples were written,
	// deletes file.
	~Wave_Writer();
	
private:
	enum { buf_size = 32768 * 2 };
	unsigned char* buf;
	FILE*   file;
	long    sample_count_;
	long    rate;
	long    buf_pos;
	int     chan_count;
	
	void flush();
};

inline void Wave_Writer::stereo( int s ) {
	chan_count = s ? 2 : 1;
}

inline long Wave_Writer::sample_count() const {
	return sample_count_;
}

#endif


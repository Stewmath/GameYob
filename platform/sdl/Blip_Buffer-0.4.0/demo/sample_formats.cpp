
// Functions to read samples as floating-point, 16-bit unsigned, and 8-bit unsigned
// (remove player.cpp since it will conflict with this)

#include "Blip_Buffer.h"

#include <stdio.h>

// Blip_Reader is optimized for efficiency, not ease-of-use, so it's
// a bit obscure to use.

// Read samples as floating-point, with values staying
// between 'low' and 'high' (defaults to -1.0 to 1.0).
long read_samples( Blip_Buffer& buf, float* out, long out_size,
		float low = -1.0f, float high = 1.0f )
{
	// Limit number of samples read to those available
	long count = buf.samples_avail();
	if ( count > out_size )
		count = out_size;
	
	// Begin reading samples from Blip_Buffer
	Blip_Reader reader;
	int bass = reader.begin( buf );
	
	float factor = (high - low) * 0.5f;
	float offset = low + 1.0f * factor;
	unsigned long const sample_range = 1UL << blip_sample_bits;
	factor *= 1.0f / (sample_range / 2);
	
	for ( long n = count; n--; )
	{
		// Read sample at full resolution and convert to output format
		long s = reader.read_raw();
		reader.next( bass );
		*out++ = s * factor + offset;
		if ( (unsigned long) (s + sample_range / 2) >= sample_range )
		{
			// clamp
			out [-1] = high;
			if ( s < 0 )
				out [-1] = low;
		}
	}
	
	// End reading and remove the samples
	reader.end( buf );
	buf.remove_samples( count );
	
	return count;
}

// Read samples as unsigned 16-bit
long read_samples( Blip_Buffer& buf, unsigned short* out, long out_size )
{
	// Limit number of samples read to those available
	long count = buf.samples_avail();
	if ( count > out_size )
		count = out_size;
	
	// Begin reading samples from Blip_Buffer
	Blip_Reader reader;
	int bass = reader.begin( buf );
	
	for ( long n = count; n--; )
	{
		// Read 16-bit sample and convert to output format
		long s = reader.read();
		reader.next( bass );
		if ( (short) s != s ) // clamp to 16 bits
			s = 0x7fff - (s >> 24);
		
		*out++ = s + 0x8000;
	}
	
	// End reading and remove the samples
	reader.end( buf );
	buf.remove_samples( count );
	
	return count;
}

// Read samples as unsigned 8-bit
long read_samples( Blip_Buffer& buf, unsigned char* out, long out_size )
{
	// Limit number of samples read to those available
	long count = buf.samples_avail();
	if ( count > out_size )
		count = out_size;
	
	// Begin reading samples from Blip_Buffer
	Blip_Reader reader;
	int bass = reader.begin( buf );
	
	for ( long n = count; n--; )
	{
		// Read 16-bit sample and convert to output format
		long s = reader.read();
		reader.next( bass );
		if ( (short) s != s ) // clamp to 16 bits
			s = 0x7fff - (s >> 24);
		
		*out++ = (s >> 8) + 0x80;
	}
	
	// End reading and remove the samples
	reader.end( buf );
	buf.remove_samples( count );
	
	return count;
}

// Quick test of functions
int main()
{
	Blip_Buffer buf;
	Blip_Synth<blip_low_quality,20> synth;
	
	// Setup buffer
	buf.clock_rate( 44100 );
	if ( buf.set_sample_rate( 44100 ) )
		return 1;
	synth.output( &buf );
	synth.volume( 0.5 );
	
	// Add wave that goes from 0 to 50% to -50%
	synth.update( 4,  10 );
	synth.update( 8, -10 );
	buf.end_frame( 30 );
	
	// Read samples as this type
	typedef float sample_t; // floating-point
	//typedef unsigned short sample_t; // unsigned 16-bit
	//typedef unsigned char sample_t; // unsigned 8-bit
	
	// Read and display samples
	const int max_samples = 30;
	sample_t samples [max_samples];
	int count = read_samples( buf, samples, max_samples );
	
	for ( int i = buf.output_latency() + 1; i < count; i++ )
		printf( "%.2f,", (double) samples [i] );
	printf( "\n" );
	
	return 0;
}


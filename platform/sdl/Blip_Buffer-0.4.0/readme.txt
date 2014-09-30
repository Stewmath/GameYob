Blip_Buffer 0.4.0: Band-Limited Sound Buffer
--------------------------------------------
Blip_Buffer provides waveform synthesis and sample buffering with a high-level
interface that allows easy generation of square waves and similar with high
sound quality. Waveforms are specified by amplitude changes at times in terms
of the source clock rate, then efficiently resampled to the output sample rate
with adjustable low-pass and high-pass filtering. It is well-suited for
emulation of the sound chips used video game consoles before the mid-1990s.

Author : Shay Green <hotpop.com@blargg>
Website: http://www.slack.net/~ant/
Forum  : http://groups.google.com/group/blargg-sound-libs
License: GNU Lesser General Public License (LGPL)


Getting Started
---------------
A series of tutorial programs and demos are included. To use a demo, compile
and run a program consisting of the demo's .cpp file, the source files in
player/ or sdl_player/ (as described below), and Blip_Buffer.cpp. The four
numbered demos should be gone through first, since they show the basics.

It is recommended that you use the SDL multimedia library when building the
demos, which enhances them with live display, output, and interactivity. It is
available at http://www.libsdl.org/ and works on most platforms. If you have
SDL, compile the demos with the source files from the sdl_player/ directory. If
you don't have SDL or want the demos to write their output to a wave sound
file, use the source files from the player/ directory

See notes.txt for more information, and Blip_Buffer.h and Blip_Synth.h for
reference. Post to the discussion forum for assistance.


Files
-----
notes.txt               General notes about the library
changes.txt             Changes made since previous releases
LGPL.txt                GNU Lesser General Public License

Blip_Buffer.h           Blip_Buffer and Blip_Synth
Blip_Buffer.cpp

demo/                   Tutorials and demos
  1.waveform.cpp        How to generate a simple waveform
  2.square.cpp          How to make a square wave
  3.clock_rate.cpp      How the clock rate and sample rate work
  4.continuous.cpp      How to generate a continuous square wave
  multiple_waves.cpp    Multiple waves can be added into a buffer
  stereo.cpp            How to generate stereo sound using two Blip_Buffers
  treble_bass.cpp       How treble and bass affect waveform
  buffering.cpp         Three ways of buffering samples in a Blip_Buffer
  external_mixing.cpp   How to mix an external sample buffer into a Blip_Buffer
  delta_synth.cpp       How to use low-level delta mode of Blip_Synth
  sample_formats.cpp    Reading samples in floating-point and unsigned formats
  sdl_audio.cpp         Complete example for programs using SDL sound
  
player/                 Use with demos to write output to wave sound file
  player.h              Demo player and utilities
  player.cpp
  Wave_Writer.h         Wave sound file writer
  Wave_Writer.cpp

sdl_player/             Use with demos to play and show output interactively
  player.cpp            Demo player using SDL
  Audio_Scope.h         Audio scope window
  Audio_Scope.cpp
  Sync_Audio.h          Simple synchronous audio interface for SDL sound
  Sync_Audio.cpp

all: Streamy.c
	g++ Streamy.c -g -o Streamy -framework CoreAudio -framework ApplicationServices -framework CoreMidi -framework AudioToolbox -framework AudioUnit

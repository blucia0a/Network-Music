all: Streamy.c
	g++ Streamy.c -std=c99 -g -o Streamy -framework CoreAudio -framework ApplicationServices -framework CoreMidi -framework AudioToolbox -framework AudioUnit

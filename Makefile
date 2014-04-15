all: Streamy.cpp
	g++ Streamy.cpp -g -o Streamy -framework CoreAudio -framework ApplicationServices -framework CoreMidi -framework AudioToolbox -framework AudioUnit

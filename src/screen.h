// screen.h

#ifndef _SCREEN_h
#define _SCREEN_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

class classScreen
{
private:
	unsigned int screen;
	unsigned int max_screen;
	unsigned int current_row;
public:
	classScreen(unsigned int _max);
	unsigned int Next();
	unsigned int Current();
	unsigned int Current(unsigned int s);
	void RowAdd(String r);
	void RowClear();
	String rows[4];
};

#endif


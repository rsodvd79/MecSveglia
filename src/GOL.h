// GOL.h

#ifndef _GOL_h
#define _GOL_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

#ifndef MondoLarghezza
#define MondoLarghezza 64
#endif

#ifndef MondoAltezza
#define MondoAltezza 32
#endif

class classGOL
{
private:
	bool calcolaCella(byte X, byte  Y);
	bool Spontanei = false;
	byte Generazione = 0;
public:
	classGOL();
	void Update();
	void Genesi();
	void Popola();
	bool Mondo[MondoLarghezza][MondoAltezza]{};

};

#endif


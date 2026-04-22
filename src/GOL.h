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
	bool calcolaCella(byte X, byte Y);
	bool Spontanei = false;
	uint32_t Generazione = 0;
	uint32_t _prevHash = 0;
	uint32_t _prevPrevHash = 0;
	uint8_t _stableCount = 0;
	uint32_t _gridHash(bool grid[][MondoAltezza]);
	bool _inserisciBlinker();
	void _resetHistory();
public:
	classGOL();
	void Update();
	void Genesi();
	void Popola();
	bool Mondo[MondoLarghezza][MondoAltezza]{};

private:
	bool NewMondo[MondoLarghezza][MondoAltezza]{};

};

#endif


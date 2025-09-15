// Button.h

#ifndef _BUTTON_h
#define _BUTTON_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "arduino.h"
#else
#include "WProgram.h"
#endif

enum ButtonType {
	NORMAL,
	PULLUP
};

class classButton {
private:
	int Pin;
	unsigned long lastDebounceTime;
	long debounceDelay;
	bool buttonState;
	bool lastButtonState;
	bool State;

public:
	classButton(int _Pin, ButtonType _Tipo);
	bool Pressed();
	void Update();
};

#endif


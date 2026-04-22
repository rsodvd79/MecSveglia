#include "Button.h"

classButton::classButton(int _Pin, ButtonInputMode _Tipo) {
	lastDebounceTime = 0;
	debounceDelay = 250;
	buttonState = false;
	lastButtonState = false;
	State = false;
	Pin = _Pin;
	_mode = _Tipo;

    switch (_Tipo)
    {
    case ButtonInputMode::BTN_NORMAL:
        pinMode(Pin, INPUT);
        break;
    case ButtonInputMode::BTN_PULLUP:
        pinMode(Pin, INPUT_PULLUP);
        break;
    default:
        break;
    }
}

bool classButton::Pressed() {
	byte ret = State;
	State = false;
	return  ret;
}

void classButton::Update() {
	unsigned long currentTime = millis();
	byte reading = false;

	int activeLevel = (_mode == ButtonInputMode::BTN_PULLUP) ? LOW : HIGH;
	if (digitalRead(Pin) == activeLevel) {
		reading = true;
	}

	if (reading != lastButtonState) {
		lastButtonState = reading;
		lastDebounceTime = currentTime;
	}
	else if ((currentTime - lastDebounceTime) > debounceDelay) {
		if (reading != buttonState) {
			buttonState = reading;
			State = reading;
		}
	}
}

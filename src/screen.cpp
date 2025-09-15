// 
// 
// 

#include "screen.h"

classScreen::classScreen(unsigned int _max) {
	screen = 0;
	max_screen = _max;
	current_row = 0;

}

unsigned int classScreen::Next() {
	screen++;

	if (screen > max_screen) {
		screen = 0;
	}

	return screen;

}

unsigned int classScreen::Current() {
	return screen;

}

unsigned int classScreen::Current(unsigned int s) {
	screen = s;
	return screen;

}

void classScreen::RowAdd(String r) {
	if (current_row < 4) {
		rows[current_row] = r;
		current_row++;
	}
	else {
		for (unsigned int i = 0; i < 3; i++) {
			rows[i] = rows[i + 1];
		}

		rows[3] = r;

	}

}

void classScreen::RowClear() {
    for (unsigned int i = 0; i < 4; i++) {
        rows[i] = "";
    }
    // resetta l'indice della riga corrente per ripartire dall'alto
    current_row = 0;

}

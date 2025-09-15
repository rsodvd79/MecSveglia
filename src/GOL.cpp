
#include "GOL.h"

classGOL::classGOL() {

	Genesi();

}

void classGOL::Genesi() {

	for (byte X = 0; X < MondoLarghezza; X++) {
		for (byte Y = 0; Y < MondoAltezza; Y++) {
			Mondo[X][Y] = false;
		}
	}

}

void classGOL::Popola() {
	randomSeed(analogRead(0));

	for (byte X = 0; X < MondoLarghezza; X++) {
		for (byte Y = 0; Y < MondoAltezza; Y++) {
			Mondo[X][Y] = (random(0, 100) >= 50);
		}
	}

}

void classGOL::Update() {
	Generazione++;
	unsigned int celle_vecchie = 0;

	for (byte X = 0; X < MondoLarghezza; X++) {
		for (byte Y = 0; Y < MondoAltezza; Y++) {
			if (Mondo[X][Y]) {
				celle_vecchie++;
			};
		}
	}

	if (Spontanei) {
		Spontanei = false;
		randomSeed(analogRead(0));
		unsigned int c = 0;

		do {
			c++;
			byte X = random(1, MondoLarghezza - 2);
			byte Y = random(0, MondoAltezza - 1);

			if (!Mondo[X - 1][Y] && !Mondo[X][Y] && !Mondo[X + 1][Y]) {
				Mondo[X - 1][Y] = true;
				Mondo[X][Y] = true;
				Mondo[X + 1][Y] = true;
				break;
			}

		} while (c < MondoLarghezza * MondoAltezza);

	}

	unsigned int celle_nuove = 0;
	bool NewMondo[MondoLarghezza][MondoAltezza]{};

	for (byte X = 0; X < MondoLarghezza; X++) {
		for (byte Y = 0; Y < MondoAltezza; Y++) {
			NewMondo[X][Y] = calcolaCella(X, Y);
			if (NewMondo[X][Y]) {
				celle_nuove++;
			};
		}
	}

	Spontanei = (celle_vecchie == celle_nuove) || (Generazione == 0);

	for (byte X = 0; X < MondoLarghezza; X++) {
		for (byte Y = 0; Y < MondoAltezza; Y++) {
			Mondo[X][Y] = NewMondo[X][Y];
		}
	}

}

bool classGOL::calcolaCella(byte X, byte  Y) {
	unsigned int c = 0;

	for (int xa = ((int)X - 1); xa <= ((int)X + 1); xa++) {
		for (int ya = ((int)Y - 1); ya <= ((int)Y + 1); ya++) {
			if (xa == (int)X && ya == (int)Y) {
				//' ignora se stessa

			}
			else {
				int xr = xa;
				int yr = ya;

				if (xr < 0) {
					xr = (MondoLarghezza - 1);
				}

				if (xr > (MondoLarghezza - 1)) {
					xr = 0;
				}

				if (yr < 0) {
					yr = (MondoAltezza - 1);
				}

				if (yr > (MondoAltezza - 1)) {
					yr = 0;
				}

				if (Mondo[xr][yr]) {
					c += 1;

				}

			}

		}

	}

	if (Mondo[X][Y] && c < 2) {
		return false;

	}
	else if (Mondo[X][Y] && c >= 2 && c <= 3) {
		return true;

	}
	else if (Mondo[X][Y] && c > 3) {
		return false;

	}
	else if (!Mondo[X][Y] && c == 3) {
		return true;

	}

	return false;

}

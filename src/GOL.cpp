
#include "GOL.h"

classGOL::classGOL() {

	Popola();

}

void classGOL::_resetHistory() {
	_prevHash = 0;
	_prevPrevHash = 0;
	_stableCount = 0;
	Spontanei = false;
}

void classGOL::Genesi() {

	for (byte X = 0; X < MondoLarghezza; X++) {
		for (byte Y = 0; Y < MondoAltezza; Y++) {
			Mondo[X][Y] = false;
		}
	}
	_resetHistory();

}

void classGOL::Popola() {
	randomSeed(esp_random());

	for (byte X = 0; X < MondoLarghezza; X++) {
		for (byte Y = 0; Y < MondoAltezza; Y++) {
			Mondo[X][Y] = (random(0, 100) >= 50);
		}
	}
	_resetHistory();

}

// Inserisce un blinker (3 celle allineate, orizzontale o verticale) in una
// posizione libera casuale. Restituisce true se il posizionamento ha avuto successo.
bool classGOL::_inserisciBlinker() {
	bool horiz = random(0, 2);
	unsigned int c = 0;

	if (horiz) {
		do {
			byte X = random(1, MondoLarghezza - 1); // 1..62: centro con margine per X±1
			byte Y = random(0, MondoAltezza);        // 0..31
			if (!Mondo[X - 1][Y] && !Mondo[X][Y] && !Mondo[X + 1][Y]) {
				Mondo[X - 1][Y] = Mondo[X][Y] = Mondo[X + 1][Y] = true;
				return true;
			}
		} while (++c < (unsigned)(MondoLarghezza * MondoAltezza));
	} else {
		do {
			byte X = random(0, MondoLarghezza);      // 0..63
			byte Y = random(1, MondoAltezza - 1);    // 1..30: centro con margine per Y±1
			if (!Mondo[X][Y - 1] && !Mondo[X][Y] && !Mondo[X][Y + 1]) {
				Mondo[X][Y - 1] = Mondo[X][Y] = Mondo[X][Y + 1] = true;
				return true;
			}
		} while (++c < (unsigned)(MondoLarghezza * MondoAltezza));
	}
	return false;
}

// Hash FNV-1a sulla griglia per rilevare stati identici (oscillatori periodici).
uint32_t classGOL::_gridHash(bool grid[][MondoAltezza]) {
	uint32_t h = 2166136261UL;
	for (byte x = 0; x < MondoLarghezza; x++) {
		for (byte y = 0; y < MondoAltezza; y++) {
			h ^= (uint32_t)(grid[x][y] ? 1 : 0);
			h *= 16777619UL;
		}
	}
	return h;
}

void classGOL::Update() {
	Generazione++;

	if (Spontanei) {
		Spontanei = false;
		randomSeed(esp_random());
		for (int i = 0; i < 3; i++) {
			_inserisciBlinker();
		}
		// Reset history: il confronto con generazioni precedenti non è più valido
		// dopo aver modificato manualmente la griglia.
		_prevHash = 0;
		_prevPrevHash = 0;
		_stableCount = 0;
	}

	unsigned int celle_nuove = 0;
	memset(NewMondo, 0, sizeof(NewMondo));

	for (byte X = 0; X < MondoLarghezza; X++) {
		for (byte Y = 0; Y < MondoAltezza; Y++) {
			NewMondo[X][Y] = calcolaCella(X, Y);
			if (NewMondo[X][Y]) {
				celle_nuove++;
			}
		}
	}

	uint32_t newHash = _gridHash(NewMondo);

	if (celle_nuove == 0 || newHash == _prevHash) {
		// Estinzione o stato statico: stimolo immediato
		Spontanei = true;
	} else if (newHash == _prevPrevHash) {
		// Oscillatore periodo-2: conta le generazioni consecutive
		// prima di iniettare nuovi blinker (evita loop rapido)
		if (++_stableCount >= 30) {
			Spontanei = true;
		}
	} else {
		_stableCount = 0;
	}

	_prevPrevHash = _prevHash;
	_prevHash = newHash;

	memcpy(Mondo, NewMondo, sizeof(Mondo));

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

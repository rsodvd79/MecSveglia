
#include "eyes.h"

classEye::classEye() {
	eyes = OCCHI_FRAME;
}

unsigned int classEye::Next() {
	eyes++;

	if (eyes >= OCCHI_FRAME) {
		eyes = 0;
	}

	return eyes;

}
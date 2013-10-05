#include <bcm2835.h>
#include "flipdot.h"

int main(void) {
	if (!bcm2835_init())
		return 1;

	flipdot_shutdown();

	return(0);
}

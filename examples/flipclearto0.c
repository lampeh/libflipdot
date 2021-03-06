#include <signal.h>
#include <bcm2835.h>
#include "flipdot.h"

int main(void) {
	if (!bcm2835_init())
		return 1;

	flipdot_init();
	flipdot_clear_to_0();
	flipdot_shutdown();

	return(0);
}

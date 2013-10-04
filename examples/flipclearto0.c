#include <bcm2835.h>
#include "flipdot.h"

int main(void) {
	if (!bcm2835_init())
		return 1;

	// shut down GPIOs on exit
	signal(SIGINT, flipdot_shutdown);
	signal(SIGTERM, flipdot_shutdown);

	flipdot_init();
	flipdot_clear_to_0();

	return(0);
}

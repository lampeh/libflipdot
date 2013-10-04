#include <signal.h>
#include <bcm2835.h>
#include "flipdot.h"

int main(void) {
	if (!bcm2835_init())
		return 1;

	// shut down GPIOs on exit
	signal(SIGINT, (__sighandler_t)flipdot_shutdown);
	signal(SIGTERM, (__sighandler_t)flipdot_shutdown);

	flipdot_init();
	flipdot_clear_to_1();

	return(0);
}

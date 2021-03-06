#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <bcm2835.h>
#include "flipdot.h"


#define BMP_SETBIT(b,x,y) ((uint8_t *)(b))[(((y)*DISP_COLS)+(x))>>3]|=(1<<((((y)*DISP_COLS)+(x))&7));
#define BMP_CLEARBIT(b,x,y) (((uint8_t *)(b))[(((y)*DISP_COLS)+(x))>>3]&=(1<<((((y)*DISP_COLS)+(x))&7))^0xFF);

flipdot_bitmap_t bmp;
unsigned int x, y;


int main(void) {
	int c;

	if (!bcm2835_init())
		return 1;

	// shut down GPIOs on exit
	signal(SIGINT, (__sighandler_t)flipdot_shutdown);
	signal(SIGTERM, (__sighandler_t)flipdot_shutdown);

	flipdot_init();
	flipdot_clear_to_0();

	memset(bmp, 0x00, sizeof(bmp));
	x = 0;
	y = 0;

	while ((c = getc(stdin)) != EOF) {
		if (c == '\n') {
			if ((c = getc(stdin)) == '\n' || c == EOF) {
				flipdot_update_bitmap(bmp);

				memset(bmp, 0x00, sizeof(bmp));
				x = 0;
				y = 0;

				if (c == EOF) {
					break;
				}
				continue;
			}

			x = 0;
			y++;
		}

		if (x < DISP_COLS && y < DISP_ROWS) {
			// Alles ist Eins - ausser der Null (und Space)
			if (c != '0' && c != ' ') {
				BMP_SETBIT(bmp, x, y);
			}
			x++;
		}
	}

	flipdot_shutdown();
	return(0);
}

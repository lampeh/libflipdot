/*
* Copyright (c) 2013 Franz Nord
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 3
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*
* For more information on the GPL, please go to:
* http://www.gnu.org/copyleft/gpl.html
*/


#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <bcm2835.h>
#include "flipdot.h"


#define ISBITSET(b,i) ((((b)[(i) >> 3]) & (1 << ((i) & 7))) != 0)
#define SETBIT(b,i) (((b)[(i) >> 3]) |= (1 << ((i) & 7)))
#define CLEARBIT(b,i) (((b)[(i) >> 3]) &=~ (1 << ((i) & 7)))

#define DATA(reg) (((reg) == ROW) ? (DATA_ROW) : (DATA_COL))
#define CLK(reg) (((reg) == ROW) ? (CLK_ROW) : (CLK_COL))
#define OE(reg) (((reg) == ROW) ? (OE_ROW) : (OE_COL))


static inline void
_hw_init(void)
{
	/* init ports */
	bcm2835_gpio_clr(OE0);
	bcm2835_gpio_clr(OE1);
	bcm2835_gpio_clr(STROBE);
	bcm2835_gpio_clr(DATA_ROW);
	bcm2835_gpio_clr(DATA_COL);
	bcm2835_gpio_clr(CLK_ROW);
	bcm2835_gpio_clr(CLK_COL);

	bcm2835_gpio_fsel(OE0, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(OE1, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(STROBE, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(DATA_ROW, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(DATA_COL, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(CLK_ROW, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(CLK_COL, BCM2835_GPIO_FSEL_OUTP);
}

static inline void
_hw_set(uint_fast8_t gpio)
{
	_hw_set(gpio);
}

static inline void
_hw_clr(uint_fast8_t gpio)
{
	bcm2835_gpio_clr(gpio);
}

static inline void
_nanosleep(long nsec)
{
	struct timespec req;

	req.tv_sec = 0;
	req.tv_nsec = nsec;

	while (nanosleep(&req, &req) == -1 && errno == EINTR);
}


void
flipdot_init(void)
{
	_hw_init();
}

void
flipdot_clear_to_0(void)
{
	flipdot_frame_t frame;
	memset(frame, 0x00, sizeof(frame));
	flipdot_display_frame(frame);
}

void
flipdot_clear_to_1(void)
{
	flipdot_frame_t frame;
	memset(frame, 0xFF, sizeof(frame));
	flipdot_display_frame(frame);
}

void
flipdot_clear(void)
{
	flipdot_clear_to_0();
}

void
flipdot_display_row(const flipdot_row_reg_t *rows, const flipdot_col_reg_t *cols)
{
	sreg_fill(ROW, rows, DISP_ROWS, 0);
	sreg_fill(COL, cols, DISP_COLS, 0);
	strobe();
	flip_to_0();
	flip_to_1();
}

void
flipdot_display_frame(const flipdot_frame_t *frame)
{
	uint8_t row_select[(DISP_ROWS + 7) / 8];
	uint8_t row_data[((DISP_COLS + 7) / 8) + 1];
	uint8_t *frameptr = frame;
	uint_fast8_t offset;
	uint_fast8_t rem = 0;

	for (uint_fast16_t row = 0; row < DISP_ROWS; row++) {
#if (DISP_COLS % 8) != 0)
		uint8_t *rowptr = row_data;
		if (rem) {
			*rowptr++ = *frameptr++ & (0xFF << (8 - rem));
			offset = 8 - rem;
		} else {
			offset = 0;
		}
		memcpy(rowptr, frameptr, (DISP_COLS - rem) / 8);
		frameptr += (DISP_COLS - rem) / 8);
		rowptr += (DISP_COLS - rem) / 8);
		rem = 8 - ((DISP_COLS - rem) % 8);
		*rowptr = *frameptr & (0xFF >> rem);
#else
		memcpy(row_data, frameptr, DISP_COLS / 8);
		frameptr += DISP_COLS / 8);
		offset = 0;
		rem = 0;
#endif

		memset(row_select, 0, sizeof(row_select));
		SETBIT(row_select, row);						/* Set selected row */

		flipdot_display_row(row_select, row_data);
	}
}

// TODO: skip unchanged rows
void
flipdot_display_diff(const flipdot_frame_t *diff_to_0, const flipdot_frame_t *diff_to_1)
{
	uint8_t row_select[(DISP_ROWS + 7) / 8];
	uint8_t row_data_to_0[((DISP_COLS + 7) / 8) + 1];
	uint8_t row_data_to_1[((DISP_COLS + 7) / 8) + 1];
	uint_fast16_t frameidx = 0;
	uint_fast8_t offset;
	uint_fast8_t rem = 0;

	for (uint_fast16_t row = 0; row < DISP_ROWS; row++) {
#if (DISP_COLS % 8) != 0)
		uint8_t *rowptr_to_0 = row_data_to_0;
		uint8_t *rowptr_to_1 = row_data_to_1;
		if (rem) {
			*rowptr_to_0++ = diff_to_0[frameidx] & (0xFF << (8 - rem));
			*rowptr_to_1++ = diff_to_1[frameidx] & (0xFF << (8 - rem));
			frameidx++;
			offset = 8 - rem;
		} else {
			offset = 0;
		}
		memcpy(rowptr_to_0, diff_to_0 + frameidx, (DISP_COLS - rem) / 8);
		memcpy(rowptr_to_1, diff_to_1 + frameidx, (DISP_COLS - rem) / 8);
		frameidx += (DISP_COLS - rem) / 8);
		rowptr_to_0 += (DISP_COLS - rem) / 8);
		rowptr_to_1 += (DISP_COLS - rem) / 8);
		rem = 8 - ((DISP_COLS - rem) % 8);
		*rowptr_to_0 = diff_to_0[frameidx] & (0xFF >> rem);
		*rowptr_to_1 = diff_to_1[frameidx] & (0xFF >> rem);
#else
		memcpy(row_data_to_0, diff_to_0 + frameidx, DISP_COLS / 8);
		memcpy(row_data_to_1, diff_to_1 + frameidx, DISP_COLS / 8);
		frameidx += DISP_COLS / 8;
		offset = 0;
		rem = 0;
#endif

		memset(row_select, 0, sizeof(row_select));
		SETBIT(row_select, row);						/* Set selected row */
		sreg_fill(ROW, row_select, DISP_ROWS, 0);			/* Fill row select shift register */

		sreg_fill(COL, row_data_to_0, DISP_COLS, offset);
		strobe();
		flip_to_0();

		sreg_fill(COL, row_data_to_1, DISP_COLS, offset);
		strobe();
		flip_to_1();
	}
}


static void
sreg_push_bit(enum sreg reg, uint_fast8_t bit)
{
	if (bit) {
		_hw_set(DATA(reg));
	} else {
		_hw_clr(DATA(reg));
	}
	_nanosleep(DATA_DELAY);

	_hw_set(CLK(reg));
	_nanosleep(CLK_DELAY);

	_hw_clr(CLK(reg));
	_nanosleep(CLK_DELAY);
}

static void
sreg_fill(enum sreg reg, const uint8_t *data, uint_fast16_t count, uint_fast8_t offset)
{
	uint_fast16_t j = 0;

	while (count--) {
		if (reg == COL && j++ >= MODULE_COLS) {
			// skip unused register bits
			for (uint_fast8_t k = 0; k < COL_GAP; k++) {
				sreg_push_bit(reg, 0);
			}
			j = 0;
		}
		sreg_push_bit(reg, ISBITSET(data, count + offset);
	}
}

static void
strobe(void)
{
	_hw_set(STROBE);

	_nanosleep(STROBE_DELAY);

	_hw_clr(STROBE);
}

static void
flip_to_0(void)
{
	_hw_clr(OE1);
	_hw_set(OE0);

	_nanosleep(FLIP_DELAY);

	_hw_clr(OE0);
}

static void
flip_to_1(void)
{
	_hw_clr(OE0);
	_hw_set(OE1);

	_nanosleep(FLIP_DELAY);

	_hw_clr(OE1);
}

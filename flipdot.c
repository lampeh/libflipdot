#include <stdint.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <bcm2835.h>
#include "flipdot.h"


#define SETBIT(b,i) (((b)[(i) >> 3]) |= (1 << ((i) & 7)))
#define ISBITSET(b,i) ((((b)[(i) >> 3]) & (1 << ((i) & 7))) != 0)

#ifndef _BV
#define _BV(x) (1 << (x))
#endif


static uint8_t frames[2][FRAME_BYTE_COUNT];
static uint8_t *frame_old, *frame_new;


static inline void
_nanosleep(long nsec)
{
	struct timespec req;

	req.tv_sec = 0;
	req.tv_nsec = nsec;

	while (nanosleep(&req, &req) == -1 && errno == EINTR);
}


static inline void
_hw_init(void)
{
	// clear ports
#ifdef GPIO_MULTI
	bcm2835_gpio_clr_multi(
		_BV(OE0) | _BV(OE1) | _BV(STROBE) |
		_BV(ROW_DATA) | _BV(ROW_CLK) |
		_BV(COL_DATA) | _BV(COL_CLK));
#else
	bcm2835_gpio_clr(OE0);
	bcm2835_gpio_clr(OE1);
	bcm2835_gpio_clr(STROBE);
	bcm2835_gpio_clr(ROW_DATA);
	bcm2835_gpio_clr(ROW_CLK);
	bcm2835_gpio_clr(COL_DATA);
	bcm2835_gpio_clr(COL_CLK);
#endif

	// set ports to output
	bcm2835_gpio_fsel(OE0, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(OE1, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(STROBE, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(ROW_DATA, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(ROW_CLK, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(COL_DATA, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(COL_CLK, BCM2835_GPIO_FSEL_OUTP);
}

static inline void
_hw_shutdown(void)
{
	// clear ports
#ifdef GPIO_MULTI
	bcm2835_gpio_clr_multi(
		_BV(OE0) | _BV(OE1) | _BV(STROBE) |
		_BV(ROW_DATA) | _BV(ROW_CLK) |
		_BV(COL_DATA) | _BV(COL_CLK));
#else
	bcm2835_gpio_clr(OE0);
	bcm2835_gpio_clr(OE1);
	bcm2835_gpio_clr(STROBE);
	bcm2835_gpio_clr(ROW_DATA);
	bcm2835_gpio_clr(ROW_CLK);
	bcm2835_gpio_clr(COL_DATA);
	bcm2835_gpio_clr(COL_CLK);
#endif

	// set ports to input
	// TODO: disable pull-ups?
	bcm2835_gpio_fsel(OE0, BCM2835_GPIO_FSEL_INPT);
	bcm2835_gpio_fsel(OE1, BCM2835_GPIO_FSEL_INPT);
	bcm2835_gpio_fsel(STROBE, BCM2835_GPIO_FSEL_INPT);
	bcm2835_gpio_fsel(ROW_DATA, BCM2835_GPIO_FSEL_INPT);
	bcm2835_gpio_fsel(ROW_CLK, BCM2835_GPIO_FSEL_INPT);
	bcm2835_gpio_fsel(COL_DATA, BCM2835_GPIO_FSEL_INPT);
	bcm2835_gpio_fsel(COL_CLK, BCM2835_GPIO_FSEL_INPT);
}

static inline void
_hw_set(uint8_t gpio)
{
	bcm2835_gpio_set(gpio);
}

static inline void
_hw_set_multi(uint32_t mask)
{
	bcm2835_gpio_set_multi(mask);
}

static inline void
_hw_clr(uint8_t gpio)
{
	bcm2835_gpio_clr(gpio);
}

static inline void
_hw_clr_multi(uint32_t mask)
{
	bcm2835_gpio_clr_multi(mask);
}


static inline void
sreg_strobe(void)
{
	_hw_set(STROBE);

#ifndef NOSLEEP
	_nanosleep(STROBE_DELAY);
#endif

	_hw_clr(STROBE);
}

static void
sreg_fill_col(uint8_t *col_data, uint_fast16_t col_count)
{
	while (col_count--) {
		if (ISBITSET(col_data, col_count)) {
			_hw_set(COL_DATA);
		} else {
			_hw_clr(COL_DATA);
		}

#ifndef NOSLEEP
		_nanosleep(DATA_DELAY);
#endif

		_hw_set(COL_CLK);

#ifndef NOSLEEP
		_nanosleep(CLK_DELAY);
#endif

		_hw_clr(COL_CLK);

#ifndef NOSLEEP
//		_nanosleep(CLK_DELAY);
#endif
	}
}

static void
sreg_fill_both(uint8_t *row_data, uint_fast16_t row_count, uint8_t *col_data, uint_fast16_t col_count)
{
	while (row_count || col_count) {

#ifdef GPIO_MULTI
		_hw_clr_multi(_BV(ROW_DATA) | _BV(COL_DATA));
		_hw_set_multi( ((row_count && ISBITSET(row_data, row_count - 1))?(_BV(ROW_DATA)):(0)) |
						((col_count && ISBITSET(col_data, col_count - 1))?(_BV(COL_DATA)):(0)));
#else
		if (row_count) {
			if (ISBITSET(row_data, row_count - 1)) {
				_hw_set(ROW_DATA);
			} else {
				_hw_clr(ROW_DATA);
			}
		}

		if (col_count) {
			if (ISBITSET(col_data, col_count - 1)) {
				_hw_set(COL_DATA);
			} else {
				_hw_clr(COL_DATA);
			}
		}
#endif

#ifndef NOSLEEP
		_nanosleep(DATA_DELAY);
#endif

#ifdef GPIO_MULTI
		_hw_set_multi( ((row_count)?(_BV(ROW_CLK)):(0)) | ((col_count)?(_BV(COL_CLK)):(0)));
#else
		if (row_count) {
			_hw_set(ROW_CLK);
		}

		if (col_count) {
			_hw_set(COL_CLK);
		}
#endif

#ifndef NOSLEEP
		_nanosleep(CLK_DELAY);
#endif

#ifdef GPIO_MULTI
		_hw_clr_multi( ((row_count)?(_BV(ROW_CLK)):(0)) | ((col_count)?(_BV(COL_CLK)):(0)));
#else
		if (row_count) {
			_hw_clr(ROW_CLK);
		}

		if (col_count) {
			_hw_clr(COL_CLK);
		}
#endif

		if (row_count) {
			row_count--;
		}

		if (col_count) {
			col_count--;
		}
	}
}


// TODO: protect OE pulse against long delay
static inline void
flip_to_0(void)
{
	_hw_clr(OE1);

	_nanosleep(OE_DELAY);

	_hw_set(OE0);

	_nanosleep(FLIP_DELAY);

	_hw_clr(OE0);
}

static inline void
flip_to_1(void)
{
	_hw_clr(OE0);

	_nanosleep(OE_DELAY);

	_hw_set(OE1);

	_nanosleep(FLIP_DELAY);

	_hw_clr(OE1);
}


void
flipdot_init(void)
{
	_hw_init();

	memset(frames[0], 0x00, FRAME_BYTE_COUNT);
	memset(frames[1], 0x00, FRAME_BYTE_COUNT);
	frame_old = frames[0];
	frame_new = frames[1];
}

void
flipdot_shutdown(void)
{
	_hw_shutdown();
}

void
flipdot_clear_to_0(void)
{
	memset(frame_old, 0x00, FRAME_BYTE_COUNT);
	flipdot_display_frame(frame_old);
}

void
flipdot_clear_to_1(void)
{
	memset(frame_old, 0xFF, FRAME_BYTE_COUNT);
	flipdot_display_frame(frame_old);
}

/*
void
flipdot_clear(void)
{
	flipdot_clear_to_0();
}
*/

void
flipdot_display_row(uint8_t *rows, uint8_t *cols)
{
	sreg_fill_both(rows, REGISTER_ROWS, cols, REGISTER_COLS);
	sreg_strobe();
	flip_to_0();
	flip_to_1();
}

void
flipdot_display_row_diff(uint8_t *rows, uint8_t *cols_to_0, uint8_t *cols_to_1)
{
	sreg_fill_both(rows, REGISTER_ROWS, cols_to_0, REGISTER_COLS);
	sreg_strobe();
	flip_to_0();

	sreg_fill_col(cols_to_1, REGISTER_COLS);
	sreg_strobe();
	flip_to_1();
}

void
flipdot_display_frame(uint8_t *frame)
{
	uint8_t rows[REGISTER_ROW_BYTE_COUNT];
	uint8_t *frameptr;

	memcpy(frame_new, frame, FRAME_BYTE_COUNT);
	frameptr = (uint8_t *)frame_new;

	for (uint_fast16_t row = 0; row < REGISTER_ROWS; row++) {
		memset(rows, 0, sizeof(rows));
		SETBIT(rows, row);

		flipdot_display_row(rows, frameptr);

		frameptr += REGISTER_COL_BYTE_COUNT;
	}
}

void
flipdot_display_bitmap(uint8_t *bitmap)
{
	uint8_t frame[FRAME_BYTE_COUNT];

	flipdot_bitmap_to_frame(bitmap, frame);
	flipdot_display_frame(frame);
}

void
flipdot_update_frame(uint8_t *frame)
{
	uint8_t rows[REGISTER_ROW_BYTE_COUNT];
	uint8_t cols_to_0[REGISTER_COL_BYTE_COUNT];
	uint8_t cols_to_1[REGISTER_COL_BYTE_COUNT];
	uint8_t *frameptr_old;
	uint8_t *frameptr_new;
	uint_fast8_t row_changed;

	uint8_t *tmp = frame_old;
	frame_old = frame_new;
	frame_new = tmp;

	memcpy(frame_new, frame, FRAME_BYTE_COUNT);

	frameptr_old = (uint8_t *)frame_old;
	frameptr_new = (uint8_t *)frame_new;

	for (uint_fast16_t row = 0; row < REGISTER_ROWS; row++) {
		row_changed = 0;

		for (uint_fast16_t col = 0; col < REGISTER_COL_BYTE_COUNT; col++) {
			cols_to_0[col] = ~((*frameptr_old) & ~(*frameptr_new));
			cols_to_1[col] = (~(*frameptr_old) & (*frameptr_new));

			if (cols_to_0[col] != 0xFF || cols_to_1[col] != 0x00) {
				row_changed = 1;
			}

			frameptr_old++;
			frameptr_new++;
		}

		if (row_changed) {
			memset(rows, 0, sizeof(rows));
			SETBIT(rows, row);

			flipdot_display_row_diff(rows, cols_to_0, cols_to_1);
		}
	}
}

void
flipdot_update_bitmap(uint8_t *bitmap)
{
	uint8_t frame[FRAME_BYTE_COUNT];

	flipdot_bitmap_to_frame(bitmap, frame);
	flipdot_update_frame(frame);
}

// Slow bit copy
void
flipdot_bitmap_to_frame(uint8_t *bitmap, uint8_t *frame)
{
	memset(frame, 0x00, FRAME_BYTE_COUNT);

	for (uint_fast16_t i = 0; i < DISP_PIXEL_COUNT; i++) {
		if (ISBITSET((uint8_t *)bitmap, i)) {
			SETBIT((uint8_t *)frame, i + ((i / MODULE_COLS) * COL_GAP));
		}
	}
}

void
flipdot_frame_to_bitmap(uint8_t *frame, uint8_t *bitmap)
{
	memset(bitmap, 0x00, DISP_BYTE_COUNT);

	for (uint_fast16_t i = 0; i < DISP_PIXEL_COUNT; i++) {
		if (ISBITSET((uint8_t *)frame, i + ((i / MODULE_COLS) * COL_GAP))) {
			SETBIT((uint8_t *)bitmap, i);
		}
	}
}

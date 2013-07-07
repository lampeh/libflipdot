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


#ifndef FLIPDOT_H
#define FLIPDOT_H

#include <stdint.h>


// BCM2835 GPIO pin mapping
#define STROBE 7

#define DATA_COL 11
#define DATA_ROW 8

#define CLK_COL 9
#define CLK_ROW 25

#define OE1 10
#define OE0 24


// shift register set-up time (ns)
#define DATA_DELAY 15
// shift register pulse width (ns)
#define CLK_DELAY 25
#define STROBE_DELAY 25

// flip motor pulse width (ns)
#define FLIP_DELAY 500*1000


// display geometry
#define MODULE_COUNT_H 1
#define MODULE_COUNT_V 1

#define MODULE_COLS 20
#define MODULE_ROWS 16

#define MODULE_PIXEL_COUNT (MODULE_COLS * MODULE_ROWS)
#define MODULE_BYTE_COUNT ((MODULE_PIXEL_COUNT + 7) / 8)

#define COL_GAP (8 - (MODULE_COLS % 8))

#define REGISTER_COLS (MODULE_COUNT_H * (MODULE_COLS + COL_GAP))
#define REGISTER_ROWS (MODULE_COUNT_V * MODULE_ROWS)

#define DISP_COLS (MODULE_COUNT_H * MODULE_COLS)
#define DISP_ROWS (MODULE_COUNT_V * MODULE_ROWS)

#define DISP_PIXEL_COUNT (DISP_COLS * DISP_ROWS)
#define DISP_BYTE_COUNT ((DISP_PIXEL_COUNT + 7) / 8)


#if (REGISTER_COLS > UINT_FAST16_MAX)
#error "unsupported display size: REGISTER_COLS too large for uint_fast16_t"
#endif

#if (REGISTER_ROWS > UINT_FAST16_MAX)
#error "unsupported display size: REGISTER_ROWS too large for uint_fast16_t"
#endif

#if (DISP_PIXEL_COUNT > UINT_FAST16_MAX)
#error "unsupported display size: DISP_PIXEL_COUNT too large for uint_fast16_t"
#endif


typedef uint8_t flipdot_frame_t[DISP_BYTE_COUNT];
typedef uint8_t flipdot_row_reg_t[(REGISTER_ROWS + 7) / 8];
typedef uint8_t flipdot_col_reg_t[(REGISTER_COLS + 7) / 8];


enum sreg {
	ROW,
	COL
};


void flipdot_init(void);
void flipdot_clear_to_0(void);
void flipdot_clear_to_1(void);
void flipdot_display_row(const flipdot_row_reg_t *rows, const flipdot_col_reg_t *cols);
void flipdot_display_frame(const flipdot_frame_t *frame);
void flipdot_display_diff(const flipdot_frame_t *diff_to_0, const flipdot_frame_t *diff_to_1);

extern inline void
flipdot_clear(void)
{
    flipdot_clear_to_0();
}

#endif /* FLIPDOT_H */

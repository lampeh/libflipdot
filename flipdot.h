#ifndef FLIPDOT_H
#define FLIPDOT_H

#include <stdint.h>


// BCM2835 GPIO pin mapping
// see http://elinux.org/RPi_Low-level_peripherals

#define ROW_DATA 11
#define ROW_CLK 9

#define COL_DATA 8
#define COL_CLK 25

#define STROBE 7

#define OE0 24
#define OE1 10


// Timing parameters
// nanosleep is only roughly accurate
// delays can be much longer

// shift register set-up time (ns)
#define DATA_DELAY 15

// shift register pulse width (ns)
#define CLK_DELAY 25
#define STROBE_DELAY 25

// OE0 to OE1 delay (ns)
#define OE_DELAY 100*1000

// flip motor pulse width (ns)
#define FLIP_DELAY 500*1000


// Display geometry

#define MODULE_COUNT_H 1
#define MODULE_COUNT_V 1

#define MODULE_COLS 20
#define MODULE_ROWS 16

#define COL_GAP (8 - (MODULE_COLS % 8))

#define MODULE_PIXEL_COUNT (MODULE_COLS * MODULE_ROWS)
#define MODULE_BYTE_COUNT ((MODULE_PIXEL_COUNT + 7) / 8)

#define REGISTER_COLS (MODULE_COUNT_H * (MODULE_COLS + COL_GAP))
#define REGISTER_ROWS (MODULE_COUNT_V * MODULE_ROWS)

#define REGISTER_COL_BYTE_COUNT ((REGISTER_COLS + 7) / 8)
#define REGISTER_ROW_BYTE_COUNT ((REGISTER_ROWS + 7) / 8)

#define DISP_COLS (MODULE_COUNT_H * MODULE_COLS)
#define DISP_ROWS (MODULE_COUNT_V * MODULE_ROWS)

#define DISP_PIXEL_COUNT (DISP_COLS * DISP_ROWS)
#define DISP_BYTE_COUNT ((DISP_PIXEL_COUNT + 7) / 8)

#define FRAME_PIXEL_COUNT (REGISTER_COLS * REGISTER_ROWS)
#define FRAME_BYTE_COUNT ((FRAME_PIXEL_COUNT + 7) / 8)


#if (REGISTER_COLS > INT_FAST16_MAX)
#error "unsupported display size: REGISTER_COLS too large for int_fast16_t"
#endif

#if (REGISTER_ROWS > INT_FAST16_MAX)
#error "unsupported display size: REGISTER_ROWS too large for int_fast16_t"
#endif

#if (FRAME_PIXEL_COUNT > INT_FAST16_MAX)
#error "unsupported display size: FRAME_PIXEL_COUNT too large for int_fast16_t"
#endif


typedef uint8_t flipdot_frame_t[FRAME_BYTE_COUNT];
typedef uint8_t flipdot_bitmap_t[DISP_BYTE_COUNT];

typedef uint8_t flipdot_col_reg_t[REGISTER_COL_BYTE_COUNT];
typedef uint8_t flipdot_row_reg_t[REGISTER_ROW_BYTE_COUNT];


void flipdot_init(void);
void flipdot_shutdown(void);
// int flipdot_install_handler(void)

void flipdot_clear_to_0(void);
void flipdot_clear_to_1(void);
static inline void flipdot_clear(void) { flipdot_clear_to_0(); }

void flipdot_display_row(uint8_t *rows, uint8_t *cols);
void flipdot_display_row_diff(uint8_t *rows, uint8_t *cols_to_0, uint8_t *cols_to_1);

void flipdot_display_frame(uint8_t *frame);
void flipdot_display_bitmap(uint8_t *bitmap);

void flipdot_update_frame(uint8_t *frame);
void flipdot_update_bitmap(uint8_t *bitmap);

void flipdot_bitmap_to_frame(uint8_t *bitmap, uint8_t *frame);
void flipdot_frame_to_bitmap(uint8_t *frame, uint8_t *bitmap);


#endif /* FLIPDOT_H */

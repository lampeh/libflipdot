libflipdot
==========

Flip Dot Display driver for Raspberry Pi  
https://wiki.attraktor.org/FlipdotDisplay


Installation
------------

* Install the [bcm2835](http://www.airspayce.com/mikem/bcm2835/) library
* Edit flipdot.h
  * configure GPIO to display input mapping
  * configure display geometry
* Connect display to GPIOs
* `make`
* `sudo ./examples/fliptest`
* `sudo ./examples/flipclear`
* `echo 'Hello World!' |toilet -f 3x5 |sudo ./examples/flip_pipe`

**Beware of signals!**  
Terminating a program while one of the OE outputs is still set will
keep the display transistors on and may cause damage to the matrix
or the driver board. Call `flipdot_shutdown()` or run
`sudo ./examples/flipoff` to turn the GPIO outputs off.


Examples
--------

`flip_pipe`: Reads an ASCII bitmap followed by an empty line (\\n\\n)
from stdin and sends it to the display, loops until EOF. Use
[this 3x5 figlet font](http://www.figlet.org/fontdb_example.cgi?font=3x5.flf)
to pipe text onto the display.

To use the library for your own code, copy flipdot.h and libflipdot.a
where compiler and linker will find it. Link with `-lflipdot`


Functions
---------

`void flipdot_init(void);`  
Initialize hardware and internal buffers

`void flipdot_shutdown(void);`  
Shut down hardware outputs

`void flipdot_clear(void);`  
`void flipdot_clear_to_0(void);`  
Flip all pixels to position 0

`void flipdot_clear_to_1(void);`  
Flip all pixels to position 1

`void flipdot_clear_full(void)`  
Flip all pixels to 0, to 1 and back to 0.
This should recover pixels stuck in the middle between end positions

`void flipdot_display_row(const uint8_t *rows, const uint8_t *cols);`  
Flip all pixels in rows selected by `rows` to positions set in `cols`.  
The internal frame buffer is **not** updated to reflect the changes

`void flipdot_display_row_diff(const uint8_t *rows, const uint8_t *cols_to_0, const uint8_t *cols_to_1);`  
Flip 0-bits in `cols_to_0` and 1-bits in `cols_to_1` in rows selected by `rows`.  
The internal frame buffer is **not** updated to reflect the changes

`void flipdot_display_frame(const uint8_t *frame);`  
Display a full display frame.  
`frame` contains all pixels shifted into the display registers,
including blind gaps between horizontal modules

`void flipdot_display_bitmap(const uint8_t *bitmap);`  
Display a full bitmap.  
`bitmap` contains only the visible pixels of the display,
excluding any blind gaps

`void flipdot_update_frame(const uint8_t *frame);`  
Update the internal frame buffer and flip only the difference to the last frame.  
`frame` contains all pixels shifted into the display registers,
including blind gaps between horizontal modules

`void flipdot_update_bitmap(const uint8_t *bitmap);`  
Update the internal frame buffer and flip only the difference to the last frame.  
`bitmap` contains only the visible pixels of the display,
excluding any blind gaps

`void flipdot_bitmap_to_frame(const uint8_t *bitmap, flipdot_frame_t *frame);`  
`void flipdot_frame_to_bitmap(const uint8_t *frame, flipdot_bitmap_t *bitmap);`  
Convert between bitmap and frame format by adding or removing blind gaps

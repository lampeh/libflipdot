libflipdot
==========

Flip Dot Display driver for Raspberry Pi
https://wiki.attraktor.org/FlipdotDisplay

* Install the [bcm2835](http://www.airspayce.com/mikem/bcm2835/) library
* Edit flipdot.h
  * configure GPIO to display input mapping
  * configure display geometry
* Connect display to GPIOs
* `make`
* `sudo ./examples/fliptest`
* `sudo ./examples/flipclear`
* `echo 'Hello World!' |toilet -f 3x5 |sudo ./examples/flip_pipe`

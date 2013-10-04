SOURCES=examples/flip_pipe.c examples/fliptest.c examples/flipclear.c examples/flipclearto0.c examples/flipclearto1.c

CPPFLAGS=-I.
CFLAGS=-O3 -flto -Wall -std=gnu99 -pedantic -funroll-loops -fno-common -ffunction-sections
LDFLAGS=-flto -Wl,--relax,--gc-sections -L . -lflipdot -lbcm2835

OBJECTS=$(SOURCES:.c=.o)
EXECUTABLES=$(SOURCES:.c=)
LIBRARIES=libflipdot.a

all: $(EXECUTABLES) $(LIBRARIES)

clean:
	-rm $(EXECUTABLES) $(OBJECTS) $(LIBRARIES) flipdot.o

flipdot.o: flipdot.c flipdot.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -DNOSLEEP -DGPIO_MULTI -c -o $@ $<

$(EXECUTABLES): % : %.o $(LIBRARIES)
	$(CC) -o $@ $< $(LDFLAGS)

$(LIBRARIES): flipdot.o
	$(AR) rcs $@ $^

%.dep: %.c
	$(CC) $(CPPFLAGS) -MM -MT $(<:.c=.o) -MP -MF $@ $<

-include $(SOURCES:.c=.dep)

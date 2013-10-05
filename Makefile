CPPFLAGS=-I.
CFLAGS=-g -O3 -flto -Wall -std=gnu99 -pedantic -funroll-loops -fno-common -ffunction-sections
LDFLAGS=-flto -Wl,--relax,--gc-sections -L . -lflipdot -lbcm2835

LIB=libflipdot.a
LIB_SOURCES=flipdot.c
LIB_OBJECTS=$(LIB_SOURCES:.c=.o)
LIB_DEP=$(LIB_SOURCES:.c=.dep)
LIB_CFLAGS=$(CFLAGS) -DNOSLEEP -DGPIO_MULTI
LIB_CPPFLAGS=$(CPPFLAGS)

SOURCES=$(wildcard examples/*.c)
OBJECTS=$(SOURCES:.c=.o)
DEP=$(SOURCES:.c=.dep)
EXECUTABLES=$(SOURCES:.c=)

all: $(LIB) $(EXECUTABLES)

clean:
	-rm $(LIB) $(LIB_OBJECTS) $(LIB_DEP) $(EXECUTABLES) $(OBJECTS) $(DEP)

$(LIB): $(LIB_OBJECTS)
	$(AR) rcs $@ $^

$(EXECUTABLES): % : %.o $(LIB)
	$(CC) -o $@ $< $(LDFLAGS)

$(LIB_OBJECTS): %.o : %.c
	$(CC) $(LIB_CFLAGS) $(LIB_CPPFLAGS) -c -o $@ $<

%.dep: %.c
	$(CC) $(CPPFLAGS) -MM -MT $(<:.c=.o) -MP -MF $@ $<

-include $(DEP)
-include $(LIB_DEP)

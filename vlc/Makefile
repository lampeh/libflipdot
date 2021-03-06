PREFIX = /usr
LD = ld
CC = cc
INSTALL = install
CFLAGS = -g -Wextra -DNOSLEEP -O3 -flto -Wall -std=gnu99 -pedantic -funroll-loops -fno-common
LDFLAGS = -flto -Wl,--relax,--gc-sections -L ..
VLC_PLUGIN_CFLAGS := $(shell pkg-config --cflags vlc-plugin)
VLC_PLUGIN_LIBS := $(shell pkg-config --libs vlc-plugin) -lflipdot

libdir = $(PREFIX)/lib
plugindir = $(libdir)/vlc/plugins

override CC += -std=gnu99
override CPPFLAGS += -DPIC -I. -I.. -Isrc
override CFLAGS += -fPIC
override LDFLAGS += -Wl,-no-undefined,-z,defs -lbcm2835

override CPPFLAGS += -DMODULE_STRING=\"flipdot\"
override CFLAGS += $(VLC_PLUGIN_CFLAGS)
override LDFLAGS += $(VLC_PLUGIN_LIBS)

TARGETS = libflipdot_plugin.so

all: libflipdot_plugin.so

install: all
	mkdir -p -- $(DESTDIR)$(plugindir)/video_output
	$(INSTALL) --mode 0755 libflipdot_plugin.so $(DESTDIR)$(plugindir)/video_output

install-strip:
	$(MAKE) install INSTALL="$(INSTALL) -s"

uninstall:
	rm -f $(plugindir)/video_output/libflipdot_plugin.so

clean:
	rm -f -- libflipdot_plugin.so src/*.o

mostlyclean: clean

SOURCES = flipdot_plugin.c

$(SOURCES:%.c=src/%.o): %: ../flipdot.h ../libflipdot.a

libflipdot_plugin.so: $(SOURCES:%.c=src/%.o)
	$(CC) -shared -o $@ $^ $(LDFLAGS)

.PHONY: all install install-strip uninstall clean mostlyclean

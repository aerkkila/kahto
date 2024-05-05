include config.mk

prefix=/usr/local

CC = gcc
CFLAGS += -Wall -g -DHAVE_PNG
CFLAGS_OBJ = $(CFLAGS) -c
CFLAGS_LIB = $(CFLAGS) -shared -fpic
LDLIBS += -lm -lpng

ifdef use_libttra
	LDLIBS += -lttra
else
	OBJECTS += ttra/ttra.o
	LDLIBS += $(pkg-config --libs freetype2) -lutf8proc -lfontconfig
	CFLAGS += $(pkg-config --cflags freetype2)
endif

ifdef use_libwaylandhelper
	LDLIBS += -lwaylandhelper
else
	OBJECTS += waylandhelper/waylandhelper.o
	LDLIBS += -lxkbcommon -lwayland-client
endif

testi.out: testi.c cplot.o $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

cplot.o: cplot.c cplot.h rendering.c rotate.c functions.c ticker.c commit.c png.c Makefile
	$(CC) $(CFLAGS_OBJ) -o $@ $<

libcplot.so: cplot.c cplot.h rendering.c rotate.c functions.c ticker.c commit.c png.c Makefile
	$(CC) $(CFLAGS_LIB) -o $@ $< $(OBJECTS) $(LDLIBS)

functions.c: make_functions.pl functions.in.c
	./$<

waylandhelper:
	git clone https://codeberg.org/aerkkila/waylandhelper

waylandhelper/waylandhelper.o: waylandhelper
	cd waylandhelper && $(MAKE) waylandhelper.o

ttra:
	git clone https://codeberg.org/aerkkila/ttra

ttra/ttra.o: ttra
	cd ttra && $(MAKE) ttra.o

clean:
	rm -f *.out *.so *.o functions.c
	if [ -e waylandhelper ]; then cd waylandhelper && $(MAKE) clean; fi
	if [ -e ttra ]; then cd ttra && $(MAKE) clean; fi

install: libcplot.so
	cp cplot.h $(prefix)/include
	cp libcplot.so $(prefix)/lib

uninstall:
	rm -rf $(prefix)/include/cplot.h $(prefix)/lib/libcplot.so

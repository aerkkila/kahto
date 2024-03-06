CFLAGS = -Wall -g
CC = gcc
LIBS = -lxkbcommon -lwayland-client -lm -lttra

CFLAGS += -DHAVE_PNG
LIBS += -lpng

testi.out: testi.c cplot.o wayland_helper/waylandhelper.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

cplot.o: cplot.c cplot.h rendering.c rotate.c functions.c ticker.c commit.c png.c wayland_helper/waylandhelper.o Makefile
	$(CC) $(CFLAGS) -c -o $@ $<

wayland_helper/waylandhelper.o: wayland_helper/*.[ch]
	cd wayland_helper && $(MAKE) waylandhelper.o

functions.c: make_functions.pl functions.in.c
	./$<

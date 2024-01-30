testi.out: testi.c cplot.o wayland_helper/waylandhelper.o
	cc -o $@ $^ -lxkbcommon -lwayland-client -lm

cplot.o: cplot.c rendering.c wayland_helper/waylandhelper.o Makefile
	gcc -c -Wall -g -o $@ $<

wayland_helper/waylandhelper.o: wayland_helper/*.[ch]
	cd wayland_helper && $(MAKE) waylandhelper.o

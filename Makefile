testi.out: testi.c cplot.o wayland_helper/waylandhelper.o
	gcc -o $@ $^ -lxkbcommon -lwayland-client -lm -lttra

cplot.o: cplot.c cplot.h rendering.c rotate.c wayland_helper/waylandhelper.o Makefile
	gcc -c -Wall -g -o $@ $<

wayland_helper/waylandhelper.o: wayland_helper/*.[ch]
	cd wayland_helper && $(MAKE) waylandhelper.o

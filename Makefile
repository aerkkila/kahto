include config.mk

prefix=/usr/local

CC = gcc
CFLAGS += -Wall -g -DHAVE_PNG
CFLAGS_OBJ = $(CFLAGS) -c
CFLAGS_LIB = $(CFLAGS) -shared -fpic
LDLIBS += -lm -lpng
OBJECTS =
cplot_sources = cplot.h cplot_rendering.c rotate.c functions.c ticker.c layout.c png.c Makefile
dep_build =
dep_install =
dep_get =

ifeq ($(use_libttra), 1)
	LDLIBS += -lttra
else ifeq ($(use_libttra), 2)
	LDLIBS += -lttra
	dep_get += ttra
	dep_build += ttra/libttra.so
	dep_install += ttra_install
else # mandatory
	dep_get += ttra
	dep_build += ttra/libttra.so
	OBJECTS += ttra/ttra.o
	LDLIBS += `pkg-config --libs freetype2` -lutf8proc -lfontconfig
	CFLAGS += -Ittra
endif

ifeq ($(use_cmh_colormaps), 1)
else ifeq ($(use_cmh_colormaps), 2)
	dep_get += colormap-headers
	dep_build += colormap-headers
	dep_install += colormap-headers_install
else # mandatory
	dep_get += colormap-headers
	dep_build += colormap-headers
	cplot_sources += colormap-headers/cmh_colormaps.h
	CFLAGS += -Icolormap-headers
endif

ifeq ($(use_libwaylandhelper), 1)
	LDLIBS += -lwaylandhelper
	CFLAGS += -DHAVE_wlh
else ifeq ($(use_libwaylandhelper), 2)
	LDLIBS += -lwaylandhelper
	dep_get += waylandhelper
	dep_build += waylandhelper/libwaylandhelper.so
	dep_install += wlh_install
	CFLAGS += -DHAVE_wlh
else ifeq ($(use_libwaylandhelper), 3)
	dep_get += waylandhelper
	dep_build += waylandhelper/libwaylandhelper.so
	OBJECTS += waylandhelper/waylandhelper.o
	LDLIBS += -lxkbcommon -lwayland-client
	CFLAGS += -Iwaylandhelper -DHAVE_wlh
endif

ifeq ($(use_ffmpeg), 1)
	LDLIBS += -lavcodec -lavutil -lavformat
	CFLAGS += -DHAVE_ffmpeg
	cplot_sources += cplot_video.c
endif

all: libcplot.so

tulosta:
	@echo dep_get: $(dep_get)
	@echo dep_build: $(dep_build)
	@echo dep_install: $(dep_install)

testi.out: testi.c cplot.o $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

cplot.o: cplot.c $(cplot_sources) config.mk
	$(CC) $(CFLAGS_OBJ) -o $@ $<

libcplot.so: cplot.c $(cplot_sources) $(OBJECTS) config.mk
	$(CC) $(CFLAGS_LIB) -o $@ $< $(OBJECTS) $(LDLIBS)

functions.c: make_functions.pl functions.in.c
	./$<

# dependencies
# ============
waylandhelper:
	git clone https://codeberg.org/aerkkila/waylandhelper
waylandhelper/waylandhelper.o: waylandhelper
	cd $< && $(MAKE) waylandhelper.o
waylandhelper/libwaylandhelper.so: waylandhelper
	cd $< && $(MAKE)
wlh_install: waylandhelper
	cd $< && $(MAKE) install

ttra:
	git clone https://codeberg.org/aerkkila/ttra
ttra/ttra.o: ttra
	cd $< && $(MAKE) ttra.o
ttra/libttra.so: ttra
	cd $< && $(MAKE)
ttra_install: ttra
	cd $< && $(MAKE) install

colormap-headers:
	git clone https://codeberg.org/aerkkila/colormap-headers
colormap-headers/cmh_colormaps.h: colormap-headers
colormap-headers_install: colormap-headers
	cd $< && $(MAKE) install

install_dep: $(dep_install)
build_dep: $(dep_build)
get_dep: $(dep_get)

# end dependencies
# ================

clean:
	rm -f *.out *.so *.o functions.c
	if [ -e waylandhelper ]; then cd waylandhelper && $(MAKE) clean; fi
	if [ -e ttra ]; then cd ttra && $(MAKE) clean; fi
	@printf "\e[93;1mDownloaded dependencies and config.mk are not removed.\e[0m\n"

install: libcplot.so
	cp cplot.h $(prefix)/include
	cp libcplot.so $(prefix)/lib

uninstall_this:
	rm -rf $(prefix)/include/cplot.h $(prefix)/lib/libcplot.so

uninstall: uninstall_this
	@printf "\e[1mTo uninstall dependencies, run $(MAKE) uninstall_all\n"

uninstall_all: uninstall_this
	cd ttra && $(MAKE) unistall || :
	cd waylandhelper && $(MAKE) uninstall || :
	cd colormap-headers && $(MAKE) uninstall || :

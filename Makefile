include config.mk

CFLAGS += -Wall -g -DHAVE_PNG
CFLAGS_OBJ = $(CFLAGS) -c
CFLAGS_LIB = $(CFLAGS) -shared -fpic
LDLIBS += -lm -lpng
OBJECTS =
cplot_sources = cplot.h cplot_rendering.c rotate.c functions.c ticker.c layout.c cplot_png.c Makefile
dep_dir = dependencies
dep_get =

ifeq ($(use_libttra), 1)
	LDLIBS += -lttra
else # mandatory
	dep_get += $(dep_dir)/ttra
	OBJECTS += $(dep_dir)/ttra/ttra.o
	LDLIBS += `pkg-config --libs freetype2` -lutf8proc -lfontconfig
	CFLAGS += -I$(dep_dir)/ttra
endif

ifeq ($(use_cmh_colormaps), 1)
else # mandatory
	dep_get += $(dep_dir)/colormap-headers
	cplot_sources += $(dep_dir)/colormap-headers/cmh_colormaps.h
	CFLAGS += -I$(dep_dir)/colormap-headers
endif

ifeq ($(use_libwaylandhelper), 1)
	LDLIBS += -lwaylandhelper
	CFLAGS += -DHAVE_wlh
else ifeq ($(use_libwaylandhelper), 2)
	dep_get += $(dep_dir)/waylandhelper
	OBJECTS += $(dep_dir)/waylandhelper/waylandhelper.o
	LDLIBS += -lxkbcommon -lwayland-client
	CFLAGS += -I$(dep_dir)/waylandhelper -DHAVE_wlh
endif
ifneq ($(use_libwaylandhelper), 0)
	cplot_sources += cplot_wayland.c
endif

ifeq ($(use_ffmpeg), 1)
	LDLIBS += -lavcodec -lavutil -lavformat
	CFLAGS += -DHAVE_ffmpeg
	cplot_sources += cplot_video.c
endif

all: libcplot.so

tulosta:
	@echo dep_get: $(dep_get)
	@echo LDLIBS: $(LDLIBS)

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
$(dep_dir)/waylandhelper: | $(dep_dir)
	cd $(dep_dir) && git clone https://codeberg.org/aerkkila/waylandhelper
$(dep_dir)/waylandhelper/waylandhelper.o: $(dep_dir)/waylandhelper
	cd $< && $(MAKE) waylandhelper.o

$(dep_dir)/ttra: | dependencies
	cd $(dep_dir) && git clone https://codeberg.org/aerkkila/ttra
$(dep_dir)/ttra/ttra.o: $(dep_dir)/ttra
	cd $< && $(MAKE) ttra.o

$(dep_dir)/colormap-headers: | dependencies
	cd $(dep_dir) && git clone https://codeberg.org/aerkkila/colormap-headers
$(dep_dir)/colormap-headers/cmh_colormaps.h: $(dep_dir)/colormap-headers
	cd $< && $(MAKE)

$(dep_dir):
	mkdir -p $@

get_dep: $(dep_get)

# end dependencies
# ================

_clean:
	rm -f *.out *.so *.o functions.c
	if [ -e $(dep_dir)/waylandhelper ]; then cd $(dep_dir)/waylandhelper && $(MAKE) clean; fi
	if [ -e $(dep_dir)/ttra ]; then cd $(dep_dir)/ttra && $(MAKE) clean; fi

clean: _clean
	@if [ -e $(dep_dir) ]; then printf "\e[93;1mDownloaded dependencies are not removed.\e[0m\n"; fi
	@if [ -e config.mk ];  then printf "\e[93;1mconfig.mk is not removed.\e[0m\n"; fi

dist-clean: _clean
	rm -rf $(dep_dir) config.mk

install: libcplot.so
	mkdir -p $(prefix)/include $(prefix)/lib
	cp cplot.h $(prefix)/include
	cp libcplot.so $(prefix)/lib

uninstall:
	rm -rf $(prefix)/include/cplot.h $(prefix)/lib/libcplot.so
	if [ -d $(prefix)/include ]; then rmdir -p --ignore-fail-on-non-empty $(prefix)/include fi
	if [ -d $(prefix)/lib ]; then rmdir -p --ignore-fail-on-non-empty $(prefix)/lib fi

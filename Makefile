include config.mk

CFLAGS += -Wall -g3 -gdwarf-2 -DHAVE_PNG
CFLAGS_OBJ = $(CFLAGS) -c
CFLAGS_LIB = $(CFLAGS) -shared -fpic
LDLIBS += -lm -lpng
OBJECTS =
kahto_sources = kahto.h kahto_rendering.c rotate.c functions.c ticker.c layout.c kahto_png.c kahto_draw_line.c kahto_draw_line_more.c kahto_draw_triangle.c kahto_colormesh.c kahto_init_markers.c Makefile
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
	kahto_sources += $(dep_dir)/colormap-headers/cmh_colormaps.h
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
	kahto_sources += kahto_wayland.c
endif

ifeq ($(use_ffmpeg), 1)
	LDLIBS += -lavcodec -lavutil -lavformat
	CFLAGS += -DHAVE_ffmpeg
	kahto_sources += kahto_video.c
endif

all: libkahto.so

tulosta:
	@echo dep_get: $(dep_get)
	@echo LDLIBS: $(LDLIBS)

testi.out: testi.c kahto.o $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

kahto.o: kahto.c $(kahto_sources) config.mk
	$(CC) $(CFLAGS_OBJ) -o $@ $<

libkahto.so: kahto.c $(kahto_sources) $(OBJECTS) config.mk
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

install: libkahto.so
	mkdir -p $(prefix)/include $(prefix)/lib
	cp kahto.h $(prefix)/include
	cp libkahto.so $(prefix)/lib

uninstall:
	rm -rf $(prefix)/include/kahto.h $(prefix)/lib/libkahto.so
	if [ -d $(prefix)/include ]; then rmdir -p --ignore-fail-on-non-empty $(prefix)/include; fi
	if [ -d $(prefix)/lib ]; then rmdir -p --ignore-fail-on-non-empty $(prefix)/lib; fi

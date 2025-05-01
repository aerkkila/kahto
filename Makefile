include config.mk

CFLAGS += -Wall -g -DHAVE_PNG
CFLAGS_OBJ = $(CFLAGS) -c
CFLAGS_LIB = $(CFLAGS) -shared -fpic
LDLIBS += -lm -lpng
OBJECTS =
cplot_sources = cplot.h cplot_rendering.c rotate.c functions.c ticker.c layout.c png.c Makefile
dep_dir = dependencies
dep_build =
dep_install =
dep_get =


ifeq ($(use_libttra), 1)
	LDLIBS += -lttra
else ifeq ($(use_libttra), 2)
	LDLIBS += -lttra
	dep_get += $(dep_dir)/ttra
	dep_build += $(dep_dir)/ttra/libttra.so
	dep_install += ttra_install
else # mandatory
	dep_get += $(dep_dir)/ttra
	dep_build += $(dep_dir)/ttra/libttra.so
	OBJECTS += $(dep_dir)/ttra/ttra.o
	LDLIBS += `pkg-config --libs freetype2` -lutf8proc -lfontconfig
	CFLAGS += -I$(dep_dir)/ttra
endif

ifeq ($(use_cmh_colormaps), 1)
else ifeq ($(use_cmh_colormaps), 2)
	dep_get += $(dep_dir)/colormap-headers
	dep_build += $(dep_dir)/colormap-headers
	dep_install += colormap-headers_install
else # mandatory
	dep_get += $(dep_dir)/colormap-headers
	dep_build += $(dep_dir)/colormap-headers
	cplot_sources += $(dep_dir)/colormap-headers/cmh_colormaps.h
	CFLAGS += -I$(dep_dir)/colormap-headers
endif

ifeq ($(use_libwaylandhelper), 1)
	LDLIBS += -lwaylandhelper
	CFLAGS += -DHAVE_wlh
else ifeq ($(use_libwaylandhelper), 2)
	LDLIBS += -lwaylandhelper
	dep_get += $(dep_dir)/waylandhelper
	dep_build += $(dep_dir)/waylandhelper/libwaylandhelper.so
	dep_install += wlh_install
	CFLAGS += -DHAVE_wlh
else ifeq ($(use_libwaylandhelper), 3)
	dep_get += $(dep_dir)/waylandhelper
	dep_build += $(dep_dir)/waylandhelper/libwaylandhelper.so
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
$(dep_dir)/waylandhelper: | $(dep_dir)
	cd $(dep_dir) && git clone https://codeberg.org/aerkkila/waylandhelper
$(dep_dir)/waylandhelper/waylandhelper.o: $(dep_dir)/waylandhelper
	cd $< && $(MAKE) waylandhelper.o
$(dep_dir)/waylandhelper/libwaylandhelper.so: $(dep_dir)/waylandhelper
	cd $< && $(MAKE)
wlh_install: $(dep_dir)/waylandhelper
	cd $< && $(MAKE) install

$(dep_dir)/ttra: | dependencies
	cd $(dep_dir) && git clone https://codeberg.org/aerkkila/ttra
$(dep_dir)/ttra/ttra.o: $(dep_dir)/ttra
	cd $< && $(MAKE) ttra.o
$(dep_dir)/ttra/libttra.so: $(dep_dir)/ttra
	cd $< && $(MAKE)
ttra_install: $(dep_dir)/ttra
	cd $< && $(MAKE) install

$(dep_dir)/colormap-headers: | dependencies
	cd $(dep_dir) && git clone https://codeberg.org/aerkkila/colormap-headers
$(dep_dir)/colormap-headers/cmh_colormaps.h: $(dep_dir)/colormap-headers
	cd $< && $(MAKE)
colormap-headers_install: $(dep_dir)/colormap-headers
	cd $< && $(MAKE) install

$(dep_dir):
	mkdir -p $@

install_dep: $(dep_install)
build_dep: $(dep_build)
get_dep: $(dep_get)

# end dependencies
# ================

clean:
	rm -f *.out *.so *.o functions.c
	if [ -e $(dep_dir)/waylandhelper ]; then cd waylandhelper && $(MAKE) clean; fi
	if [ -e $(dep_dir)/ttra ]; then cd ttra && $(MAKE) clean; fi
	@if [ -e $(dep_dir) ]; then printf "\e[93;1mDownloaded dependencies are not removed.\e[0m\n"; fi
	@if [ -e config.mk ];  then printf "\e[93;1mconfig.mk is not removed.\e[0m\n"; fi

install: libcplot.so
	mkdir -p $(prefix)/include $(prefix)/lib
	cp cplot.h $(prefix)/include
	cp libcplot.so $(prefix)/lib

uninstall_this:
	rm -rf $(prefix)/include/cplot.h $(prefix)/lib/libcplot.so
	rmdir -p --ignore-fail-on-non-empty $(prefix)/include $(prefix)/lib

uninstall: uninstall_this
	@if [ -e $(dep_dir) ]; then printf "\e[1mTo uninstall dependencies, run $(MAKE) uninstall_all\e[0m\n"; fi

virhe = printf "\e[91mError\e[0m\n"

uninstall_all: uninstall_this
	@cd $(dep_dir)/ttra && $(MAKE) uninstall || $(virhe)
	@cd $(dep_dir)/waylandhelper && $(MAKE) uninstall || $(virhe)
	@cd $(dep_dir)/colormap-headers && $(MAKE) uninstall || $(virhe)
	rmdir -p --ignore-fail-on-non-empty $(prefix)/include $(prefix)/lib

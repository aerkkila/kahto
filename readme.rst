A plotting library in C.
========================
See examples/ for more information.

Installation
------------
| $ ./configure.sh
| (Edit config.mk which was created, if necessary)
|
| (Install small dependencies:)
| $ make dep_get
| $ make dep_build
| # make dep_install
|
| (Build and install the library:)
| $ make
| # make install
|
| (To uninstall:)
| # make uninstall
| (To unistall dependencies as well:)
| # make uninstall-all

Dependencies
------------
- libpng
	- for cplot_write_png

- ffmpeg (optional) (libavcodec, libavformat, libavutil)
	- for cplot_write_mp4 to create a video

If not installed, these can be installed with 'make dep_get', etc.:

- libttra (https://codeberg.org/aerkkila/ttra)
	- for rendering text

- cmh_colomaps (https://codeberg.org/aerkkila/colormap-headers)
	- Very small, header-only library

- libwaylandhelper (optional) (https://codeberg.org/aerkkila/waylandhelper)
	- for cplot_show to show the figure on the screen (Wayland is needed)

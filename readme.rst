A plotting library in C.
========================
See examples/ for more information.

Installation
------------
| ./configure.sh
| make
| make install
|
| # To uninstall:
| make uninstall

After running configure.sh, you can edit config.mk, which the command generated. When running it, you can use non-default installation directory and c-compiler using environment variables 'prefix' and 'CC' respectively.

Other make commands
-------------------
| # To remove the compiled files:
| make clean
| # To remove everything which can be regenerated: compiled files, config.mk, dependencies:
| make dist-clean
| (To download the dependencies marked to be downloaded in config.mk:)
| $ make get_dep

Dependencies
------------
- libpng (optional)
	- for cplot_write_png

- ffmpeg (optional) (libavcodec, libavformat, libavutil)
	- for cplot_write_mp4 to create a video

If not installed, these will by default be downloaded and linked statically.

- libttra (https://codeberg.org/aerkkila/ttra)
	- for rendering text

- cmh_colomaps (https://codeberg.org/aerkkila/colormap-headers)
	- Very small, header-only library

- libwaylandhelper (optional) (https://codeberg.org/aerkkila/waylandhelper)
	- for cplot_show to show the figure on the screen (Wayland is needed)

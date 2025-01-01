#!/bin/sh

tmpfile=/tmp/cplot_configure_test
LIBS=
OBJS=
file=config.mk

isinstalled() {
	libs=
	headers=
	for a in $@; do
		if echo $a |grep -q "\.h$"; then
			headers="$headers#include <$a>
			"
		else
			libs="$libs -l$a"
		fi
	done
	gcc -o $tmpfile -x c $libs - 2>/dev/null <<-eof
		$headers
		int main() {}
	eof
}

make_dependency() {
	default=$1
	name=$2
	shift 2
	isinstalled $@ && arvo=1 || arvo=$default
	printf "use_$name = $arvo\n" >> $file
}

printf "# Automatically created by $0. Edit if necessary.\n%s\n\n"\
	"# If a value is not 1, that dependency was not found."\
	> $file

printf "# Needed by cplot_write_png\n" >> $file
make_dependency 0 libpng png
printf "# Needed by cplot_write_mp4\n" >> $file
make_dependency 0 ffmpeg avcodec avutil avformat
printf "\n# Small libraries. Here you have the following options:\n" >> $file
printf "#   0: Disables an optional dependency. Some dependencies are mandatory.\n" >> $file
printf "#   1: Already installed.\n" >> $file
printf "#   2: Download and install this.\n" >> $file
printf "#   3: Download but don't install this. Link statically to the library instead.\n" >> $file
printf "#      This might not be fully supported.\n" >> $file
make_dependency 2 libttra ttra
isinstalled wayland-client && default=2 || default=0
make_dependency $default libwaylandhelper waylandhelper
make_dependency 2 cmh_colormaps cmh_colormaps.h

rm -f $tmpfile

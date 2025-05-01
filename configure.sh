#!/bin/sh

# These can be given as environment variables.
[ "$CC" = "" ] && CC=cc
[ "$prefix" = "" ] && prefix=/usr/local

LIBS=
OBJS=
tmpfile=/tmp/cplot_configure_test
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
	$CC -o $tmpfile -x c $libs - 2>/dev/null <<-eof
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
printf "\n" >> $file
printf "prefix = %s\n" "$prefix" >> $file
printf "CC = %s\n" "$CC" >> $file

compiler_flag_works() {
	echo "" |$CC $1 -E -x c - >/dev/null 2>&1
}

found=
for std in gnu23 gnu2x; do
	if compiler_flag_works "--std=$std"; then
		printf "CFLAGS = --std=$std\n" >> $file
		found=1
		break
	fi
done
if [ ! "$found" ]; then
	echo "Warning: gnu23 not available. You may have to edit the source code."
fi

rm -f $tmpfile

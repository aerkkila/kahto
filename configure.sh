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
    name=$1
    shift
    if ! isinstalled $@; then
	printf "#" >> $file
    fi
    printf "use_$name = 1\n" >> $file
}

printf "# Automatically created by $0. Comment out those which you don't want.\n\n" > $file

printf "# Needed by cplot_write_png\n" >> $file
make_dependency libpng png
printf "# Needed by cplot_write_mp4\n" >> $file
make_dependency ffmpeg avcodec avutil avformat
printf "\n# Small libraries which will be downloaded and linked statically, if commented out.\n" >> $file
printf "# Comment these out, if you don't have them installed.\n" >> $file
make_dependency libttra ttra
make_dependency libwaylandhelper waylandhelper
make_dependency cmh_colormaps cmh_colormaps.h

rm -f $tmpfile

#!/bin/sh

tmpfile=/tmp/cplot_configure_test
LIBS=
OBJS=
file=config.mk

isinstalled() {
    libs=-l$1
    shift
    for a in $@; do
	libs="$libs -l$a"
    done
    gcc -o $tmpfile -x c $libs - 2>/dev/null <<-eof
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

printf "# Automatically created by $0. Comment out those which you don't want.\n" > $file

make_dependency libttra ttra
make_dependency libwaylandhelper waylandhelper
printf "# Needed by cplot_write_mp4\n" >> $file
make_dependency ffmpeg avcodec avutil avformat

rm -f $tmpfile

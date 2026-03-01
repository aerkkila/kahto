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
	success=$1
	default=$2
	name=$3
	shift 3
	isinstalled $@ && arvo=$success || arvo=$default
	printf "use_$name = $arvo\n" >> $file
}

printf "# Automatically created by $0. Edit if necessary.\n\n" > $file

printf "# Optional third party libraries. If value is no, library was not found.\n" >> $file
printf "# Enable cplot_write_png (yes, no)\n" >> $file
make_dependency yes no libpng png
printf "# Enable cplot_write_mp4 (yes, no)\n" >> $file
printf "# This becomes a separate object file to be linked with -lkahto-ffmpeg when necessary.\n" >> $file
printf "# Linking to ffmpeg libraries is slow, which is the reason for separating it.\n" >> $file
make_dependency yes no ffmpeg avcodec avutil avformat
printf "\n" >> $file

printf "# Small libraries. Here you have the following options:\n" >> $file
printf "#   no:     Disables the dependency, if optional.\n" >> $file
printf "#   yes:    Already installed. Will be linked dynamically.\n" >> $file
printf "#   static: Download this and link statically to the library.\n" >> $file
make_dependency yes static libttra ttra
make_dependency yes static cmh_colormaps cmh_colormaps.h
printf "# Optional. Enables cplot_show\n" >> $file
isinstalled wayland-client && default=static || default=no
make_dependency yes $default libwaylandhelper waylandhelper
printf "\n" >> $file

printf "prefix = %s\n" "$prefix" >> $file
printf "CC = %s\n" "$CC" >> $file

# try to preprocess an empty input with the compiler flag
compiler_flag_works() {
	echo "" |$CC -Werror $1 -E -x c - >/dev/null 2>&1
}

found=
cflags=
for std in gnu23 gnu2x; do
	if compiler_flag_works "--std=$std"; then
		cflags="--std=$std"
		found=1
		break
	fi
done
if [ ! "$found" ]; then
	echo "Warning: gnu23 not available. You may have to edit the source code."
fi

[ "$cflags" = "" ] && space="" || space=" "
if compiler_flag_works "-fpermissive"; then
	cflags="${cflags}${space}-fpermissive"
fi

printf "CFLAGS = %s\n" "$cflags" >> $file

rm -f $tmpfile

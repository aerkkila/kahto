#!/bin/sh

tmpfile=/tmp/cplot_configure_test
LIBS=
OBJS=

isinstalled() {
	gcc -o $tmpfile -x c -l$1 - 2>/dev/null <<-EOF
		int main() {}
	EOF
}

file=config.mk
printf "#Automatically created by $0. Comment out those which you don't want.\n" > $file

if ! isinstalled ttra; then
    printf "#" >> $file
fi
printf "use_libttra = 1\n" >> $file

if ! isinstalled waylandhelper; then
    printf "#" >> $file
fi
printf "use_libwaylandhelper = 1\n" >> $file

rm -f $tmpfile

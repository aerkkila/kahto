#!/bin/sh
for f in *.c; do
	make `basename -s .c $f`
done

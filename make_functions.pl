#!/bin/env perl
use warnings;
@enumtypes  = ('i1', 'u1', 'i2', 'u2', 'i4', 'u4',
	     'i8', 'u8', 'f4', 'f8', 'f10');
@ctypes   = ('char', 'unsigned char', 'short', 'unsigned short', 'int', 'unsigned',
             'long', 'long unsigned', 'float', 'double', 'long double');

sub main {
    open file_in, "<functions.in.c" or die;
    open file_out, ">functions.c";

    # Copy only once until @startperl.
    $line = <file_in>;
    while (not $line =~ /^\@startperl\s*.*/) { #*
	print file_out $line;
	$line = <file_in>;
	if (not $line) {
	    last; }
    }
    $startpos = tell file_in;

    for (my $i=0; $i<@enumtypes; $i++) {
	while (<file_in>) {
	    $_ =~ s/\@dtype/$enumtypes[$i]/g;
	    $_ =~ s/\$dtype/$ctypes[$i]/g;
	    print file_out $_;
	}
	print file_out "\n";
	seek(file_in, $startpos, 0);
    }

    my $first = 1;

    while (<file_in>) {
	($type, $name, $args) = ($_ =~ /(\S+.*)\s+(\S+)_\@dtype*\((.*)\)\s*{/g); #+
	if (!$type || !$name) {
	    next; }

	if (not $first) {
	    print file_out "\n"; }
	$first = 0;
	print file_out "$type (*${name}[])($args) = {\n";
	for (my $i=0; $i<@enumtypes; $i++) {
	    print file_out "    [cplot_$enumtypes[$i]] = ${name}_$enumtypes[$i],\n"; }
	print file_out "};\n";
    }
    close file_in;
}

main();

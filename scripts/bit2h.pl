#!/usr/bin/perl

use warnings;

die "\nUsage: $0 NAME\n\topens NAME.data (512x384) and writes NAME.h\n\n" if @ARGV <1;

(my $name = $ARGV[0]) =~ s/\.[^.]*$//;

open $BMAP, "$name\.data" or die "Bitmap File not found: $name.data";
open ($HFILE, '>', "${name}\.h") or die "Cound not open H file: $name.h";

# scale the pixels so that there's room for SYNC below BLACK
# NTSC in IRE units (+40): SYNC = 0; BLACK = 47.5; WHITE = 140
$BLACK = 255.0 / 140.0 * 47.5;
$SCALE = (255.0 - $BLACK) / 255.0;

$startInclude = 1;
printf $HFILE "#pragma once\nunsigned char %s[height][width] = {\n",$name;

while ( read $BMAP, $bytes, 512 ) {

	if ( $startInclude == 1 ) {
		$startInclude = 0;
	} else {
		printf $HFILE ",\n";
	}

	@pixels = unpack 'C*', $bytes;

	$startRow = 1;
	for (@pixels) { 
		if ( $startRow == 1 ) {
			$startRow = 0;
			printf $HFILE "{";
		} else {
			printf $HFILE ",";
		}
		printf $HFILE "0x%02X", $_ * $SCALE + $BLACK; 
	}
	printf $HFILE "}";
}
printf $HFILE "\n};\n";
close $BMAP;
close $HFILE;

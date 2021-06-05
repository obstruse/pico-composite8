#!/usr/bin/perl

use warnings;

@files = glob( '*data');

$dataCount = @files;

open ($FLASH, '>', "flash\.bin") or die "Could not open FLASH file: flash.bin";
binmode $FLASH;    # for those DOS people...

$count = pack 'C', $dataCount;
print $FLASH $count;

# scale the pixels so that there's room for SYNC below BLACK
# NTSC in IRE units (+40): SYNC = 0; BLACK = 47.5; WHITE = 140
$BLACK = 255.0 / 140.0 * 47.5;
$SCALE = (255.0 - $BLACK) / 255.0;

for ( @files ) {
	open $DATA, "$_" or die "Could not open DATA file: $_";
	while ( read $DATA, $bytes, 512 ) {
		@pixels = unpack 'C*', $bytes;
		@out    = map { $_ * $SCALE + $BLACK } @pixels;
		$bytes  = pack 'C*', @out;

		print $FLASH $bytes;
	}
}

close $FLASH;


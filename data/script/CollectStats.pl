#!/usr/bin/perl -w

BEGIN {
    use Cwd;
    our $directory = cwd;
}

use lib $directory;

require 'FighterStats.pl';
require 'QuickSave.pl';

sub RegisterFighter($)
{
	my ($reginfo) = @_;
	print "Registering: ", $reginfo->{ID}, "\n";
}

@chars = `ls ../characters/*.pl`;
chomp @chars;

print join (',',@chars), "\n";

foreach $char (@chars) {
	require $char;
}

while (($key,$val) = each %FighterStats ) {
    delete $val->{STATES};
    delete $val->{FRAMES};
    delete $val->{STARTCODE};
}

$allstats = store( \%::FighterStats );
$allstats =~ s/^{/\(/s;
$allstats =~ s/}$/\)/s;

open OUTPUT, ">CollectedStats.pl";
print OUTPUT "%::FighterStats = ";
print OUTPUT $allstats;
print OUTPUT ";";
close OUTPUT;

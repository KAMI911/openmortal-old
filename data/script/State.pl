
use strict;

sub ParseConfig($)
{
	my ($filename) = @_;
	my ($escaped1, $escaped2);
	open CONFFILE, "$filename" or return;
	
	while (<CONFFILE>)
	{
		if ($_ =~ /(\w+)\s*=\s*(.*\S)\s*/)
		{
			$escaped1 = $1;
			$escaped1 =~ s/'/\\'/g;
			$escaped2 = $2;
			$escaped2 =~ s/'/\\'/g;
			eval( "\$::$escaped1 = '$escaped2';" );
		}
	}
	
	close CONFFILE;
}


return 1;

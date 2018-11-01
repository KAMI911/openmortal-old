
use strict;


$::RewindInterwal = 10;			# Save every 10th game tick
$::RewindMax = 200;				# Must be a multiple of $RewindInterwal
%::RewindData = ();


sub ResetRewind()
{
	%::RewindData = ();
}


sub MakeRewindData()
{
	my ( $doodadrewind, $doodad, $rewinddata );
	
	$doodadrewind = [];
	foreach $doodad (@::Doodads) {
		push @{$doodadrewind}, ( $doodad->RewindData() );
	}

	$rewinddata = {
		gametick		=> $::gametick,
		fighter1		=> $::Fighter1->RewindData(),
		fighter2		=> $::Fighter2->RewindData(),
		doodads			=> $doodadrewind,

		bgx				=> $::bgx,
		bgy				=> $::bgy,
		bgspeed			=> $::BgSpeed,
		bgposition		=> $::BgPosition,
		over			=> $::over,
		ko				=> $::ko,

		quakeamplitude	=> $::QuakeAmplitude,
		quakeoffset		=> $::QuakeOffset,
		quakex			=> $::QuakeX,
		quakey			=> $::QuakeY,

		input1			=> $::Input1->RewindData(),
		input2			=> $::Input2->RewindData(),
	};

	$::RewindData{$::gametick} = $rewinddata;
	delete $::RewindData{$::gametick - $::RewindMax};
}



sub RewindTo($)
{
	my ($rewindto) = @_;
	my ($rewinddata);

	$rewindto = (int($rewindto / $::RewindInterwal) * $::RewindInterwal);		#/
	
	$rewinddata = $::RewindData{$rewindto};
	return unless defined $rewinddata;

	$::gametick		= $rewinddata->{gametick};
	$::Fighter1		= $rewinddata->{fighter1};
	$::Fighter2		= $rewinddata->{fighter2};
	@::Doodads		= @{$rewinddata->{doodads}};

	$::bgx			= $rewinddata->{bgx};
	$::bgy			= $rewinddata->{bgy};
	$::BgSpeed		= $rewinddata->{bgspeed};
	$::BgPosition	= $rewinddata->{bgposition};
	$::over			= $rewinddata->{over};
	$::ko			= $rewinddata->{ko};

	$::QuakeAmplitude = $rewinddata->{quakeamplitude};
	$::QuakeOffset	= $rewinddata->{quakeoffset};
	$::QuakeX		= $rewinddata->{quakex};
	$::QuakeY		= $rewinddata->{quakey};

	$::Input1		= $rewinddata->{input1};
	$::Input2		= $rewinddata->{input2};
}

1;

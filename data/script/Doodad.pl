=comment

Doodad members are:

T		int			The Doodad's type. 0 means text.
OWNER	int			The Doodad's owner (0 for player 1, 1 for player 2, other means no owner)
HOSTILE	int			1 if the doodad can collision with players and damage them.
LIFETIME int		The amount of time before this doodad dies. -1 means infinite.

POS		(int,int)	The Doodad's logical position.
SIZE	(int,int)	The Doodad's physical size.
SPEED	(int,int)	The Doodad's logical speed.
ACCEL	(int,int)	The Doodad's logical acceleration.
DIR		int			1: heading right; -1: heading left (implies flipped state)

GFXOWNER int		The Doodad's graphics owner (0 for player 1, 1 for player 2, -1 means global)
FIRSTFRAME int		The first frame of the doodad (only meaningful if GFXOWNER is a player).
FRAMES	int			The number of frames.
SA		int			Animation speed
F		int			The Doodad's frame.
TEXT	string		The text displayed in a text doodad.

INITCODE sub		This will run then the doodad is created.
UPDATECODE sub		This will be ran instead of MoveDoodad if defined.



Doodad types are:
0		Text
1		Zoli's shot
2		UPi's shot
3		UPi's explosion
4		UPi's familiar
5		Tooth

=cut

BEGIN {
    use Cwd;
    our $directory = cwd;
}

use lib $directory;

use strict;

require 'Collision.pl';


package Doodad;


%Doodad::DoodadDefinitions = (
'ZoliShot' => {
	'T'  => 1,					'HOSTILE' => 1,				'LIFETIME' => -1,
	'SIZE'	=> [ 64, 64 ],		'SPEED'	=> [ 48, -25 ],		'ACCEL'	=> [ 0, 2 ],

	'GFXOWNER' => -1,			'FIRSTFRAME' => 0,			'FRAMES' => 6,
	'SA' => 1/10,
},

'Tooth' => {
	'T' => 5,					'HOSTILE' => 0,				'LIFETIME' => -1,
	'SIZE'	=> [ 24, 24 ],		'SPEED' => [ 20, -20 ],		'ACCEL' => [ 0, 2 ],
	'GFXOWNER' => -1,			'FIRSTFRAME' => 0,			'FRAMES' => 12,
	'SA' => 1/4,
	'INITCODE' => sub {
		my ($self) = @_;
		$self->{SA} = $self->{SA} * $self->{DIR};
	},
	'UPDATECODE' => sub {
		my ($self) = @_;
		if ( $self->{POS}->[1] > (440 * $::GAMEBITS2) ) {
			$self->{SPEED}->[1] = -abs( $self->{SPEED}->[1] / 2 );
			$self->{POS}->[1] = 439 * $::GAMEBITS2;
		}
		MoveDoodad( $self );
	},
},

'UPiShot' => {
	'T' => 2,					'HOSTILE' => 1,				'LIFETIME' => -1,
	'SIZE' => [170, 22],		'SPEED' => [ 48, 0 ],		'ACCEL' => [ 0, 0 ],
	
	'GFXOWNER' => 0,			'FIRSTFRAME' => 340,		'FRAMES' => 5,
	'SA' => 1/5,
},

'UPiExplosion' => {
	'T' => 3,					'HOSTILE' => 0,				'LIFETIME' => 30,
	'SIZE' => [ 58, 80],		'SPEED' => [ 0, 0 ],		'ACCEL' => [ 0, 0 ],
	
	'GFXOWNER' => 0,			'FIRSTFRAME' => 345,		'FRAMES' => 15,
	'SA' => 1/2,
},

'UPiFamiliar' => {
	'T' => 4,					'HOSTILE' => 0,				'LIFETIME' => -1,
	'SIZE' => [ 20, 20],		'SPEED' => [ 0, 0 ],		'ACCEL' => [ 0, 0 ],
	
	'GFXOWNER' => 0,			'FIRSTFRAME' => 360,		'FRAMES' => 3,
	'SA' => 1/10,				
	
	'ANGLE' => 0,
	'UPDATECODE' => sub {
		my ($self) = @_;
		my ($fighter, $frame, $head, $targetposx, $targetposy);
		
		$fighter = $::Fighters[$self->{OWNER}];
		$frame = $fighter->{FRAMES}->[$fighter->{FR}];
		$head = $frame->{head};

		$self->{ANGLE} += 0.05 * $self->{DIR};
		$targetposx = $fighter->{X} + $head->[0] * $::GAMEBITS2 * $fighter->{DIR} + sin( $self->{ANGLE} ) * 300;
		$targetposy = $fighter->{Y} + $head->[1] * $::GAMEBITS2 + cos( $self->{ANGLE} ) * 300;
		
		if ( $fighter->{HP} > 0 )
		{
			$self->{ACCEL}->[0] = ($targetposx - $self->{POS}->[0] ) / 200;
			$self->{ACCEL}->[1] = ($targetposy - $self->{POS}->[1] ) / 200;
			$self->{SPEED}->[0] += $self->{ACCEL}->[0];
			$self->{SPEED}->[1] += $self->{ACCEL}->[1];
			$self->{SPEED}->[0] *= 0.95;
			$self->{SPEED}->[1] *= 0.95;
		}
		
		#$self->{SPEED}->[0] = ($targetposx - $self->{POS}->[0]) / 30;
		#$self->{SPEED}->[1] = ($targetposy - $self->{POS}->[1]) / 30;
		$self->{POS}->[0] += $self->{SPEED}->[0];
		$self->{POS}->[1] += $self->{SPEED}->[1];

		$self->{F} += $self->{SA};
		$self->{F} -= $self->{FRAMES} if $self->{F} > ($self->{FRAMES} + $self->{FIRSTFRAME});

		return 0;
	},
},

);


=comment

CreateDoodad creates a new doodad, and appends it to the global doodad list,
@Doodads. The parameters are:
	x		int		The logical horizontal position of the center of the doodad
	y		int		The logical vertical position of the center of the doodad
	t		string	The type of the doodad (doodad types are described above)
	dir		int		1 or -1
	owner	int		The number of the player who 'owns' the doodad (0 or 1, anything else means no owner)

=cut

sub CreateDoodad
{
	my ($x, $y, $t, $dir, $owner) = @_;
	my ($doodad, $doodaddef, $w, $h);
	
	$doodaddef = $Doodad::DoodadDefinitions{$t};
	
	if ( ( not defined $doodaddef ) and $t != 0 )
	{
		print "CreateDoodad: Doodad $doodaddef doesn't exist!\n";
		return;
	}
	
	if ( defined $doodaddef )
	{
		$w = $doodaddef->{SIZE}->[0];
		$h = $doodaddef->{SIZE}->[1];
		$t = $doodaddef->{T};
	}
	else
	{
		$w = $h = $t = 0;
		$doodaddef = {};
	}
	
	$doodad = {
		'T'			=> $t,
		'OWNER'		=> $owner,
		'HOSTILE'	=> 0,
		'LIFETIME'	=> -1,
		
		'POS'		=> [ $x - $w * $::GAMEBITS2 / 2, $y - $h * $::GAMEBITS2 / 2 ],
		'SIZE'		=> [ $w, $h ],
		'SPEED'		=> [ 0, 0 ],
		'ACCEL'		=> [ 0, 0 ],
		'DIR'		=> $dir,
		
		'GFXOWNER'	=> $owner,
		'FIRSTFRAME'=> 0,
		'FRAMES'	=> 1,
		'SA'		=> 0,
		'F'			=> 0,
		'TEXT'		=> '',
		%{$doodaddef},
	};
	
	$doodad->{SIZE}  = [@{$doodaddef->{SIZE}} ] if defined $doodaddef->{SIZE};
	$doodad->{SPEED} = [@{$doodaddef->{SPEED}}] if defined $doodaddef->{SPEED};
	$doodad->{ACCEL} = [@{$doodaddef->{ACCEL}}] if defined $doodaddef->{ACCEL};
	$doodad->{F}	 = $doodad->{FIRSTFRAME};
	
	$doodad->{SPEED}->[0] *= $doodad->{DIR};
	$doodad->{ACCEL}->[0] *= $doodad->{DIR};
	$doodad->{GFXOWNER} = $owner if $doodad->{GFXOWNER} >= 0;
	
	if ( exists $doodad->{INITCODE} )
	{
		# Call the updatecode.
		&{$doodad->{INITCODE}}( $doodad );
	}
	
	push @::Doodads, ($doodad);
	return $doodad;
}



sub CreateTextDoodad
{
	my ($x, $y, $owner, $text) = @_;
	my ($self);
	$self = CreateDoodad($x, $y, 0, 0, $owner);
	$self->{'TEXT'} = $text;
	$self->{'GFXOWNER'} = -1;
	return $self;
}



sub RewindData
{
	my ($self) = @_;

	return {
		%{$self},
		POS			=> [ @{$self->{POS}} ],
		SIZE		=> [ @{$self->{SIZE}} ],
		SPEED		=> [ @{$self->{SPEED}} ],
		ACCEL		=> [ @{$self->{ACCEL}} ],
	}
}


=comment
UpdateDoodad is called once every game tick.

It should return 0 if the doodad is still active.
If UpdateDoodad returns nonzero, it will be removed in this tick and
deleted.
=cut

sub UpdateDoodad
{
	my ($doodad) = @_;
	
	if ( exists $doodad->{UPDATECODE} )
	{
		# Call the updatecode.
		return &{$doodad->{UPDATECODE}}( $doodad );
	}
	else
	{
		return MoveDoodad( $doodad );
	}
}


sub MoveDoodad
{
	my ($doodad) = @_;
	
	if ( $doodad->{LIFETIME} >= 0 )
	{
		$doodad->{LIFETIME} -= 1;
		if ( $doodad->{LIFETIME} < 0 )
		{
			# Doodad dies.
			return 1;
		}
	}

	$doodad->{SPEED}->[0] += $doodad->{ACCEL}->[0];
	$doodad->{SPEED}->[1] += $doodad->{ACCEL}->[1];
	$doodad->{POS}  ->[0] += $doodad->{SPEED}->[0];
	$doodad->{POS}  ->[1] += $doodad->{SPEED}->[1];
	
	$doodad->{F} += $doodad->{SA};
	$doodad->{F} -= $doodad->{FRAMES} if $doodad->{F} > ($doodad->{FRAMES} + $doodad->{FIRSTFRAME});
	$doodad->{F} += $doodad->{FRAMES} if $doodad->{F} < $doodad->{FIRSTFRAME};

	if ( $doodad->{POS}->[0] > $::BGWIDTH2 ) { return 1; }
	if ( $doodad->{POS}->[0] < $doodad->{SIZE}->[0] * $::GAMEBITS2 ) { return 1; }
	if ( $doodad->{POS}->[1] > $::SCRHEIGHT2 ) { return 1; }
	if ( $doodad->{POS}->[1] < $doodad->{SIZE}->[1] * $::GAMEBITS2 ) { return 1; }
	
	if ( $doodad->{HOSTILE} )
	{
		CheckDoodadHit($doodad);
	}
	
#	print "Doodad: POS=", join(',', @{$doodad->{POS}}), 
#		  "; SPEED=", join(',', @{$doodad->{SPEED}}), 
#		  "; ACCEL=", join(',', @{$doodad->{ACCEL}}), "\n";
	
	return 0;
}


sub CheckDoodadHit($)
{
	my ( $self ) = @_;

	my ( @poly, $x, $y, $w, $h, $i, $fighter );
	$x = $self->{POS}->[0] / $::GAMEBITS2;
	$y = $self->{POS}->[1] / $::GAMEBITS2;
	$w = $self->{SIZE}->[0];
	$h = $self->{SIZE}->[1];
	
	for ( $i=0; $i<$::NUMPLAYERS; ++$i )
	{
		next if $i == $self->{OWNER};
		$fighter = $::Fighters[$i];
		
		@poly = (
			$x, $y,
			$x+$w, $y,
			$x+$w, $y+$h,
			$x, $y+$h );

		if ( $fighter->IsHitAt( \@poly ) )
		{
			DoHitPlayer($self, $fighter);
		}
	}
	
}


sub DoHitPlayer($$)
{
	my ($self,$player) = @_;

	$self->{HOSTILE} = 0;
	$player->HitEvent( $::Fighters[$self->{OWNER}], 'Hit', $self->{T} );
}
=cut

return 1;


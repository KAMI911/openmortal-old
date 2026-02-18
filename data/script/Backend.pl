#!/usr/bin/perl -w

# PERL backend for the game

=comment

This is the main perl backend for Mortal Szombat. It loads the other files,
compiles characters, creates shared variables.



1. FIGHTER ACTIONS

1.1. BASIC MOVEMENT        1.2. OFFENSIVE MOVES         1.3. HURT MOVES

Start                      KneelingPunch                Falling
Stand                      KneelingKick                 Laying
Walk                       KneelingUppercut             Getup
Back                       HighPunch                    HighPunched
Turn                       LowPunch                     LowPunched
Hurt                       HighKick                     GroinKicked
Block                      LowKick                      KneelingPunched
Kneeling                   Sweep                        KneelingKicked
Onknees                    GroinKick                    Thrown
Jump                       KneeKick                     Dead
JumpFW                     Elbow
JumpBW                     Grenade
Fun                        Uppercut
Threat                     Throw
                           [fighter-specific stuff]
                           
=cut


=comment

MAIN CONSTANTS

=cut

use FindBin;
use lib "$FindBin::Bin";

sub InitMainConstants($$)
{
	my ($wide, $numplayers) = @_;
	
	$MAXPLAYERS	= 4;							# The maximal number of players in the game
	$WIDE		= $wide;
	$NUMPLAYERS	= $numplayers;					# Number of players in the game
	$GAMEBITS	= 3;							# Number of oversampling bits in coordinates
	$GAMEBITS2	= 1 << $GAMEBITS;				# 2^(oversampling)
	$SCRWIDTH	= $wide ? 800 : 640;			# The physical horizontal screen resolution
	$SCRHEIGHT	= 480;							# The physical vertical screen resolution
	$SCRWIDTH2	= ($SCRWIDTH << $GAMEBITS);		# The logical horizontal screen resolution
	$SCRHEIGHT2	= ($SCRHEIGHT << $GAMEBITS);	# The logical vertical screen resolution
	$MOVEMARGIN2= 50 << $GAMEBITS;				# The logical border for fighters.

	$BGWIDTH2	= 1920 << $GAMEBITS;			# The logical background width
	$BGHEIGHT2	= 480 << $GAMEBITS;				# The logical background height

	$GROUND2	= 160 << $GAMEBITS;				# The logical ground level.
	$DELMULTIPLIER = 1;							# DEL values in states are multiplied by this.
	$MAXCOMBO	= 5;							# Maximum combo count.

	$BgMax		= $BGWIDTH2 - $SCRWIDTH2;		# The logical maximum camera position
	$BgSpeed	= 0;							# The current speed of the background movement
	$BgPosition	= $BgMax >> 1;					# The logical camera position
	$BgScrollEnabled = 1;						# Can the background scroll?
	$HitPointScale = 10;						# Scale the hit points by this.
	$NextDoodad = 0;							# The next doodad to process
	$Debug		= 0;
}

InitMainConstants( 0, 2 );

require 'PlayerInput.pl';
require 'Fighter.pl';
require 'FighterStats.pl';
require 'Doodad.pl';
require 'Keys.pl';
require 'State.pl';
require 'Translate.pl';
require 'Rewind.pl';



=comment

MAIN OBJECTS

=cut

sub CreateFighters()
{
	my ( $i, $fighter );
		
	@Fighters = ();
	
	for ( $i=0; $i<$MAXPLAYERS; ++$i )
	{
		$fighter = Fighter->new();
		$fighter->{NUMBER} = $i;
		$fighter->{OTHER} = $fighter;
		$Fighters[$i] = $fighter;
	}
	
	$Fighters[1]->{OTHER} = $Fighters[0];
	$Fighters[0]->{OTHER} = $Fighters[1];
}


CreateFighters();
CreatePlayerInputs();



=comment

VARIABLES FOR DRAWING THE SCENE

=cut

$gametick = 0;	# Number of Advance()'s called since the start.
$p1x = 10;		# Player 1 X position
$p1y = 100;		# Player 1 Y position
$p1f = 10;		# Player 1 frame
$p1h = 100;		# Player 1 HP (the displayed bar)
$p1hreal = 100;	# Player 1 real HP (actual value, not the displayed bar)

$p2x = 400;		# Player 2 X position
$p2y = 100;		# Player 2 Y position
$p2f = -40;		# Player 2 frame. Negative means flipped.
$p2h = 100;		# Player 2 HP
$p2hreal = 100;	# Player 2 real HP (actual value, not the displayed bar)

$bgx = 0;		# Background X position
$bgy = 0;		# Background Y position
$over = 0;		# Is the game over? Used to end the game / player selection.
$ko = 0;		# Is one fighter knocked down? Used for instant replay, to capture the moment of the KO. Also disables player input / connections


$doodad_x = -1;			# Doodad X position
$doodad_y = -1;			# Doodad Y position
$doodad_t = -1;			# Doodad type
$doodad_f = -1;			# Doodad frame number
$doodad_dir = 0;		# The direction of the doodad; 1: normal; -1: flipped
$doodad_gfxowner = -1;	# 0: first player; 1: second player; -1: Global doodad
$doodad_text = '';		# The text of type 0 doodads.



=comment
ResetGame clears 'most' game variables. It should not be called by the 
frontend directly, instead it is used by the various starting functions,
e.g. "SelectStart" or "JudgementStart".
=cut

sub ResetGame($$$)
{
	my ($bgposition, $wide, $numplayers) = @_;

	InitMainConstants( $wide, $numplayers );
	
	$gametick		= 0;
	$over			= 0;
	$ko				= 0;
	
	$bgx			= 0;
	$bgy			= 0;
	$BgSpeed		= 0;
	$BgPosition		= $bgposition;
	$BgScrollEnabled = $bgposition != 0;
	
	$ActiveFighters	= $numplayers;		# Number of fighters not in the Win2 / Dead states.
	$ActiveTeams	= $numplayers;		# Number of teams not in the Win2 / Dead states or have more team members.
	$OverTimer		= 0;
	$JudgementMode	= 0;
	$Debug			= 0;
	
	@Doodads = ();
	@Sounds = ();
	
	ResetEarthquake();
	ResetRewind();

	my ($fighter, $input);
	foreach $fighter (@Fighters)
	{
		$fighter->Reset if $fighter->{OK};
		$fighter->{TEAMSIZE} = 1;
	}

	foreach $input (@Inputs)
	{
		$input->Reset();
	}
}


=comment

JUDGEMENT METHODS

=cut


sub JudgementStart($)
{
	ResetGame(0, 0, 2);
	$JudgementMode = 1;
	($JudgementWinner) = @_;
	
	my ($i);
	for ( $i=0; $i<$NUMPLAYERS; ++$i )
	{
		$Fighters[$i]->{X} =  ($JudgementWinner!=$i ? 150 : 520 ) * $GAMEBITS2;
		$Fighters[$i]->{DIR} = ($JudgementWinner!=$i ? 1 : -1 );
		$Fighters[$i]->{NEXTST} = 'Stand';
		$Fighters[$i]->Update();
	}
}


=comment

PLAYER SELECTION METHODS 

=cut

sub SelectStart($)
{
	my ( $numplayers ) = @_;
	ResetGame(0,0,$numplayers);
	my ( $i, $fighter);
	
	$ActiveTeams = 10000;		# Make sure we don't trigger an accidental 'Won' event...
	
	if ( 2 == $NUMPLAYERS )
	{
		for ( $i=0; $i<$NUMPLAYERS; ++$i )
		{
			if ($Fighters[$i]->{OK})
			{
				$Fighters[$i]->{X} = (100 + 440*$i) * $GAMEBITS2;
				$Fighters[$i]->{NEXTST} = 'Stand';
				$Fighters[$i]->{DIR} = $i ? -1 : 1;
				$Fighters[$i]->Update();
			}
		}
	}
	else
	{
		foreach $fighter (@Fighters)
		{
			next unless $fighter->{OK};
			last if $fighter->{NUMBER} == $NUMPLAYERS;
			$fighter->{X} = $MOVEMARGIN2 + $SCRWIDTH2 / $NUMPLAYERS * $fighter->{NUMBER};
			$fighter->{DIR} = 1;
			$fighter->{NEXTST} = 'Start';
			$fighter->Update();
		}
	}
}


sub SetPlayerNumber
{
	my ($player, $fighterenum) = @_;
	my ($f);
	
	DeleteDoodadsOf( $player );
	
	$f = $Fighters[$player];
	return unless defined $f;
	
	$f->Reset($fighterenum);
	$f->{NEXTST} = 'Stand';
	$f->Update();
	
	GetFighterStats($fighterenum);
	$::PlayerName = $::Name;
	$over = 0;
	$OverTimer = 0;
	
	if ( 2 != $NUMPLAYERS )
	{
		$f->{X} = $MOVEMARGIN2 + $SCRWIDTH2 / $NUMPLAYERS * $f->{NUMBER};
		$f->{DIR} = 1;
	}
}


sub PlayerSelected
{
	my ($number) = @_;
	my ($f) = $Fighters[$number];
	
	$f->{NEXTST} = 'Stand';
	$f->Update();
	$f->Event( 'Won' );
	$f->Update();
	$over = 0;
	$OverTimer = 1;
}



=comment

EARTHQUAKE RELATED METHODS

=cut



@QUAKEOFFSET = ( 0,  6, 11, 15, 
				16, 15, 11,  6, 
				 0, -6,-11,-15,
			   -16,-15,-11, -6, 0, 6 );


sub ResetEarthquake
{
	$QuakeAmplitude = 0;
	$QuakeOffset = 0;
	$QuakeX = 0;
	$QuakeY = 0;
}


sub AddEarthquake
{
	my ($amplitude) = @_;
	$QuakeAmplitude += $amplitude;
	$QuakeAmplitude = 20 if ( $QuakeAmplitude > 20 );
}


sub AdvanceEarthquake
{
	if ( $QuakeAmplitude <= 0.2 )
	{
		$QuakeAmplitude = $QuakeX = $QuakeY = 0;
		return;
	}
	
	$QuakeAmplitude -= $QuakeAmplitude / 30 + 0.1;
	$QuakeOffset = ( $QuakeOffset + 1 ) % 16;
	$QuakeX = $QUAKEOFFSET[$QuakeOffset] * $QuakeAmplitude / 16;
	$QuakeY = $QUAKEOFFSET[$QuakeOffset + 1] * $QuakeAmplitude / 16;	# 1/1
	
	$bgx -= $QuakeX;
	$bgy -= $QuakeY;
	$p1x += $QuakeX;
	$p1y += $QuakeY;
	$p2x += $QuakeX;
	$p2y += $QuakeY;
	$p3x += $QuakeX;
	$p3y += $QuakeY;
	$p4x += $QuakeX;
	$p4y += $QuakeY;
	
	# Do not quake doodads for now.
}



=comment

GAME BACKEND METHODS

=cut

sub GameStart($$$$$)
{
	my ( $MaxHP, $numplayers, $teamsize, $wide, $debug ) = @_;
	
	ResetGame( $BgMax >> 1, $wide, $numplayers );

	$::MaxHP = $MaxHP;
	
	$bgx = ( $SCRWIDTH2 - $BGWIDTH2) >> 1;
	$bgy = ( $SCRHEIGHT2 - $BGHEIGHT2 ) >> 1;
	$BgScrollEnabled = 1;
	$HitPointScale = 1000 / $MaxHP;			# 1/1
	$Debug = $debug;
	
	my ($fighter);
	foreach $fighter (@Fighters)
	{
		$fighter->{TEAMSIZE} = $teamsize;
		$fighter->{HP} = $MaxHP;
	}
	
	$p1h = $p2h = 0;
}


=comment
NextTeamMember releases the next team member of the given team. This method
is called by the frontend when a fighter dies in team mode. The new fighter
will be released someplace "safe". The team size of the given player will
be decremented.

Input parameters:
$player			0 or 1, the player who has his current fighter replaced
$fighterenum	The new fighter who will replace the current figther.

=cut

sub NextTeamMember($$)
{
	my ($player, $fighterenum) = @_;
	
	my ($fighter) = $Fighters[$player];
	my ($otherx, $oldx);
	
	-- $fighter->{TEAMSIZE};
	$oldx = $fighter->{X};
	
	SetPlayerNumber( $player, $fighterenum );
	$fighter->{HP} = $::MaxHP;
	$over = 0;
	$ko = 0;
	$OverTimer = 0;
	
	if ( $NUMPLAYERS == 2 )
	{
		$otherx = $Fighters[1-$fighter->{NUMBER}]->{X};
		if ( $otherx < $BGWIDTH2 / 2 )
		{
			$fighter->{X} = $otherx + $SCRWIDTH2 ;#- $MOVEMARGIN2 * 6;
			$fighter->{DIR} = -1;
		}
		else
		{
			$fighter->{X} = $otherx - $SCRWIDTH2 ;#+ $MOVEMARGIN2 * 6;
			$fighter->{DIR} = +1;
		}
	
		if ( abs( $fighter->{X} - $oldx ) > $SCRWIDTH2 / 2 ) 
		{
			$fighter->{DELPENALTY} = 100;
		}
	}
	else
	{
		if ($BgPosition < $BgMax2/2)
		{
			$fighter->{X} = $BgPosition + $SCRWIDTH2 + $MOVEMARGIN2;
			$fighter->{DIR} = -1;
		}
		else
		{
			$fighter->{X} = $BgPosition - $MOVEMARGIN2;
			$fighter->{DIR} = 1;
		}
	}
	
	$fighter->{BOUNDSCHECK} = 0;
	$fighter->{NEXTST} = 'JumpStart';
	$fighter->Update();
}



=comment

Takes a Fighter object, and returns the data relevant for the frontend.

Parameters:
$fighter	The fighter which is to be converted to frontend data.
$hp			The displayed hit points

Returns:
$x			The physical X coordinate of the fighter
$y			The physical Y coordinate of the fighter
$fnum		The frame number of the fighter. A negative sign means the
			fighter is flipped horizontally (facing left)
$hp			The displayed hit points
$hpreal		The real HP of the given player

=cut

sub GetFighterData($$)
{
	my ($fighter, $hp) = @_;
	my ($x, $y, $fnum, $f);
	
	$fnum = $fighter->{FR};
	$f = $fighter->{FRAMES}->[$fnum];
	$x = $fighter->{X} / $GAMEBITS2 + $f->{'x'};
	$y = $fighter->{Y} / $GAMEBITS2 + $f->{'y'};
	
	if ($fighter->{DIR}<0)
	{
		$fnum = -$fnum;
		$x -= $f->{x}*2 + $f->{w};
	}
	
	$phTarget = $fighter->{HP}*$HitPointScale;
	if ( $hp < $phTarget ) { $hp += 5; }
	if ( $hp > $phTarget ) { $hp -= 3; }
	$hp = $phTarget if abs($hp - $phTarget) < 3;
	$hp = 0 if $hp < 0;	
	
	return ($x, $y, $fnum, $hp, $fighter->{HP});
}



sub GetNextDoodadData
{
	if ( $NextDoodad >= scalar @Doodads )
	{
		$doodad_x = $doodad_y = $doodad_t = $doodad_f = $doodad_dir = $doodad_gfxowner = -1;
		$doodad_text = '';
		return;
	}

	my ($doodad) = $Doodads[$NextDoodad];
	$doodad_x = $doodad->{POS}->[0] / $GAMEBITS2 - $bgx;
	$doodad_y = $doodad->{POS}->[1] / $GAMEBITS2 - $bgy;
	$doodad_t = $doodad->{T};
	$doodad_f = $doodad->{F};
	$doodad_dir = $doodad->{DIR};
	$doodad_gfxowner = $doodad->{GFXOWNER};
	$doodad_text = $doodad->{TEXT};
	
	
	if ($doodad_gfxowner >=0 )
	{
		$doodad_x += $Fighters[$doodad_gfxowner]->{FRAMES}->[$doodad_f]->{'x'} * $doodad_dir;
		$doodad_y += $Fighters[$doodad_gfxowner]->{FRAMES}->[$doodad_f]->{'y'};
	}
	
	++$NextDoodad;
}



sub UpdateDoodads
{
	my ($i, $j, $doodad);
	
	for ($i=0; $i<scalar @Doodads; ++$i)
	{
		$doodad = $Doodads[$i];
		$j = Doodad::UpdateDoodad( $doodad );
		if ( $j )
		{
			# Remove this Doodad
			splice @Doodads, $i, 1;
			--$i;
		}
	}
}



sub DeleteDoodadsOf($)
{
	my ($owner) = @_;
	my ($i, $doodad);
	for ($i=0; $i<scalar @Doodads; ++$i)
	{
		$doodad = $Doodads[$i];
		if ( $doodad->{OWNER} == $owner )
		{
			# Remove this Doodad
			splice @Doodads, $i, 1;
			--$i;
		}
	}
}



sub GetNextSoundData()
{
	# print "GetSoundData: ", scalar @Sounds, join ' ,', @Sounds, "\n";
	$sound = pop @Sounds;
	$sound = '' unless defined $sound;
}


sub DoFighterHitEvent($$$)
{
	my ($fighter, $other, $hit) = @_;
	
	$fighter->HitEvent( $other, $other->GetCurrentState()->{HIT}, $hit );
}



sub DoFighterEvents($)
{
	my ($fighter) = @_;
	
	if ( $JudgementMode )
	{
		if ( $fighter->{NUMBER} == $JudgementWinner )
		{
			$fighter->Event("Won");
		}
		else
		{
			$fighter->Event("Hurt");
		}
		return;
	}
	
	if ( $NUMPLAYERS ==2 and $fighter->IsBackTurned )
	{
		$fighter->Event("Turn");
	}
	if ( $ActiveTeams <= 1 )
	{
		$fighter->Event("Won");
	}
	if ( $fighter->{IDLE} > 150 )
	{
		my ($r) = $fighter->QuasiRandom( 353 );
		$fighter->Event("Fun") if $r==1;
		$fighter->Event("Threat") if $r==2;
	}
}




sub GameAdvance
{
	my ( @hits, @playerhit, $i, $j, $fighter, $input);
	
	$gametick += 1;
	
	$NextDoodad = 0;
	$NextSound = 0;
	
	# 1. ADVANCE THE PLAYERS
	
	for ( $i=0; $i<$NUMPLAYERS; ++$i ) {
		$input = $Inputs[$i];
		$fighter = $Fighters[$i];
		$input->Advance();
		$fighter->Advance( $input ) if $fighter->{OK};
		$hit[$i] = 0;
	}
	
	@hits = ();
	for ( $i=0; $i<$NUMPLAYERS; ++$i ) {
		push @hits, ( $Fighters[$i]->CheckHit() );
	}
		
	# 2. Events come here
	
	foreach $hit (@hits) {
		print STDERR "hits: $hit->[0] $hit->[1] $hit->[2]\n";
		DoFighterHitEvent( $hit->[1], $hit->[0], $hit->[2] );
	}
	
	for ( $i=0; $i<$NUMPLAYERS; ++$i ) {
		$fighter = $Fighters[$i];
		DoFighterEvents( $fighter ) if $fighter->{OK};
	}
	
	UpdateDoodads();
	
	for ( $i=0; $i<$NUMPLAYERS; ++$i ) {
		$fighter = $Fighters[$i];
		$fighter->Update() if $fighter->{OK};
	}
	
	if ( $OverTimer == 0 and $ActiveTeams <= 0 )
	{
		$OverTimer = 1;
	}
	elsif ( $OverTimer > 0 )
	{
		$OverTimer++;
		$over = 1 if ( $OverTimer > 200 )
	}
	
	# 3. DO THE BACKGROUND SCROLLING THING

	if ( $BgScrollEnabled )
	{
		$BgOpt = 0;
		for ( $i=0; $i<$NUMPLAYERS; ++$i ) { $BgOpt += $Fighters[$i]->{X}; }
		$BgOpt /= $NUMPLAYERS;		# 1/1 Stupid kdevelop syntax hightlight.
		if ( ($BgOpt - $BgSpeed*50 - $BgPosition) > ($SCRWIDTH2 / 2) ) { $BgSpeed++; }
		if ( ($BgOpt - $BgSpeed*50 - $BgPosition) < ($SCRWIDTH2 / 2) ) { $BgSpeed--; }
	
		$BgPosition+=$BgSpeed;
		if ($BgPosition<0) { $BgPosition = $BgSpeed = 0; }
		if ($BgPosition>$BgMax) { $BgPosition = $BgMax; $BgSpeed = 0; }
		
		# print "Pos:$BgPosition\tOpt:$BgOpt\tSpd:$BgSpeed\t";
	}
	
	# 4. SET GLOBAL VARIABLES FOR THE C++ DRAWING TO READ
	
	($p1x, $p1y, $p1f, $p1h, $p1hreal) = GetFighterData( $Fighters[0], $p1h );
	($p2x, $p2y, $p2f, $p2h, $p2hreal) = GetFighterData( $Fighters[1], $p2h ) if ($NUMPLAYERS >= 2);
	($p3x, $p3y, $p3f, $p3h, $p3hreal) = GetFighterData( $Fighters[2], $p3h ) if ($NUMPLAYERS >= 3);
	($p4x, $p4y, $p4f, $p3h, $p4hreal) = GetFighterData( $Fighters[3], $p4h ) if ($NUMPLAYERS >= 4);
	
	$bgx = $BgPosition >> $GAMEBITS;
	$p1x -= $bgx;
	$p2x -= $bgx;
	$p3x -= $bgx;
	$p4x -= $bgx;
	$bgy = 0;
	
	AdvanceEarthquake();
	
	# 5. DEBUG POLYGONS
	
	if ( $Debug )
	{
		$fr1 = $Fighter1->{FRAMES}->[ $Fighter1->{FR} ];
		@p1head = @{ $fr1->{head} };
		MirrorPolygon( \@p1head ) if $Fighter1->{DIR} < 0;
		OffsetPolygon( \@p1head,  $Fighter1->{X} / $GAMEBITS2 - $bgx, $Fighter1->{Y} / $GAMEBITS2 - $bgy );
		@p1body = @{ $fr1->{body} };
		MirrorPolygon( \@p1body ) if $Fighter1->{DIR} < 0;
		OffsetPolygon( \@p1body,  $Fighter1->{X} / $GAMEBITS2 - $bgx, $Fighter1->{Y} / $GAMEBITS2 - $bgy );
		@p1legs = @{ $fr1->{legs} };
		MirrorPolygon( \@p1legs) if $Fighter1->{DIR} < 0;
		OffsetPolygon( \@p1legs,  $Fighter1->{X} / $GAMEBITS2 - $bgx, $Fighter1->{Y} / $GAMEBITS2 - $bgy );
		if ( defined $fr1->{hit} )
		{
			@p1hit = @{ $fr1->{hit} };
			MirrorPolygon( \@p1hit ) if $Fighter1->{DIR} < 0;
			OffsetPolygon( \@p1hit,  $Fighter1->{X} / $GAMEBITS2 - $bgx, $Fighter1->{Y} / $GAMEBITS2 - $bgy );
		}
		else
		{
			undef @p1hit;
		}
		
		$fr2 = $Fighter2->{FRAMES}->[ $Fighter2->{FR} ];
		@p2head = @{ $fr2->{head} };
		MirrorPolygon( \@p2head ) if $Fighter2->{DIR} < 0;
		OffsetPolygon( \@p2head,  $Fighter2->{X} / $GAMEBITS2 - $bgx, $Fighter2->{Y} / $GAMEBITS2 - $bgy );
		@p2body = @{ $fr2->{body} };
		MirrorPolygon( \@p2body ) if $Fighter2->{DIR} < 0;
		OffsetPolygon( \@p2body,  $Fighter2->{X} / $GAMEBITS2 - $bgx, $Fighter2->{Y} / $GAMEBITS2 - $bgy );
		@p2legs = @{ $fr2->{legs} };
		MirrorPolygon( \@p2legs) if $Fighter2->{DIR} < 0;
		OffsetPolygon( \@p2legs,  $Fighter2->{X} / $GAMEBITS2 - $bgx, $Fighter2->{Y} / $GAMEBITS2 - $bgy );
		if ( defined $fr2->{hit} )
		{
			@p2hit = @{ $fr2->{hit} };
			MirrorPolygon( \@p2hit ) if $Fighter2->{DIR} < 0;
			OffsetPolygon( \@p2hit,  $Fighter2->{X} / $GAMEBITS2 - $bgx, $Fighter2->{Y} / $GAMEBITS2 - $bgy );
		}
		else
		{
			undef @p2hit;
		}
	}

}



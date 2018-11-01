# DataHelper contains subroutines useful for loading a character's
# frames, and creating his states.

BEGIN {
    use Cwd;
    our $directory = cwd;
}

use lib $directory;

use strict;

require 'FighterStats.pl';



=comment


SITUATIONS ARE:

Ready, Stand, Crouch, (Midair = any + character is flying), Falling



SITUATION DEPENDENT EVENTS ARE:

Highhit, Uppercut, Hit, Groinhit, Leghit, Fall



STANDBY EVENTS ARE:

Won, Hurt, Threat, Fun, Turn




________|___Ready___________Block___Stand___________Crouch______________Midair______Falling
Highhit	|	HighPunched		-		HighPunched		KneelingPunched 	(...)		(...)
Uppercut|	Falling			-		Falling			KneelingPunched		(...)		(...)
Hit		|	LowPunched		-		LowPunched		KneelingKicked		
Groinhit|	GroinKicked		-		GroinKicked		KneelingKicked		
Leghit	|	Swept			-		Swept			KneelingKicked		
Fall	|	Falling			-		Falling			KneelingPunched



FRAME MEMBER DESCRIPTION IS:

x		int		X coordinate offset of the image relative to the character's anchor.
y		int		Y coordinate offset of the image relative to the character's anchor.
w		int		The width of the image.
h		int		The height of the image.
head	array	The coordinates of a polygon marking the head within the image, relative to the anchor.
body	array	The coordinates of a polygon marking the body within the image, relative to the anchor.
legs	array	The coordinates of a polygon marking the legs within the image, relative to the anchor.
hit		array	The coordinates of a polygon marking the hit within the image, relative to the anchor.


STATE MEMBER DESCRIPTION IS:

F		int		The number of the visible frame.
SITU	string	The situation associated with this state (Ready, Stand, Crouch, Falling)
DEL		int		The delay before moving to the next state.
NEXTST	string	The name of the state which follows this state, if none of the CONs is used.
CON		hash	Connections from this state. The keys are either events or keyboard input.
HIT		?		The hit delivered at the beginning of this state.
BLOCK	int		If true, the character is blocking in his current state.
MOVE	int		The character's anchor should continously move this much during this state.
DELTAX	int		The character's anchor should immediately change by this much after this state.
PUSHX	int		The character is pushed, with this much momentum upon entering this state.
TURN	int		If true, the character's facing direction changes after this state.
JUMP	int		The character leaps into the air, with this much initial momentum upon entering this state.
DOODAD	string	A doodad is created at the beginning of this state. The string contains the doodad's type and speed.
SOUND	string	The sound effect associated with this state (if any);
HITSND	string	The sound effect if the HIT is successful.
MISSSND	string	The sound effect if the HIT fails.
CODE	string	This code will be evaled at the beginning of this state.
LAYER	int		The "priority" of the graphics. 5: hurt; 10: block; 15: kneeling; 20: normal; 25: attack


=cut




# Loads the frame data (x, y, w, h) from the given datafile.
# The path to the datafile is inserted automatically. The frame data will
# be shifted by (-PivotX,-PivotY).
#
# Returns an array of frame data. The first element in the array is
# a dummy entry, the second is the first real entry. This is because
# the first thing in the datafile is a PAL entry.
#
# Example: LoadFrames( "ZOLIDATA.DAT.txt" );
sub LoadFrames ($$$)
{
	my ($DataName, $PivotX, $PivotY) = @_;
	my (@Frames, $data, $frame, $DatName);

	# Make sure that Whatever.dat also exists.
	$DatName = $DataName;
	$DatName =~ s/\.txt$//;
	open DATFILE, "../data/characters/$DatName" || die ("Couldn't open ../data/characters/$DatName");
	close DATFILE;
	
	open DATAFILE, "../data/characters/$DataName" || die ("Couldn't open ../data/characters/$DataName");
	$data = '';
	while ( read DATAFILE, $data, 16384, length($data) )
	{
	}
	close DATAFILE;

	print "$DataName file is ", length($data), " bytes long.\n";
	
	# Insert a dummy first row for the palette entry
	eval ("\@Frames = ( {}, $data);");
	die $@ if $@;
	
	foreach $frame (@Frames)
	{
		OffsetFrame( $frame, -$PivotX, -$PivotY );
	}
	print "$DataName loaded, ", scalar @Frames, " frames.\n";
	
	return @Frames;
}


# Creates a frame lookup from a descriptor array.
# The first parameter is the expected number of frames.
# The descriptor array should have frame base names in even positions,
# and lengths at odd. positions. For example:
# ("start", 6, "stand", 4, ... )
#
# The routine will return a lookup which will contain the frame's logical
# name as a key, and its physical index as a value. The logical names are
# simply the basename plus a number. The example above would return:
# ("start1"=>1, "start2"=>2, ..., "start6"=>6, "stand1"=>6, "stand2"=>7, ...)
sub CreateFrameLookup
{
	my ($ExpectedCount, @FrameDesc) = @_;
	
	my ($FrameName, $NumFrames);
	my ($i, $j);
	my (%FrameLookup);
	
	for ( $i=0; $i<scalar @FrameDesc; $i +=2 )
	{
		$FrameName = $FrameDesc[$i];
		$NumFrames = $FrameDesc[$i+1];
		
		for ( $j = 1; $j<=$NumFrames; ++$j )
		{
			# print "Frame ", (scalar keys %FrameLookup) + 1, " is now called $FrameName$j\n";
			print "Name redefinition: $FrameName!\n" if defined $FrameLookup{ "$FrameName$j" };
			$FrameLookup{ "$FrameName$j" } = (scalar keys %FrameLookup) + 1;
		}
	}
	
	if ( $ExpectedCount !=  scalar keys( %FrameLookup ) )
	{
		die( "Expected number of frames ($ExpectedCount) doesn't equal the actual number of frames: ".
			scalar keys(%FrameLookup) );
	}
	
	return %FrameLookup;
}



# Helper function. Finds the last frame with a given name in a frame
# lookup structure. Return the index of the last frame (1-based), or
# 0 if none with the given name were found.
#
# Example: If there are 6 "highpunch" frames (highpunch1 to highpunch6),
# FindLastFrame(\%FrameLookup, "highpunch") returns 6.
sub FindLastFrame($$) {
	my ($FrameLookup, $FrameName) = @_;
	my ($i) = (1);
	while ( exists ${$FrameLookup}{"$FrameName$i"} ) { $i++; }
	return $i-1;
}



sub OffsetPolygon($$$)
{
	my ($poly, $dx, $dy) = @_;
	my ($i, $n);

	$n = scalar @{$poly};

	for ( $i=0; $i < $n; $i+=2 )
	{
		$poly->[$i] += $dx;
		$poly->[$i+1] += $dy;
	}
}



sub MirrorPolygon($)
{
	my ($poly) = @_;
	my ($i, $n);

	$n = scalar @{$poly};

	for ( $i=0; $i < $n; $i+=2 )
	{
		$poly->[$i] = - $poly->[$i];
	}
}


sub GetPolygonCenter($)
{
	my ($poly) = @_;
	
	my ($i, $n, $x, $y);
	
	$n = scalar @{$poly};
	$x = $y = 0;
	for ( $i=0; $i < $n; $i+=2 )
	{
		$x += $poly->[$i];
		$y += $poly->[$i+1];
	}
	
	return ( $x*2/$n, $y*2/$n );
}



sub OffsetFrame($$$) {
	my ($frame, $dx, $dy) = @_;
	
	$frame->{'x'} += $dx;
	$frame->{'y'} += $dy;

	OffsetPolygon( $frame->{head}, $dx, $dy ) if defined ($frame->{head});
	OffsetPolygon( $frame->{body}, $dx, $dy ) if defined ($frame->{body});
	OffsetPolygon( $frame->{legs}, $dx, $dy ) if defined ($frame->{legs});
	OffsetPolygon( $frame->{hit}, $dx, $dy )  if defined ($frame->{hit});
}



# FindLastState returns the last index of a given state.
# For example, if Punch4 is the last in Punch, FindLastState("Punch") is 4.
sub FindLastState($$) {
	my ( $States, $StateName ) = @_;
	my ( $i ) = ( 1 );
	
	while ( exists ${$States}{ "$StateName $i" } ) { $i++; }
	return $i-1;
}


# Translates an abbreviated sequence to a full sequence.
# "-punch" is every punch frame backwards.
# "_punch" is every punch frame except the last one backwards.
# "+punch" is every punch frame forwards.
sub TranslateSequence($$) {
	my ($FrameLookup, $Sequence) = @_;
	
	my ($pre, $frame) = $Sequence =~ /^([+-_]{0,1})(\w+)/;
	my ($LastFrame) = (FindLastFrame( $FrameLookup, $frame ) );
	#$LastFrame = (FindLastFrame( $FrameLookup, "$pre$frame" ) ) if $LastFrame == 0;
	#print "Last frame of $frame is $LastFrame.\n";

	return "$frame 1-$LastFrame" if ( $pre eq '+' );
	return "$frame $LastFrame-1" if ( $pre eq '-' );
	return "$frame " . ($LastFrame-1) . "-1" if ( $pre eq '_' );

	$Sequence =~ s/\sn(-{0,1})/ $LastFrame$1/;	# Replace n- with last frame
	$Sequence =~ s/-n/-$LastFrame/;				# Replace -n with last frame
	return $Sequence;
}



sub SetStateData($$$)
{
	my ($state, $FrameDesc, $suffix) = @_;
	
	$state->{DEL} = $FrameDesc->{"DEL$suffix"} if defined $FrameDesc->{"DEL$suffix"};
	$state->{HIT} = $FrameDesc->{"HIT$suffix"} if defined $FrameDesc->{"HIT$suffix"};
	$state->{CON} = $FrameDesc->{"CON$suffix"} if defined $FrameDesc->{"CON$suffix"};
	$state->{BLOCK} = $FrameDesc->{"BLOCK$suffix"} if defined $FrameDesc->{"BLOCK$suffix"};
	$state->{NEXTST} = $FrameDesc->{"NEXTST$suffix"} if defined $FrameDesc->{"NEXTST$suffix"};
	$state->{MOVE} = $FrameDesc->{"MOVE$suffix"} if defined $FrameDesc->{"MOVE$suffix"};
	$state->{DELTAX} = $FrameDesc->{"DELTAX$suffix"} if defined $FrameDesc->{"DELTAX$suffix"};
	$state->{PUSHX} = $FrameDesc->{"PUSHX$suffix"} if defined $FrameDesc->{"PUSHX$suffix"};
	$state->{TURN} = $FrameDesc->{"TURN$suffix"} if defined $FrameDesc->{"TURN$suffix"};
	$state->{JUMP} = $FrameDesc->{"JUMP$suffix"} if defined $FrameDesc->{"JUMP$suffix"};
	$state->{SITU} = $FrameDesc->{"SITU$suffix"} if defined $FrameDesc->{"SITU$suffix"};
	$state->{DOODAD} = $FrameDesc->{"DOODAD$suffix"} if defined $FrameDesc->{"DOODAD$suffix"};
	$state->{SOUND} = $FrameDesc->{"SOUND$suffix"} if defined $FrameDesc->{"SOUND$suffix"};
	$state->{CODE} = $FrameDesc->{"CODE$suffix"} if defined $FrameDesc->{"CODE$suffix"};
}



# Adds a sequence to the end of a state
# Sequences are: e.g. "throw 10-14, throw 16, throw 14-10"
# Each piece of the sequence will have $Delay delay.
sub AddStates($$$) {
	my ( $States, $Frames, $FrameDesc ) = @_;
	my ( $StateName, $SequenceString, $LastState, $i, $sloop, $s, @Sequences );
	my ( $from, $to, $frame, $state );

	$StateName = $FrameDesc->{'N'};
	$SequenceString = $FrameDesc->{'S'};
	$FrameDesc->{SITU} = 'Stand' unless defined $FrameDesc->{SITU};
	$LastState = FindLastState($States,$StateName)+1;

	@Sequences  = split ( /\s*,\s*/, $SequenceString );
	for ( $sloop = 0; $sloop < scalar @Sequences; ++$sloop )
	{
		$s = TranslateSequence( $Frames, $Sequences[$sloop] );
		#print "Sequence is $s\n";
		
		if ( $s =~ /^\s*(\w+)\s+(\d+)-(\d+)\s*$/ )
		{
			# Sequence is '<frame> <from>-<to>'
			$frame = $1;
			$from = $2;
			$to = $3;
		}
		elsif ( $s =~ /^\s*(\w+)\s+(\d+)\s*$/ )
		{
			# Sequence is '<frame> <number>'
			$frame = $1;
			$from = $to = $2;
		}
		else
		{
			die "Sequence '$s' incorrect.\n";
		}

		$i = $from;
		while (1)
		{
			die "Error: Frame $frame$i doesn't exist.\n"
				unless defined ${$Frames}{"$frame$i"};

			$state = { 'F'=>"$frame$i" };
			SetStateData( $state, $FrameDesc, '' );
			SetStateData( $state, $FrameDesc, $LastState );
			if ( ( $sloop == scalar @Sequences -1 ) and ( $i == $to ) )
			{
				SetStateData( $state, $FrameDesc, 'N' );
			}
			
			$States->{"$StateName $LastState"} = $state;
			# print "Added state '$StateName $LastState' as frame '$frame$i', delay $Delay\n";
			
			$LastState++;
			if ( $from < $to )
			{
				$i++;
				last if $i > $to;
			}
			else
			{
				$i--;
				last if $i < $to;
			}
		}

	}
}


sub BlockStates($$)
{
	my ( $frames, $del) = @_;
	my ( $retval, $i );

	# We need to make sure that blocking is the same speed for every character.
	# Typical is 5 frames, +- 1 frame
	$del = int( 25 / $frames );			# 1/1
	
	$retval = { 'N'=>'Block', 'DEL'=>$del, 'S'=>'+block', };
	for ($i = 1; $i <= $frames; ++$i )
	{
		$retval->{"NEXTST$i"} = "Block " . ($i-1);
		$retval->{"CON$i"} = { 'block'=> "Block " . ($i+1) };
		$retval->{"BLOCK$i"} = 1 if $i*$del > 10;
	}
	
	$retval->{'NEXTST1'} = 'Stand';
	$retval->{"CON$frames"} = { 'block'=> "Block " . $frames };
	
	return $retval;
}


sub KneelingStates($$$$)
{
	my ( $frames, $frames2, $del, $con ) = @_;
	my ( $retval, $retval2, $i, $j );

	$retval = { 'N'=>'Kneeling', 'DEL'=> $del, 'S' => '+kneeling', 'SITU'=>'Crouch' };
	for ( $i = 1; $i <= $frames; ++$i )
	{
		$retval->{"NEXTST$i"} = "Kneeling " . ($i-1);
		$retval->{"CON$i"} = { 'down' => "Kneeling " . ($i+1) };
	}
	$retval->{'NEXTST1'} = 'Stand';
	$retval->{"CON$frames"} = { 'down' => "Onknees" };

	$retval2 = { 'N'=>'Onknees', 'DEL'=>$del, 'S' => '+onknees,-onknees', 'SITU'=>'Crouch',
		'NEXTST' => "Kneeling $frames" };
	$frames2 *= 2;
	for ( $i = 1; $i <= $frames2; ++$i )
	{
		$j = ($i % $frames2) + 1;
		$retval2->{"CON$i"} = { %{$con}, 'down'=>"Onknees $j", 'forw'=>"Onknees $j", 'back'=>"Onknees $j" };
	}

	return ($retval, $retval2);
}



=comment
JumpStates is for generating the Jump, JumpFW, JumpBW, JumpFly, 
JumpStart, JumpKick, JumpPunch states for a state description list.

Parameters:
$frames		hash	The frame lookup hash.
$con		hash	Connections during jumping (usually, JumpKick and JumpPunch only)
$framenames	hash	[optional] If the standard frame names (kneeling, 
					onknees, kneelingkick, kneelingpunch) are not good, this has should
					contain replacement names (e.g. 'kneelingkick' => 'sweep')
=cut

sub JumpStates
{
	my ( $frames, $con, $framenames ) = @_;
	my ( $kneelingframes, $onkneesframes,
		$kickframes, $punchframes ) = (
			FindLastFrame( $frames, 'kneeling' ),
			FindLastFrame( $frames, 'onknees' ),
			FindLastFrame( $frames, 'kneelingkick' ),
			FindLastFrame( $frames, 'kneelingpunch' ) );
	my ( $jumpheight ) = 120;

	my ( $i, $j, $statestotal, $statesdown, $statesknees, $deldown,
		$jump, $jumpfw, $jumpbw, $flying, $flyingsequence, $flyingstart, $jumpkick, $jumppunch );

	# The jump's first part is going down on knees, second part is
	# on knees, third part is getting up.

	if ( $::DELMULTIPLIER )
	{
		$statestotal = $jumpheight * 2 / 3 / $::DELMULTIPLIER; 				# 1/1
	}
	else
	{
		$statestotal = $jumpheight * 2 / 3;
	}
	$statesdown  = $statestotal / 4;
	
	$deldown = int($statesdown / $kneelingframes + 0.1);			# 1/1
	$statesdown  = $deldown * $kneelingframes;
	$statesknees = $statestotal - $statesdown * 2;

	$jump = { 'N'=>'Jump', 'DEL'=> $deldown, 'S'=>'kneeling 1-2, kneeling 1',
		'JUMPN'=>$jumpheight, NEXTSTN=>'JumpFly', 'SOUND1'=>'PLAYER_JUMPS', };
	$jumpfw = { %{$jump}, 'N'=>'JumpFW', 'PUSHX3'=>18*16 };
	$jumpbw = { %{$jump}, 'N'=>'JumpBW', 'PUSHX3'=>-9*16 };
	
	$flyingsequence = '';
	$flying = {};
	for ( $i = 0; $i < $statesknees / $deldown; ++$i )				#1/1
	{
		$j = $i + $statesdown / $deldown;							#1/1
		$flyingsequence .= 'onknees 1,';
		$flying->{"CON$j"} = $con;
		# $flying->{"DEL$j"} = 1;
	}
	$flyingsequence = "+kneeling, $flyingsequence -kneeling";
	
	$flying = { %{$flying}, 'N'=>'JumpFly', 'DEL'=> $deldown, 'S'=>$flyingsequence,
		'DELN'=>100 };
	$flyingstart = { 'N'=>'JumpStart', 'JUMP2'=>$jumpheight, 'PUSHX2'=>9*16, 'DEL1'=>1, 
		'DEL'=> $deldown, 'S'=>"stand 1,$flyingsequence", 'DELN'=>100 };
	
	print join( ',', %{$flying}), "\n";
	
	$jumpkick = { 'N'=>'JumpKick', 'HIT'=>'Fall',
		'DEL'=> int( $statestotal * 2 / 3 / ( $kickframes + $kneelingframes*2 + 3 ) ),		# 1/1
		'S'=> '+kneelingkick,kneelingkick n, kneelingkick n, kneelingkick n,-kneelingkick,-kneeling',
		'HIT'=>'Fall', 'DELN'=>100 };
	
	$jumppunch = { 'N'=>'JumpPunch', 'HIT'=>'Highhit',
		'DEL'=> int( $statestotal * 2 / 3 / ( $punchframes + $kneelingframes*2 + 3 ) ),		# 1/1
		'S'=> '+kneelingpunch,kneelingpunch n, kneelingpunch n, kneelingpunch n,-kneelingpunch,-kneeling',
		'HIT'=>'Fall', 'DELN'=>100 };

	return ($jump, $jumpfw, $jumpbw, $flying, $flyingstart, $jumpkick, $jumppunch);
}



sub WalkingFrames($$$$$)
{
	my ( $frameLookup, $frameArray, $preFrames, $distance, $con ) = @_;

	my ( $walkFrames, $totalFrames, $seq, $seq2, $distPerFrame,
		$walk, $back, $walkNextst, $backNextst,
		$i, $j, );

	$totalFrames = FindLastFrame( $frameLookup, 'walk' );
	$walkFrames = $totalFrames - $preFrames;

	if ( $preFrames > 0 ) {
		$seq = "+walk, walk $preFrames-1";
		$seq2 = "walk 1-$preFrames, -walk";
	} else {
		$seq = "+walk";
		$seq2 = "-walk";
	}
	
	$walk = { 'N'=>'Walk', 'S'=>$seq, 'DEL'=>5, 'CON'=>$con };
	$back = { 'N'=>'Back', 'S'=>$seq2, 'DEL'=>5, };

	# Add attributes for the 'pre' states.

	for ( $i=1; $i <= $preFrames; ++$i )
	{
		$j = $i + 1;
		$walk->{"CON$i"} = { %{$con}, 'forw' => "Walk $j" };
		$walk->{"NEXTST$i"} = 'Stand';
		$back->{"CON$i"} = { %{$con}, 'back' => "Back $j" };
		$back->{"NEXTST$i"} = 'Stand';
	}

	# Add attributes for the 'walk' states.
	
	$walkNextst = $preFrames ? 'Walk ' . ($totalFrames+1) : 'Stand';
	$backNextst = $preFrames ? 'Back ' . ($totalFrames+1) : 'Stand';
	$distPerFrame = $distance / $walkFrames;							# 1/1

	print "*** $preFrames $walkFrames $totalFrames $walkNextst $backNextst\n";

	for ( $i=$preFrames+1; $i <= $totalFrames; ++$i )
	{
		$j = ($i == $totalFrames) ? $preFrames+1 : $i+1;
		$walk->{"MOVE$i"} = 4;
		$walk->{"NEXTST$i"} = $walkNextst;
		$walk->{"CON$i"} = { %{$con}, 'forw' => "Walk $j" };
		$back->{"MOVE$i"} = -4;
		$back->{"NEXTST$i"} = $backNextst;
		$back->{"CON$i"} = { %{$con}, 'back' => "Back $j" };
		
		OffsetFrame( $frameArray->[$frameLookup->{"walk$i"}],
			- ($i-$preFrames-1) * $distPerFrame, 0 );
	}

	return ( $walk, $back );
}



sub TravelingStates( $$$$$$ )
{
	my ( $frameLookup, $frameArray, $states, $frameName, $from, $to ) = @_;
	
	$from = 1 unless $from;
	unless ( $to )
	{
		$to = FindLastFrame( $frameLookup, $frameName );
		$to += 1 if $frameName eq 'falling';
	}

	my ( $fromIndex, $toIndex, $fromFrame, $toFrame, $fromOffset, $toOffset,
		$deltax, $i, $state, $nextst );
	
	# 1. Calculate the 'deltax' and 'fromOffset'.
	
	$fromIndex = $frameLookup->{"$frameName$from"};
	die "couldn't find frame $frameName$from" unless defined $fromIndex;
	$toIndex = $fromIndex - $from + $to;
	
	$fromFrame = $frameArray-> [ $fromIndex ];
	$toFrame = $frameArray-> [ $toIndex ];

	$fromOffset = $fromFrame->{x} + ($fromFrame->{w} >> 1);
	$toOffset = $toFrame->{x} + ($toFrame->{w} >> 1);
	$deltax = ( $toOffset - $fromOffset ) / ( $to - $from );	#1/1

	# print "Offsets: $fromOffset $toOffset $deltax\n";

	# 2. Offset every relevant frame.
	for ( $i=$fromIndex; $i<=$toIndex; ++$i )
	{
		# print "Offsetting frame $i by ", - $fromOffset - $deltax * ($i-$fromIndex), "\n";
		OffsetFrame( $frameArray->[$i],
			- $fromOffset - $deltax * ($i-$fromIndex), 0 );
	}

	# 3. Apply deltax to every relevant state.
	while ( ($i, $state) = each %{$states} )
	{
		if ( $state->{F} >= $fromIndex and $state->{F} <= $toIndex )
		{
			$nextst = $states->{$state->{NEXTST}};
			if ( defined($nextst) and $nextst->{F} >= $fromIndex and $nextst->{F} <= $toIndex )
			{
				$state->{DELTAX} = $deltax * ($nextst->{F} - $state->{F});
				# print "Fixing state $i : deltax = ", $state->{DELTAX}, "\n";
			}
		}
	}
}



sub FixStates($$)
{
	my ( $frameLookup, $states ) = @_;

	my ( $framename, $st, $lastchar, $key, $value, $nextchar, $nextst );
	
	while (($key, $value) = each %{$states})
	{
		$framename = $value->{'F'};
		unless ( $framename =~/^\d+$/ )
		{
			# Convert non-numeric frames to their numeric counterparts.
			die "Can't find image $framename in frame $key" unless defined $frameLookup->{ $framename };
			$value->{'F'} = $frameLookup->{ $framename };
		}

		($st,$lastchar) = $key =~ /(\w+)\s+(\d+)/;

		unless ( defined $value->{'NEXTST'} )
		{
			$nextchar = $lastchar + 1;
			$nextst = "$st $nextchar";
			unless ( defined $states->{$nextst} ) {
				# print "Go to Standby after $key\n";
				$nextst = 'Stand';
			}
			$value->{'NEXTST'} = $nextst;
		}
		
	}
}


sub FindShorthands($)
{
	my ( $states ) = @_;

	my ( $key, $value, $st, $lastchar, %Shorthands );
	
	while (($key, $value) = each %{$states})
	{
		($st,$lastchar) = $key =~ /(\w+)\s+(\d+)/;
		print "$key has no lastchar" unless defined $lastchar;
		if ( $lastchar == 1 )
		{
			$Shorthands{$st} = $states->{$key};
		}
	}
	
	return %Shorthands;
}


sub CheckStates($$)
{
	my ( $fightername, $states ) = @_;
	my ( $key,$state, $con );
	my ( $seq,$nextst );
	
	while (($key, $state) = each %{$states})
	{
		die "Bad connection in fighter $fightername to '$state->{NEXTST} from $key!'" unless exists $states->{ $state->{NEXTST} };
		next unless $state->{CON};
		$con = $state->{CON};

		while (($seq, $nextst) = each %{$con})
		{
			die "Bad connection in fighter $fightername to '$nextst' from $key!" unless exists $states->{$nextst};
		}
	}
}





return 1;

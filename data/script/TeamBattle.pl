package TeamBattle;

# =============================================================================
# TeamBattle.pl  —  Multi-character battle coordination (2v2, 3v3)
#
# Extends the existing team system (which already handles sequential teams)
# with:
#   - Per-tick team AI coordination (avoid simultaneous attacks, target select)
#   - Dynamic camera bounds calculation for all fighters
#   - Optional tag-out system for switching active fighter
#
# Backend.pl calls TeamBattle::Tick() once per game tick when NUMPLAYERS > 2.
# =============================================================================

# ---------------------------------------------------------------------------
# Team assignment helpers
# ---------------------------------------------------------------------------

# Maps fighter NUMBER → team index.
# Team 0 = players 0,2  (left side / good)
# Team 1 = players 1,3  (right side / evil)
sub GetTeam {
    my ($fighter_num) = @_;
    return $fighter_num % 2;
}

sub AreAllies {
    my ($a, $b) = @_;
    return GetTeam($a) == GetTeam($b);
}

# ---------------------------------------------------------------------------
# Target selection — choose the best opponent for an AI fighter to focus
# ---------------------------------------------------------------------------

# Returns the fighter object that $ai_fighter should target.
# Priority: lowest HP opponent → closest opponent → first alive opponent.
sub SelectTarget {
    my ($ai_fighter) = @_;
    my $my_team = GetTeam($ai_fighter->{NUMBER});
    my ($best, $best_score);

    for my $i (0 .. $::NUMPLAYERS - 1) {
        my $f = $::Fighters[$i];
        next unless defined $f && $f->{OK} && $f->{ST} ne 'Dead';
        next if GetTeam($i) == $my_team;   # skip allies

        # Score: lower HP + closer distance is higher priority
        my $dist  = abs($f->{X} - $ai_fighter->{X});
        my $score = (1.0 - ($f->{HP} / ($::MaxHP || 100))) * 10
                  + (1.0 - ($dist / ($::SCRWIDTH2 || 5120)));
        if (!defined $best || $score > $best_score) {
            $best       = $f;
            $best_score = $score;
        }
    }
    return $best;
}

# Update the OTHER pointer for all AI fighters so they face the right target.
# In 1v1 mode Backend.pl hardcodes OTHER; in team mode we recalculate it.
sub UpdateTargets {
    for my $i (0 .. $::NUMPLAYERS - 1) {
        my $f = $::Fighters[$i];
        next unless defined $f && $f->{OK};
        # Only update OTHER for AI-controlled slots
        next unless defined $::Inputs[$i] && $::Inputs[$i]->isa('AIInput');
        my $target = SelectTarget($f);
        if (defined $target) {
            $f->{OTHER} = $target;
        }
    }
}

# ---------------------------------------------------------------------------
# Team coordination — avoid all AI on the same team attacking simultaneously
# ---------------------------------------------------------------------------

# Max fraction of a team allowed to be attacking at once.
# This prevents gang-up spam and creates more readable fights.
use constant MAX_SIMULTANEOUS_ATTACK_FRACTION => 0.5;

# Attacking state names (from Backend.pl header comment)
my %IS_ATTACKING = map { $_ => 1 } qw(
    HighPunch LowPunch HighKick LowKick Sweep GroinKick KneeKick
    Elbow Uppercut Throw Grenade KneelingPunch KneelingKick KneelingUppercut
);

# Returns 1 if the given AI slot should be suppressed from attacking this tick.
sub ShouldSuppressAttack {
    my ($slot) = @_;
    my $my_team = GetTeam($slot);

    # Count how many team-mates are currently attacking
    my ($team_size, $attacking_count) = (0, 0);
    for my $i (0 .. $::NUMPLAYERS - 1) {
        next unless GetTeam($i) == $my_team;
        my $f = $::Fighters[$i];
        next unless defined $f && $f->{OK} && $f->{ST} ne 'Dead';
        $team_size++;
        $attacking_count++ if $IS_ATTACKING{ $f->{ST} // '' };
    }

    return 0 if $team_size <= 1;

    my $frac = $team_size > 0 ? $attacking_count / $team_size : 0;
    # Suppress if we'd exceed the threshold
    return $frac >= MAX_SIMULTANEOUS_ATTACK_FRACTION;
}

# ---------------------------------------------------------------------------
# Tag system (optional)
# ---------------------------------------------------------------------------
# To use: call TeamBattle::RequestTag($player_slot) from a menu action or
# when a special "tag" move is triggered. The next available team-mate
# replaces the current fighter at the same X position.
#
# In the current game engine, tag is implemented via the existing
# NextTeamMember() Perl function + the C++ frontend NextTeamMember() call.
# This wrapper adds the "next available team-mate" selection logic.

# Per-player tag cooldown (ticks)
my @_tag_cooldown = (0, 0, 0, 0);
use constant TAG_COOLDOWN_TICKS => 180;  # ~2 seconds at 12 ms/tick

sub RequestTag {
    my ($slot) = @_;
    return 0 if $_tag_cooldown[$slot] > 0;
    return 0 unless defined $::Fighters[$slot] && $::Fighters[$slot]->{TEAMSIZE} > 1;

    # Tag-out: NextTeamMember is called by the C++ frontend; here we just
    # signal the request by setting a global the frontend can poll.
    $::TagRequest[$slot] = 1;
    $_tag_cooldown[$slot] = TAG_COOLDOWN_TICKS;
    return 1;
}

# Decrement tag cooldowns — call once per tick
sub TickCooldowns {
    for my $i (0 .. 3) {
        $_tag_cooldown[$i]-- if $_tag_cooldown[$i] > 0;
    }
}

# ---------------------------------------------------------------------------
# Camera — dynamic bounds for all active fighters
# ---------------------------------------------------------------------------

# Returns (min_x, max_x) in game units for all active fighters.
# The C++ camera system can use these to set zoom / pan.
sub GetCameraExtents {
    my ($min_x, $max_x);
    for my $f (@::Fighters) {
        next unless defined $f && $f->{OK} && $f->{ST} ne 'Dead';
        $min_x = $f->{X} if !defined $min_x || $f->{X} < $min_x;
        $max_x = $f->{X} if !defined $max_x || $f->{X} > $max_x;
    }
    return (0, $::SCRWIDTH2 || 5120) unless defined $min_x;

    # Add margin
    my $margin = 400;   # ~50 pixels
    $min_x -= $margin;
    $max_x += $margin;

    # Clamp to background bounds
    $min_x = 0           if $min_x < 0;
    $max_x = $::BGWIDTH2 if $max_x > ($::BGWIDTH2 // 15360);

    return ($min_x, $max_x);
}

# Compute suggested zoom factor (1.0 = normal, <1.0 = zoomed out)
sub GetCameraZoom {
    my ($min_x, $max_x) = GetCameraExtents();
    my $span   = $max_x - $min_x;
    my $screen = $::SCRWIDTH2 || 5120;
    my $zoom   = $screen / ($span > $screen ? $span : $screen);
    $zoom = 0.5 if $zoom < 0.5;   # don't zoom out too far
    return $zoom;
}

# ---------------------------------------------------------------------------
# Main per-tick entry point — call from GameAdvance in Backend.pl
# ---------------------------------------------------------------------------

sub Tick {
    return if $::NUMPLAYERS <= 2;   # nothing to coordinate in 1v1

    TickCooldowns();
    UpdateTargets();
    # ShouldSuppressAttack is queried by AIInput indirectly via AIController
}

1;

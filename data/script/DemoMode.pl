package DemoMode;

# =============================================================================
# DemoMode.pl  —  AI-vs-AI demo / spectator mode for OpenMortal
#
# Supports:
#   1v1 (2 AI), 2v2 (4 AI), 3v3 (6 AI via team-size settings)
#
# Entry point:
#   DemoMode::Start(%opts)   — begins a demo match
#   DemoMode::Tick()         — called once per game tick from GameAdvance
#   DemoMode::IsActive()     — true while demo is running
#   DemoMode::Stop()         — end demo (e.g., user pressed a key)
#
# Demo matches loop automatically. When a match ends the next one begins with
# randomly selected characters and optionally shuffled difficulties.
# =============================================================================

require 'AIController.pl';
require 'FighterStats.pl';

# ---------------------------------------------------------------------------
# State
# ---------------------------------------------------------------------------

my $_active     = 0;
my $_mode       = '1v1';     # '1v1', '2v2', '3v3'
my $_difficulty = AIInput::HARD;
my $_match_num  = 0;
my $_auto_loop  = 1;         # restart automatically after each match

# Keep track of which character IDs have been used recently (avoid repeats)
my @_recent_fighters = ();
use constant RECENT_WINDOW => 6;

# Available fighter enum values (matches FighterEnum.h / FighterStats.pl)
# Indices 1-15 map to the 15 characters.
my @ALL_FIGHTER_IDS;  # populated in Start()

# ---------------------------------------------------------------------------
# Public interface
# ---------------------------------------------------------------------------

sub IsActive { return $_active; }

sub Start {
    my (%opts) = @_;

    $_mode       = $opts{mode}       // '1v1';
    $_difficulty = $opts{difficulty} // AIInput::HARD;
    $_auto_loop  = $opts{auto_loop}  // 1;
    $_active     = 1;
    $_match_num  = 0;

    # Discover available fighter IDs from FighterStats
    _populate_fighter_ids();

    _start_match();
}

sub Stop {
    $_active = 0;
    # Restore human input for slots 0 and 1
    $::CPUSlots[0] = 0;
    $::CPUSlots[1] = 0;
    AIController::CreateMixedInputs();
}

# Called once per game tick while demo is running
sub Tick {
    return unless $_active;

    # Coordinate team battles if needed
    TeamBattle::Tick() if $_mode ne '1v1';

    # Check if the current match has ended
    if ($::over && $_auto_loop) {
        _start_match();
    }
}

# ---------------------------------------------------------------------------
# Match setup helpers
# ---------------------------------------------------------------------------

sub _start_match {
    $_match_num++;
    print "DemoMode: starting match $_match_num ($_mode)\n";

    my ($num_players, $team_size) = _mode_params($_mode);

    # Pick random fighters avoiding recent repeats
    my @selected = _pick_fighters($num_players);

    # Configure slots: all CPU
    for my $i (0 .. $num_players - 1) {
        $::CPUSlots[$i]      = 1;
        $::CPUDifficulty[$i] = _pick_difficulty();
    }

    # Randomise match settings slightly for variety
    $::MaxHP = 100;

    # Set character selections before GameStart — use SetPlayerNumber
    for my $i (0 .. $num_players - 1) {
        SetPlayerNumber($i, $selected[$i]) if defined &SetPlayerNumber;
    }

    # Create all-AI inputs
    AIController::CreateFullAIInputs(difficulty => $_difficulty);

    # Start the game backend for this many players
    GameStart($::MaxHP, $num_players, $team_size, $::WIDE // 0, 0)
        if defined &GameStart;

    print "DemoMode: players=(${\join(',', @selected)})\n";
}

sub _mode_params {
    my ($mode) = @_;
    return (2, 1) if $mode eq '1v1';
    return (4, 1) if $mode eq '2v2';   # 4 individual fighters, team_size=1 each
    return (4, 2) if $mode eq '3v3';   # 4 slots, team_size=2 means 2 each team
    return (2, 1);  # default
}

sub _pick_fighters {
    my ($count) = @_;
    my @pool = @ALL_FIGHTER_IDS;
    # Remove recently used
    my %recent = map { $_ => 1 } @_recent_fighters;
    my @fresh   = grep { !$recent{$_} } @pool;
    @fresh = @pool unless @fresh >= $count;   # fallback if pool is too small

    # Shuffle and pick
    my @shuffled = _shuffle(@fresh);
    my @chosen = @shuffled[0 .. $count - 1];

    # Update recent window
    push @_recent_fighters, @chosen;
    @_recent_fighters = @_recent_fighters[ -RECENT_WINDOW .. -1 ]
        if scalar(@_recent_fighters) > RECENT_WINDOW;

    return @chosen;
}

# Pick a difficulty for a demo slot.
# Always one of Hard / Mars / Ares — never Adaptive ML or below Hard.
# Weighted toward the harder end so spectators see impressive play:
#   Hard  25%  (exciting but readable)
#   Mars  40%  (dominant, punishing)
#   Ares  35%  (near-perfect, highlight-reel moments)
sub _pick_difficulty {
    my $roll = rand();
    return AIInput::HARD if $roll < 0.25;
    return AIInput::MARS if $roll < 0.65;   # 0.25–0.65 → 40%
    return AIInput::ARES;                   # 0.65–1.00 → 35%
}

sub _populate_fighter_ids {
    @ALL_FIGHTER_IDS = ();
    # FighterStats.pl exports $NUMFIGHTERS and GetFighterStats($enum)
    if (defined $::NUMFIGHTERS && $::NUMFIGHTERS > 0) {
        @ALL_FIGHTER_IDS = (1 .. $::NUMFIGHTERS);
    } else {
        # Fallback: IDs 1-15 (standard OpenMortal roster)
        @ALL_FIGHTER_IDS = (1 .. 15);
    }
}

# Fisher-Yates shuffle (pure Perl)
sub _shuffle {
    my @a = @_;
    for my $i (reverse 1 .. $#a) {
        my $j = int(rand($i + 1));
        @a[$i, $j] = @a[$j, $i];
    }
    return @a;
}

1;

package AIController;

# =============================================================================
# AIController.pl  —  Global AI configuration manager for OpenMortal
#
# Manages which slots are human/CPU, what difficulty each CPU runs at, and
# provides the factory functions that Backend.pl uses to set up $::Inputs[].
#
# Global variables set here and read by Backend.pl:
#   @::CPUSlots        — booleans, 1 = CPU-controlled for that player slot
#   @::CPUDifficulty   — difficulty level (AIInput constants) per slot
#   $::AIDebugEnabled  — set 1 to dump AI decision log each tick
# =============================================================================

require 'AIInput.pl';

# ---------------------------------------------------------------------------
# Default configuration
# ---------------------------------------------------------------------------

@::CPUSlots      = (0, 1, 0, 0);   # Player 0 = human, 1 = CPU by default
@::CPUDifficulty = (
    AIInput::MEDIUM,
    AIInput::MEDIUM,
    AIInput::MEDIUM,
    AIInput::MEDIUM,
);
$::AIDebugEnabled = 0;

# Config loaded from file, keyed by slot index
my @_slot_overrides = ({}, {}, {}, {});

# ---------------------------------------------------------------------------
# Factory — create $::Inputs[] array with correct types per slot
# ---------------------------------------------------------------------------

# Call this instead of (or after) CreatePlayerInputs() when running a game
# that includes CPU players.
sub CreateMixedInputs {
    @::Inputs = ();
    for my $i (0 .. $::MAXPLAYERS - 1) {
        if ($::CPUSlots[$i]) {
            my $diff = $::CPUDifficulty[$i] // AIInput::MEDIUM;
            my $ai   = AIInput->new($i, $diff);
            $ai->ApplyConfig(%{ $_slot_overrides[$i] }) if %{ $_slot_overrides[$i] };
            $::Inputs[$i] = $ai;
        } else {
            $::Inputs[$i] = PlayerInput->new();
        }
    }
}

# Make all active slots CPU (used for demo mode)
sub CreateFullAIInputs {
    my (%opts) = @_;
    my $diff = $opts{difficulty} // AIInput::HARD;
    @::Inputs = ();
    for my $i (0 .. $::MAXPLAYERS - 1) {
        my $ai = AIInput->new($i, $diff);
        $::Inputs[$i] = $ai;
    }
}

# ---------------------------------------------------------------------------
# Configuration setters (called from menu / CLI at startup)
# ---------------------------------------------------------------------------

sub SetSlotMode {
    my ($slot, $is_cpu, $difficulty) = @_;
    $::CPUSlots[$slot]      = $is_cpu     ? 1 : 0;
    $::CPUDifficulty[$slot] = $difficulty if defined $difficulty;
}

# Apply a JSON config hash (output of ai_config.py) to a specific slot.
# $config_href comes from DecodeConfigHash below.
sub ApplySlotConfig {
    my ($slot, $config_href) = @_;
    $_slot_overrides[$slot] = $config_href;
    # If input objects are already created, apply immediately
    if (defined $::Inputs[$slot] && $::Inputs[$slot]->isa('AIInput')) {
        $::Inputs[$slot]->ApplyConfig(%$config_href);
    }
}

# Parse a simple key=value config string (produced by ai_config.py --output perl)
sub LoadConfigString {
    my ($slot, $cfg_str) = @_;
    my %cfg;
    while ($cfg_str =~ /(\w+)\s*=\s*([^\n,;]+)/g) {
        my ($k, $v) = ($1, $2);
        $v =~ s/^\s+|\s+$//g;
        $cfg{$k} = $v + 0;  # coerce to number
    }
    ApplySlotConfig($slot, \%cfg);
}

# ---------------------------------------------------------------------------
# Per-tick update — called from a patched GameAdvance
# Handles debug logging and damage tracking
# ---------------------------------------------------------------------------

sub Tick {
    if ($::AIDebugEnabled) {
        for my $i (0 .. $::NUMPLAYERS - 1) {
            next unless defined $::Inputs[$i] && $::Inputs[$i]->isa('AIInput');
            my $log = $::Inputs[$i]->GetDebugLog();
            if (@$log && $log->[-1] =~ /action=/) {
                print STDERR $log->[-1] . "\n";
            }
        }
    }
}

# ---------------------------------------------------------------------------
# Difficulty name helpers (used by menu system)
# ---------------------------------------------------------------------------

my @DIFF_NAMES = ('', 'Very Easy', 'Easy', 'Medium', 'Hard', 'Mars', 'Ares', 'Adaptive ML');

sub DifficultyName {
    my ($level) = @_;
    return $DIFF_NAMES[$level] // 'Unknown';
}

sub DifficultyCount { return 7; }

1;

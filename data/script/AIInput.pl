package AIInput;

# =============================================================================
# AIInput.pl  —  AI decision engine for OpenMortal
#
# Drop-in replacement for PlayerInput for CPU-controlled fighters. Implements
# five difficulty levels plus an optional Adaptive ML mode that loads a small
# neural-network model exported by ml_training/train_ai.py.
#
# Interface (identical to PlayerInput):
#   new($player_num, $difficulty)  →  AIInput object
#   Advance()                      →  called once per game tick
#   GetAction()                    →  ($action, $modifier)
#   ActionAccepted()               →  called when move was consumed
#   KeyDown($key) / KeyUp($key)    →  ignored (AI drives itself)
#   Reset()                        →  reset between rounds
#   RewindData()                   →  snapshot for instant-replay rewind
# =============================================================================

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

use constant {
    VERY_EASY   => 1,
    EASY        => 2,
    MEDIUM      => 3,
    HARD        => 4,
    MARS        => 5,   # Harder than Hard — punishing and relentless
    ARES        => 6,   # Near-perfect rule-based — inhuman reflexes
    ADAPTIVE_ML => 7,   # ML model-driven
};

# Game unit constants (GAMEBITS = 3, so 1 pixel = 8 game units)
use constant {
    DIST_PUNCH_RANGE  =>  800,   # ≈100 px: in range for punches
    DIST_KICK_RANGE   =>  900,   # ≈112 px: in range for kicks
    DIST_APPROACH     => 1600,   # ≈200 px: start walking in
    DIST_FAR          => 2800,   # ≈350 px: jump-approach from here
    DIST_TOO_CLOSE    =>  560,   # ≈70  px: back off
};

# Action output names (must match PlayerInput / Fighter.pl CON keys)
my @RANDOM_SAFE_ACTIONS = ('', 'forw', 'back', 'down', 'block');
my @BASE_ATTACKS        = ('lpunch', 'hpunch', 'lkick', 'hkick');

# Opponent state tactical category
my %OPP_CAT = (
    Stand     => 'idle',   Walk      => 'idle',  Back      => 'idle',
    Turn      => 'idle',   Fun       => 'idle',  Threat    => 'idle',
    Jump      => 'air',    JumpFW    => 'air',   JumpBW    => 'air',
    Block     => 'block',
    Kneeling  => 'crouch',
    Onknees   => 'down',   Falling   => 'down',  Laying    => 'down',
    Getup     => 'getup',  Dead      => 'dead',
    HighPunch => 'atk',    LowPunch  => 'atk',   HighKick  => 'atk',
    LowKick   => 'atk',    Sweep     => 'atk',   GroinKick => 'atk',
    KneeKick  => 'atk',    Elbow     => 'atk',   Uppercut  => 'atk',
    Throw     => 'atk',
);

# ---------------------------------------------------------------------------
# Difficulty profiles
# ---------------------------------------------------------------------------
# reaction_ticks:     minimum ticks between decisions (simulates reaction lag)
# decision_interval:  how often the AI re-evaluates its situation
# aggression:         probability of attacking when in range
# combo_chance:       probability of choosing a special/combo over basic attack
# block_chance:       probability of blocking when opponent is attacking
# jump_chance:        probability of jump-approaching from far range
# error_rate:         probability of doing a random wrong action (lower = harder)
# hp_panic_thresh:    HP fraction below which AI becomes more defensive
# counter_aggression: extra probability to immediately counter after blocking
# ---------------------------------------------------------------------------

my %PROFILE = (
    VERY_EASY() => {
        reaction_ticks     => 60,
        decision_interval  => 30,
        aggression         => 0.20,
        combo_chance       => 0.05,
        block_chance       => 0.10,
        jump_chance        => 0.05,
        error_rate         => 0.70,
        hp_panic_thresh    => 0.20,
        counter_aggression => 0.00,
    },
    EASY() => {
        reaction_ticks     => 40,
        decision_interval  => 20,
        aggression         => 0.40,
        combo_chance       => 0.15,
        block_chance       => 0.25,
        jump_chance        => 0.10,
        error_rate         => 0.40,
        hp_panic_thresh    => 0.25,
        counter_aggression => 0.10,
    },
    MEDIUM() => {
        reaction_ticks     => 20,
        decision_interval  => 10,
        aggression         => 0.55,
        combo_chance       => 0.35,
        block_chance       => 0.45,
        jump_chance        => 0.15,
        error_rate         => 0.20,
        hp_panic_thresh    => 0.30,
        counter_aggression => 0.30,
    },
    HARD() => {
        reaction_ticks     => 8,
        decision_interval  => 5,
        aggression         => 0.75,
        combo_chance       => 0.60,
        block_chance       => 0.65,
        jump_chance        => 0.20,
        error_rate         => 0.05,
        hp_panic_thresh    => 0.35,
        counter_aggression => 0.55,
    },
    MARS() => {
        # Punishing and relentless. Reacts almost twice as fast as Hard,
        # chains combos consistently, and punishes every blocked hit.
        reaction_ticks     => 5,
        decision_interval  => 3,
        aggression         => 0.86,
        combo_chance       => 0.72,
        block_chance       => 0.75,
        jump_chance        => 0.25,
        error_rate         => 0.02,
        hp_panic_thresh    => 0.20,
        counter_aggression => 0.75,
    },
    ARES() => {
        # Near-inhuman rule-based ceiling. ~24 ms reaction, zero random
        # errors, maximum pressure, and almost always punishes after a block.
        reaction_ticks     => 2,
        decision_interval  => 1,
        aggression         => 0.93,
        combo_chance       => 0.85,
        block_chance       => 0.88,
        jump_chance        => 0.28,
        error_rate         => 0.00,
        hp_panic_thresh    => 0.10,
        counter_aggression => 0.92,
    },
    ADAPTIVE_ML() => {  # fallback values; overridden by ML model output
        reaction_ticks     => 3,
        decision_interval  => 2,
        aggression         => 0.65,
        combo_chance       => 0.50,
        block_chance       => 0.60,
        jump_chance        => 0.20,
        error_rate         => 0.00,
        hp_panic_thresh    => 0.40,
        counter_aggression => 0.60,
    },
);

# ---------------------------------------------------------------------------
# Constructor
# ---------------------------------------------------------------------------

sub new {
    my ($class, $player_num, $difficulty) = @_;
    $difficulty //= MEDIUM;

    my $self = {
        PLAYER_NUM        => $player_num,
        DIFFICULTY        => $difficulty,
        PROFILE           => $PROFILE{$difficulty},

        # Current decision state
        CURRENT_ACTION    => '',
        CURRENT_MOD       => '',
        TICKS_HELD        => 0,      # ticks we've been doing current action
        REACTION_WAIT     => 0,      # ticks until first reaction after change
        DECISION_TIMER    => 0,      # countdown to next re-evaluation

        # ML model (loaded lazily)
        ML_MODEL          => undef,
        ML_ENABLED        => ($difficulty == ADAPTIVE_ML),

        # Per-match stats for adaptive behaviour
        DAMAGE_DEALT      => 0,
        DAMAGE_TAKEN      => 0,
        COMBOS_USED       => 0,

        # Log buffer (for debug overlay)
        DEBUG_LOG         => [],
    };
    bless($self, $class);

    # Try to load ML model if adaptive
    if ($self->{ML_ENABLED}) {
        $self->_load_ml_model();
    }

    return $self;
}

# ---------------------------------------------------------------------------
# PlayerInput interface — lifecycle methods
# ---------------------------------------------------------------------------

sub Reset {
    my ($self) = @_;
    $self->{CURRENT_ACTION}  = '';
    $self->{CURRENT_MOD}     = '';
    $self->{TICKS_HELD}      = 0;
    $self->{REACTION_WAIT}   = $self->{PROFILE}{reaction_ticks};
    $self->{DECISION_TIMER}  = 0;
    $self->{DAMAGE_DEALT}    = 0;
    $self->{DAMAGE_TAKEN}    = 0;
    $self->{COMBOS_USED}     = 0;
    $self->{DEBUG_LOG}       = [];
}

sub RewindData {
    my ($self) = @_;
    my $snapshot = {
        PLAYER_NUM       => $self->{PLAYER_NUM},
        DIFFICULTY       => $self->{DIFFICULTY},
        PROFILE          => $self->{PROFILE},
        CURRENT_ACTION   => $self->{CURRENT_ACTION},
        CURRENT_MOD      => $self->{CURRENT_MOD},
        TICKS_HELD       => $self->{TICKS_HELD},
        REACTION_WAIT    => $self->{REACTION_WAIT},
        DECISION_TIMER   => $self->{DECISION_TIMER},
        ML_MODEL         => $self->{ML_MODEL},
        ML_ENABLED       => $self->{ML_ENABLED},
        DAMAGE_DEALT     => $self->{DAMAGE_DEALT},
        DAMAGE_TAKEN     => $self->{DAMAGE_TAKEN},
        COMBOS_USED      => $self->{COMBOS_USED},
        DEBUG_LOG        => [ @{$self->{DEBUG_LOG}} ],
    };
    bless($snapshot, 'AIInput');
    return $snapshot;
}

# KeyDown/KeyUp are no-ops for AI — keyboard events do not affect CPU players
sub KeyDown { }
sub KeyUp   { }

# Called once per game tick by Backend.pl before Fighter::Advance
sub Advance {
    my ($self) = @_;
    $self->{TICKS_HELD}++;
    $self->{DECISION_TIMER}-- if $self->{DECISION_TIMER} > 0;
    $self->{REACTION_WAIT}--  if $self->{REACTION_WAIT}  > 0;
}

# Called by Fighter.pl when our proposed action was accepted into a new state
sub ActionAccepted {
    my ($self) = @_;
    $self->{TICKS_HELD}    = 0;
    $self->{DECISION_TIMER} = $self->{PROFILE}{decision_interval};
}

# ---------------------------------------------------------------------------
# GetAction — main decision point
# Called by Fighter::Advance each tick while fighter is in a "Ready" state
# Returns ($action, $modifier) — same contract as PlayerInput::GetAction
# ---------------------------------------------------------------------------

sub GetAction {
    my ($self) = @_;

    # Still waiting on reaction lag — hold current action
    return ($self->{CURRENT_ACTION}, $self->{CURRENT_MOD})
        if $self->{REACTION_WAIT} > 0;

    # Decision timer not expired — hold current action unless forced
    return ($self->{CURRENT_ACTION}, $self->{CURRENT_MOD})
        if $self->{DECISION_TIMER} > 0;

    # Time to make a new decision
    my ($action, $mod) = $self->_decide();

    $self->{CURRENT_ACTION} = $action;
    $self->{CURRENT_MOD}    = $mod // '';
    $self->{TICKS_HELD}     = 0;
    # Add brief reaction delay before next re-evaluation
    $self->{REACTION_WAIT}  = int($self->{PROFILE}{reaction_ticks} * 0.3);

    # Track whether we were blocking last tick (for counter_aggression)
    my $me = $::Fighters[$self->{PLAYER_NUM}];
    $self->{WAS_BLOCKING} = (defined $me && ($me->{ST} // '') eq 'Block') ? 1 : 0;

    $self->_log("action=$action mod=" . ($mod // ''));
    return ($action, $mod);
}

# ---------------------------------------------------------------------------
# Core decision logic
# ---------------------------------------------------------------------------

sub _decide {
    my ($self) = @_;
    my $prof = $self->{PROFILE};

    # ---- Error roll: lower difficulties sometimes do something random ----
    if ($prof->{error_rate} > 0 && rand() < $prof->{error_rate}) {
        return ($RANDOM_SAFE_ACTIONS[int(rand(scalar @RANDOM_SAFE_ACTIONS))], '');
    }

    # ---- Read game state ----
    my $me  = $::Fighters[$self->{PLAYER_NUM}];
    return ('', '') unless defined $me && $me->{OK};

    my $opp = $me->{OTHER};
    return ('', '') unless defined $opp && $opp->{OK} && $opp->{ST} ne 'Dead';

    # ---- Compute distance (positive = opponent in front) ----
    my $dist = ($opp->{X} - $me->{X}) * $me->{DIR};

    # ---- HP-based panic modifier ----
    my $hp_frac = $me->{HP} / ($::MaxHP || 100);
    my $panicking = ($hp_frac < $prof->{hp_panic_thresh});

    # ---- Classify opponent's tactical situation ----
    my $opp_cat = $OPP_CAT{ $opp->{ST} } // 'idle';

    # ---- ML model override (ADAPTIVE_ML difficulty) ----
    if ($self->{ML_ENABLED} && defined $self->{ML_MODEL}) {
        my ($ml_action, $ml_mod) = $self->_ml_decide($me, $opp, $dist);
        return ($ml_action, $ml_mod) if defined $ml_action;
    }

    # ---- Rule-based decision tree ----
    return $self->_rule_decide($me, $opp, $dist, $opp_cat, $panicking, $prof);
}

sub _rule_decide {
    my ($self, $me, $opp, $dist, $opp_cat, $panicking, $prof) = @_;

    # ---- 1. Opponent is behind us — turn / walk to face them ----
    if ($dist < 0) {
        return ('forw', '');
    }

    # ---- 1b. Counter-attack: we were blocking and opponent just stopped ----
    # Mars/Ares punish every blocked hit; lower levels rarely bother.
    if ($self->{WAS_BLOCKING} && $opp_cat ne 'atk'
        && $dist < DIST_KICK_RANGE
        && rand() < ($prof->{counter_aggression} // 0))
    {
        return $self->_pick_attack($me, $dist, $prof, $prof->{combo_chance});
    }

    # ---- 2. Opponent is attacking — consider blocking ----
    if ($opp_cat eq 'atk') {
        my $block_roll = $panicking ? $prof->{block_chance} * 1.5 : $prof->{block_chance};
        $block_roll = 1.0 if $block_roll > 1.0;
        if (rand() < $block_roll) {
            return ('block', '');
        }
        # Didn't block — try a counter-attack if in range
        if ($dist < DIST_KICK_RANGE && rand() < $prof->{aggression} * 0.7) {
            return $self->_pick_attack($me, $dist, $prof, 0);
        }
    }

    # ---- 3. Opponent is in the air — anti-air or wait ----
    if ($opp_cat eq 'air') {
        if ($dist < DIST_KICK_RANGE && rand() < $prof->{aggression}) {
            # High punch / high kick effective against airborne
            return ('hpunch', '') if rand() < 0.5;
            return ('hkick',  '');
        }
        return ('', '');   # wait for them to land
    }

    # ---- 4. Opponent is knocked down — press the advantage ----
    if ($opp_cat eq 'down' || $opp_cat eq 'getup') {
        if ($dist < DIST_KICK_RANGE && rand() < $prof->{aggression}) {
            return $self->_pick_attack($me, $dist, $prof, $prof->{combo_chance});
        }
        # Approach if not close enough
        return ('forw', '') if $dist > DIST_APPROACH;
        return ('', '');
    }

    # ---- 5. Within attack range — strike ----
    if ($dist < DIST_KICK_RANGE) {
        if ($dist < DIST_TOO_CLOSE && !$panicking) {
            # Too close — back off or jump over
            return ('back', '') if rand() < 0.6;
            return ('jumpbw', '');
        }
        if (rand() < $prof->{aggression}) {
            return $self->_pick_attack($me, $dist, $prof, $prof->{combo_chance});
        }
        # Didn't commit to attack — defensive stance
        return ('block', '') if rand() < $prof->{block_chance} * 0.5;
        return ('', '');
    }

    # ---- 6. Medium range — approach ----
    if ($dist < DIST_APPROACH) {
        if ($panicking && rand() < 0.4) {
            return ('block', '');
        }
        if (rand() < $prof->{aggression} * 0.6) {
            return ('forw', '');
        }
        return ('', '');
    }

    # ---- 7. Far range — close the gap ----
    if ($dist >= DIST_APPROACH) {
        if (rand() < $prof->{jump_chance}) {
            return ('jumpfw', '');   # leap in
        }
        return ('forw', '');
    }

    return ('', '');
}

# ---------------------------------------------------------------------------
# Attack selection — picks an appropriate attack given range and combo chance
# ---------------------------------------------------------------------------

sub _pick_attack {
    my ($self, $fighter, $dist, $prof, $combo_chance) = @_;

    # Collect available moves from the fighter's current CON table
    my ($attacks_ref, $specials_ref) = _parse_con($fighter);

    # Try a special/combo move
    if ($combo_chance > 0 && rand() < $combo_chance && @$specials_ref) {
        my $move = $specials_ref->[ int(rand(scalar @$specials_ref)) ];
        $self->{COMBOS_USED}++;
        return @$move;   # ($action, $modifier)
    }

    # Basic attack: prefer punches at close range, kicks slightly further out
    if (@$attacks_ref) {
        if ($dist < DIST_PUNCH_RANGE) {
            # Bias toward punches at close range
            my @punches = grep { $_->[0] =~ /punch/ } @$attacks_ref;
            if (@punches && rand() < 0.65) {
                return @{ $punches[ int(rand(scalar @punches)) ] };
            }
        }
        return @{ $attacks_ref->[ int(rand(scalar @$attacks_ref)) ] };
    }

    # Fallback if CON isn't readable (should not happen in practice)
    return ('hpunch', '');
}

# ---------------------------------------------------------------------------
# Parse the fighter's current-state CON hash into categorised move lists.
# Returns (\@basic_attacks, \@special_moves).
# Each entry is [$action_name, $modifier_string].
# ---------------------------------------------------------------------------

sub _parse_con {
    my ($fighter) = @_;
    my (@attacks, @specials);

    my $stname = $fighter->{ST} // 'Stand';
    my $states  = $fighter->{STATES} // {};
    my $st      = $states->{$stname} // {};
    my $con     = $st->{CON}         // {};

    for my $key (keys %$con) {
        if ($key =~ /^(lpunch|hpunch|lkick|hkick)(.*)$/) {
            my ($base, $mod) = ($1, $2);
            if (length($mod) > 0) {
                push @specials, [$base, $mod];
            } else {
                push @attacks,  [$base, ''];
            }
        }
    }

    # If no attacks in current state, fall back to generic list
    unless (@attacks) {
        @attacks = map { [$_, ''] } @BASE_ATTACKS;
    }

    return (\@attacks, \@specials);
}

# ---------------------------------------------------------------------------
# ML model — forward pass
# ---------------------------------------------------------------------------
# The model is a small MLP saved as JSON by ml_training/train_ai.py.
# Format: { "layers": [ {W: [[...]], b: [...]}, ... ], "actions": [...] }
# Activation: ReLU on hidden layers, argmax on output.
# ---------------------------------------------------------------------------

sub _load_ml_model {
    my ($self) = @_;

    my $model_path = "$::DATADIR/ai_model.json";
    unless (-f $model_path) {
        $model_path = "data/ai_model.json";  # fallback during development
    }
    unless (-f $model_path) {
        $self->_log("ML model not found — falling back to rule-based HARD AI");
        $self->{ML_ENABLED} = 0;
        $self->{PROFILE}    = $PROFILE{ HARD() };
        return;
    }

    eval {
        open(my $fh, '<', $model_path) or die "Cannot open $model_path: $!";
        local $/;
        my $json_text = <$fh>;
        close($fh);
        $self->{ML_MODEL} = _parse_json_model($json_text);
        $self->_log("ML model loaded from $model_path");
    };
    if ($@) {
        $self->_log("ML model load error: $@");
        $self->{ML_ENABLED} = 0;
        $self->{PROFILE}    = $PROFILE{ HARD() };
    }
}

# Minimal JSON model parser (avoids requiring JSON module)
# Only handles the simple nested-array structure produced by train_ai.py.
sub _parse_json_model {
    my ($json) = @_;

    # Extract action list
    my (@actions);
    if ($json =~ /"actions"\s*:\s*\[([^\]]+)\]/) {
        my $acts = $1;
        @actions = ($acts =~ /"([^"]+)"/g);
    }

    # Extract layers: each layer has W (2-D array) and b (1-D array)
    my @layers;
    while ($json =~ /"W"\s*:\s*(\[\[.*?\]\])\s*,\s*"b"\s*:\s*(\[[^\]]+\])/gs) {
        my ($w_str, $b_str) = ($1, $2);
        my @W = map { [ /(-?[\d.e+-]+)/g ] } ($w_str =~ /\[([^\[\]]+)\]/g);
        my @b  = ($b_str  =~ /(-?[\d.e+-]+)/g);
        push @layers, { W => \@W, b => \@b };
    }

    return { layers => \@layers, actions => \@actions };
}

# Run forward pass: input vector → action index
sub _ml_forward {
    my ($self, @state_vec) = @_;
    my $model = $self->{ML_MODEL};
    return undef unless $model && @{$model->{layers}};

    my @x = @state_vec;

    for my $i (0 .. $#{ $model->{layers} }) {
        my $layer = $model->{layers}[$i];
        my @W = @{ $layer->{W} };
        my @b = @{ $layer->{b} };
        my @y;
        for my $j (0 .. $#W) {
            my $sum = $b[$j];
            for my $k (0 .. $#x) {
                $sum += $W[$j][$k] * $x[$k];
            }
            # ReLU on hidden layers; linear on output layer
            if ($i < $#{ $model->{layers} }) {
                $sum = 0 if $sum < 0;
            }
            push @y, $sum;
        }
        @x = @y;
    }

    # Argmax
    my $best_i = 0;
    for my $i (1 .. $#x) {
        $best_i = $i if $x[$i] > $x[$best_i];
    }
    return $best_i;
}

# Build state vector and run ML inference, return ($action, $mod)
sub _ml_decide {
    my ($self, $me, $opp, $dist) = @_;

    my $max_hp   = $::MaxHP || 100;
    my $max_dist = 5120;   # SCRWIDTH2 = 640 * 8

    # 11-dimensional state vector
    my @state = (
        ($me->{X}  // 0) / $max_dist,        # own normalised X
        ($me->{Y}  // 0) / ($::SCRHEIGHT2 || 3840),
        ($me->{HP} // 0) / $max_hp,           # own HP fraction
        ($opp->{X} // 0) / $max_dist,
        ($opp->{Y} // 0) / ($::SCRHEIGHT2 || 3840),
        ($opp->{HP} // 0) / $max_hp,
        $dist / $max_dist,                     # signed distance (positive = in front)
        (($OPP_CAT{$opp->{ST} // ''} // 'idle') eq 'atk')  ? 1 : 0,
        (($OPP_CAT{$opp->{ST} // ''} // 'idle') eq 'air')  ? 1 : 0,
        (($OPP_CAT{$opp->{ST} // ''} // 'idle') eq 'down') ? 1 : 0,
        ($me->{IDLE} // 0) > 5               ? 1 : 0,
    );

    my $action_idx = $self->_ml_forward(@state);
    return (undef, undef) unless defined $action_idx;

    my @action_map = ('', 'forw', 'back', 'block', 'hpunch', 'lpunch', 'hkick', 'lkick');
    my $action = $action_map[$action_idx] // '';
    return ($action, '');
}

# ---------------------------------------------------------------------------
# Debug helpers
# ---------------------------------------------------------------------------

sub _log {
    my ($self, $msg) = @_;
    push @{ $self->{DEBUG_LOG} }, sprintf("[AI p%d d%d t%d] %s",
        $self->{PLAYER_NUM}, $self->{DIFFICULTY}, $::gametick // 0, $msg);
    # Keep log bounded
    shift @{ $self->{DEBUG_LOG} } while scalar(@{ $self->{DEBUG_LOG} }) > 50;
}

sub GetDebugLog {
    my ($self) = @_;
    return $self->{DEBUG_LOG};
}

sub GetStats {
    my ($self) = @_;
    return {
        difficulty    => $self->{DIFFICULTY},
        damage_dealt  => $self->{DAMAGE_DEALT},
        damage_taken  => $self->{DAMAGE_TAKEN},
        combos_used   => $self->{COMBOS_USED},
        ml_active     => $self->{ML_ENABLED} ? 1 : 0,
    };
}

# ---------------------------------------------------------------------------
# Damage tracking (called from AIController when a hit is confirmed)
# ---------------------------------------------------------------------------

sub RecordDamageDealt { $_[0]->{DAMAGE_DEALT} += $_[1]; }
sub RecordDamageTaken { $_[0]->{DAMAGE_TAKEN} += $_[1]; }

# ---------------------------------------------------------------------------
# Apply an external config hash (from claude_interface/ai_config.py output)
# ---------------------------------------------------------------------------

sub ApplyConfig {
    my ($self, %cfg) = @_;
    my $p = $self->{PROFILE};

    $p->{aggression}     = $cfg{aggression}      if exists $cfg{aggression};
    $p->{combo_chance}   = $cfg{combo_frequency} if exists $cfg{combo_frequency};
    $p->{block_chance}   = $cfg{block_chance}    if exists $cfg{block_chance};
    $p->{jump_chance}    = $cfg{jump_chance}     if exists $cfg{jump_chance};
    $p->{error_rate}     = 1.0 - ($cfg{accuracy} // (1.0 - $p->{error_rate}));

    if (exists $cfg{reaction_time}) {
        # reaction_time in ms; game runs at ~12 ms/tick by default
        $p->{reaction_ticks} = int($cfg{reaction_time} / 12);
    }
}

1;

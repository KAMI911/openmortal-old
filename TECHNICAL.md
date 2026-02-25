# OpenMortal — Technical Documentation

**For use with AI-assisted development (Claude AI)**

---

## Project Overview

OpenMortal is an open-source 2D fighting game written in C++ with a Perl scripting backend. It is a parody of Mortal Kombat featuring characters from the author's community. The C++ frontend handles rendering, input, audio, and networking; the Perl backend runs the game logic (moves, collisions, damage, AI).

- **Author**: UPI (upi@apocalypse.rulez.org)
- **Version**: ~0.7
- **Language**: C++ (frontend), Perl 5 (backend/scripting)
- **Build system**: GNU Autotools (autoconf + automake)
- **Platform**: Linux, macOS, Windows (via SDL 1.2)

---

## Key Dependencies

| Library      | Purpose                              |
|--------------|--------------------------------------|
| SDL 1.2      | Video, events, joystick              |
| SDL_image    | PNG/JPEG loading                     |
| SDL_mixer    | Music (MOD/S3M) and sound (WAV/VOC)  |
| SDL_ttf      | TrueType font rendering              |
| SDL_net      | TCP/IP networking                    |
| Perl 5       | Embedded game scripting engine       |
| FreeType2    | Font rendering (via SDL_ttf)         |
| Qt 3/4       | Character editor tool only           |

---

## Directory Structure

```
openmortal-old/
├── src/                    # C++ source files
├── data/
│   ├── characters/         # Fighter .pl scripts + .dat sprites
│   ├── fonts/              # TrueType and bitmap fonts
│   ├── gfx/                # UI graphics, backgrounds, portraits
│   ├── script/             # Perl game engine modules
│   └── sound/              # Music (MOD/S3M) and effects (WAV/VOC)
├── editor/                 # Qt-based MortalEditor (character tool)
├── debian/                 # Debian packaging
├── distribution/           # Distribution artifacts
├── osx/                    # macOS build support
└── OpenMortal.xcode/       # Xcode project
```

---

## Source Files Reference (`src/`)

### `main.cpp`
Entry point. Initializes SDL, loads fonts, initializes the Perl backend, and runs the main loop.

Key functions:
- `main()` — SDL init, font loading, main loop dispatch
- `init()` — video mode + font setup
- `init2()` — Perl interpreter init
- `GameLoop()` — player select → game → game over
- `ChatLoop()` — MortalNet online lobby + game
- `DrawMainScreen()` — intro screen with character roster

Global fonts: `inkFont`, `impactFont`, `titleFont`, `chatFont` (TTF), `fastFont`, `creditsFont`, `storyFont` (bitmap)

---

### `State.h` / `State.cpp` — `SState` struct
Global singleton: `g_oState`

Holds all persistent configuration and global runtime state:

| Field | Type | Description |
|---|---|---|
| `m_enGameMode` | `TGameMode` | IN_DEMO, IN_SINGLE, IN_MULTI, IN_NETWORK, IN_CHAT |
| `m_bQuitFlag` | bool | Request program exit |
| `m_iNumPlayers` | int | 2–4 |
| `m_enTeamMode` | `TTeamMode` | ONE_VS_ONE, GOOD_VS_EVIL, CUSTOM |
| `m_iTeamSize` | int | Players per team |
| `m_iRoundLength` | int | Seconds per round |
| `m_iHitPoints` | int | Starting health |
| `m_iGameSpeed` | int | ms per tick (1000/fps) |
| `m_bFullscreen` | bool | Fullscreen toggle |
| `m_aiPlayerKeys[4][9]` | int | Key bindings (4 players × 9 actions) |
| `m_acLatestServer` | char[] | Network server hostname |
| `m_bServer` | bool | Act as server |
| `m_acNick` | char[] | Online nick |

Config persisted to `~/.openmortalrc` (Linux), `~/Library/Preferences/OpenMortal.cfg` (macOS), or registry/ini (Windows).

Key methods: `Load()`, `Save()`, `ToggleFullscreen()`, `SetLanguage()`, `SetServer()`

---

### `Backend.h` / `Backend.cpp` — `CBackend` class
Global singleton: `g_oBackend`

Embeds a Perl interpreter and bridges C++ ↔ Perl.

Key methods:
- `PerlEvalF(format, ...)` — execute arbitrary Perl code
- `GetPerlString(name)`, `GetPerlInt(name)` — read Perl scalars
- `AdvancePerl()` — step backend one tick
- `ReadFromPerl()` — sync Perl state → C++ structs
- `IsDead(player)` — check if player is KO'd
- `PlaySounds()` — trigger queued sound effects
- `WriteToString(out)` / `ReadFromString(in)` — serialize/deserialize for replay

Exposed state after `ReadFromPerl()`:
- `m_iGameTick`, `m_iGameOver`, `m_bKO`
- `m_iBgX`, `m_iBgY` — parallax scroll position
- `m_aoPlayers[4]` (SPlayer): `m_iX`, `m_iY`, `m_iFrame`, `m_iHitPoints`, `m_iRealHitPoints`
- `m_aoDoodads[20]` (SDoodad): `m_iX`, `m_iY`, `m_iType`, `m_iFrame`, `m_iDir`, `m_sText`
- `m_asSounds[20]` — queued sound names

---

### `Game.h` / `Game.cpp` — `CGame` class
Runs a single complete match (multiple rounds).

Game phases (`TGamePhaseEnum`):
- `Ph_START` — "Round X" intro
- `Ph_NORMAL` — active fighting
- `Ph_TIMEUP` — time expired
- `Ph_KO` — knockout
- `Ph_REWIND` — rewind to pre-KO
- `Ph_SLOWFORWARD` — slow-motion replay
- `Ph_REPLAY` — replay mode

Key members:
- `m_iGameTime` — current tick
- `m_iFrame` — frame counter
- `m_aiRoundsWonByPlayer[4]` — score per player
- `m_enGamePhase` — current phase
- `m_oKeyQueue` — CKeyQueue for network-synchronized input
- `m_poBackground` — arena background

Key methods:
- `Run()` — main game loop, returns winner index
- `GetReplay()` — serialized replay string
- `DoReplay(filename)` — load and play replay

Inner class `CKeyQueue`:
- `EnqueueKey(time, player, key, down)` — queue input with timestamp
- `DequeueKeys(to_time)` — flush queued input up to current time

---

### `Background.h` / `Background.cpp` — `CBackground` class
Multi-layer parallax background system.

Layer struct `SBackgroundLayer`:
- `m_poSurface` — SDL_Surface*
- `m_iXOffset`, `m_iYOffset` — layer offset
- `m_dDistance` — parallax factor (1.0 = no parallax)

Methods:
- `Load(number)` — load arena by number (e.g. level1.jpg or *.desc multi-layer)
- `AddExtraLayer(layer)` — overlay layer (dead fighters in team mode)
- `Draw(x, y, y_offset)` — render all layers with parallax

---

### `RlePack.h` / `RlePack.cpp` — `CRlePack` class
RLE-compressed sprite system. Loads `.dat` files, stores 256-color palette.

Features:
- 256-color palette with tinting support
- Fast blitting via RLE compression
- Horizontal flip without pre-mirrored data

Key methods:
- `CRlePack(filename, numcolors)` — load .dat file
- `Count()` — number of sprites
- `SetTint(TintEnum)` — apply color transform
- `ApplyPalette()` — set current palette as SDL palette
- `Draw(index, x, y, flipped)` — blit sprite to screen
- `CreateSurface(index, flipped)` — create SDL_Surface from sprite

**Tint types** (`TintEnum`): NO_TINT, ZOMBIE_TINT, GRAY_TINT, DARK_TINT, INVERTED_TINT, FROZEN_TINT

**Palette layout** (256 colors):
- 0–63: Background
- 64–111: Doodads
- 112–175: Player 1
- 176–239: Player 2
- 240–255: System colors

---

### `Audio.h` / `Audio.cpp` — `COpenMortalAudio` class
Global pointer: `Audio`

SDL_mixer wrapper managing named samples and music.

Sample methods:
- `LoadSample(filename, name)` / `UnloadSample(name)` / `PlaySample(name)` / `PlayFile(filename)`
- `LoadSampleMap()` — loads `soundmap.txt` name→file mappings

Music methods:
- `LoadMusic(filename, name)` / `UnloadMusic(name)` / `PlayMusic(name)`
- `FadeMusic(ms)` / `SetMusicVolume(0-100)` / `StopMusic()` / `IsMusicPlaying()`

Supports 10 concurrent channels. Volumes respect `g_oState` settings.

---

### `gfx.h` / `gfx.cpp` — Graphics Utilities

Global: `extern SDL_Surface* gamescreen`

Key functions:
- `DrawTextMSZ(...)` — TrueType text with flags: `AlignHCenter`, `AlignVCenter`, `UseTilde` (color codes), `UseShadow`
- `DrawGradientText(...)` — gradient effect text
- `LoadBackground(file)` — load JPEG/PNG with palette extraction
- `LoadImage(file)` — load PNG
- `SetVideoMode(wide, fullscreen)` — switch resolution (640×480 or 800×600)

`CSurfaceLocker` — RAII SDL surface lock/unlock helper

---

### `Event.h` — Input Event System

Mortal event types (`TMortalEventEnum`):
- `Me_NOTHING`, `Me_QUIT`, `Me_MENU`, `Me_SKIP`
- `Me_PLAYERKEYDOWN`, `Me_PLAYERKEYUP`

Mortal key indices (`TMortalKeysEnum`):
- 0=UP, 1=DOWN, 2=LEFT, 3=RIGHT, 4=BLOCK
- 5=LPUNCH, 6=HPUNCH, 7=LKICK, 8=HKICK

`SMortalEvent` struct: `m_enType`, `m_iPlayer`, `m_iKey`

Functions:
- `TranslateEvent(SDL_Event, SMortalEvent&)` — keyboard/joystick → Mortal event
- `MortalPollEvent(event)` — non-blocking
- `MortalWaitEvent(event)` — blocking

---

### `Joystick.h` / `Joystick.cpp` — `CJoystick` class
Global: `g_oJoystick`

- Multi-joystick support (up to 32 devices)
- Axis-to-direction translation
- Workaround for "SIGHT FIGHTER ACTION USB" hardware bug

---

### `PlayerSelect.h` / `PlayerSelect.cpp` — `CPlayerSelect` class
Global: `g_oPlayerSelect`

`SPlayerInfo` per player:
- `m_enFighter` — FighterEnum selection
- `m_enTint` — TintEnum palette effect
- `m_poPack` — CRlePack* sprite data
- `m_sFighterName` — name string
- `m_aenTeam` — vector for team mode fighters

Methods:
- `DoPlayerSelect()` — run selection UI
- `SetPlayer(player, fighter)` / `SetTint(player, tint)`
- `IsFighterAvailable(fighter)` / `IsLocalFighterAvailable(fighter)`
- `GetPlayerInfo(player)` / `EditPlayerInfo(player)`

Companion classes:
- `CPlayerSelectView` — renders selection UI
- `CPlayerSelectController` — handles input + network sync during selection

---

### `menu.h` / `menu.cpp` — Menu System

Class hierarchy:
- `CMenu` — base menu (tree navigation, ESC to exit)
- `CMenuItem` — button/action
- `CEnumMenuItem` — left/right selector
- `CTextMenuItem` — editable text field
- `CNetworkMenu` — subclass for network setup

Main menu tree:
```
MAIN MENU
├── SINGLE PLAYER (Easy/Medium/Hard)
├── MULTI PLAYER (players, teams, team size)
├── NETWORK GAME (server/client, hostname, nick, MortalNet)
├── OPTIONS
│   ├── GAME SPEED / ROUND LENGTH / HIT POINTS
│   ├── SOUND (channels, rate, bits, volumes)
│   ├── FULLSCREEN
│   └── KEY BINDINGS
├── LANGUAGE
├── INFO
└── QUIT
```

---

### `MortalNetwork.h` — Network Interface (abstract)
`CMortalNetwork` pure virtual class.

Game parameters struct `SGameParams`:
```cpp
Uint32 iRoundLength;
Uint32 iGameSpeed;
Uint32 iHitPoints;
Uint32 iBackgroundNumber;
```

Connection: `Start(server)`, `Stop()`, `IsConnectionAlive()`, `Update()`, `IsMaster()`

Chat: `SendMsg(text)`, `IsMsgAvailable()`, `GetMsg()`

Fighter selection: `SendFighter()`, `SendReady()`, `SendGameParams()`, `GetGameParams()`, `IsRemoteSideReady()`

Game sync: `SynchStartRound()`, `SendGameData()`, `GetLatestGameData()`, `SendKeystroke()`, `GetKeystroke()`, `SendGameTick()`, `GetGameTick()`, `SendHurryup()`, `SendRoundOver()`, `IsRoundOver()`, `IsGameOver()`, `GetWhoWon()`

---

### `MortalNetworkImpl.h` / `MortalNetworkImpl.cpp`
TCP/IP implementation using SDL_net.

**Message protocol** (text-based, newline-delimited):
```
I <version> <username>    — introduction
F <number>                — fighter choice
R                         — ready
Q/A <number>              — fighter availability query/response
P <n> <n> <n> <n>         — game parameters
M <text>                  — chat message
S                         — sync/start
G <text>                  — backend game state (serialized Perl)
T <number> <number>       — game tick
K <number> <bool>         — keystroke (key#, down)
H <number>                — hurryup code
O <number> <bool>         — round over (winner, game_over)
```

Network states (`TNetworkState`): NS_DISCONNECTED, NS_CHARACTER_SELECTION, NS_IN_GAME

Master/slave alternates each round for fairness. Master sends game state; slave sends input.

---

### `OnlineChat.h` / `OnlineChat.cpp`
MortalNet online lobby (separate TCP server).

Protocol subset:
- Server → `J <user> <IP>` (join), `L <user>` (leave), `M <user> <text>` (message), `C <user>` (challenge)
- Client → `N <version> <nick>` (login), `M <text>` (chat), `C <user>` (challenge), `L` (quit)

---

### `Demo.h` / `Demo.cpp` — Demo / Intro Sequences

Base `CDemo` class — displays intros, stats, victory screens.
- `Run()` / `Advance(frames, flip)` / `AdvanceFlyingChars(frames)` / `AdvanceGame(frames)`

Derived: `CFighterStatsDemo` — winner stats + story text

---

### `FlyingChars.h` / `FlyingChars.cpp` — `CFlyingChars` class
Animated letters flying onto screen.

`SFlyingLetter`: position, start, velocity, delay, character.

Methods: `AddText(text, alignment, one_by_one)`, `Advance(frames)`, `Draw()`, `IsDone()`

Alignment: `FC_AlignLeft`, `FC_AlignRight`, `FC_AlignCenter`, `FC_AlignJustify`

---

### `GameOver.cpp`
`GameOver(winner_index)` — "Final Judgement" screen with animated foot crushing loser. Calls `JudgementStart()` Perl function.

---

### `FighterStats.h` / `FighterStats.cpp`
Post-match stats display.

---

### `TextArea.h` — `CTextArea` / `CReadline`
Chat UI text area and input line widgets.

---

### `common.h` / `common.cpp`
Global utility functions:
- `debug(format, ...)` — stderr (Unix) or MessageBox (Windows)
- `DoMenu()` — show main menu
- `GameOver(winner)` — victory screen
- `DoDemos()` — run demo sequence
- `DoGame(replay_file, is_replay, debug)` — run single match
- `DoOnlineChat()` — connect to MortalNet
- `Translate(text)` / `TranslateUTF8(text)` — localization
- Math macros: `omABS`, `omMAX`, `omMIN`

---

## Perl Backend (`data/script/`)

| File | Purpose |
|---|---|
| `Backend.pl` | Main game loop, init |
| `Fighter.pl` | Fighter AI, move definitions |
| `PlayerInput.pl` | Keyboard/input processing |
| `Collision.pl` | Hit detection |
| `Damage.pl` | Health/damage calculation |
| `Doodad.pl` | Floating text, projectiles |
| `FighterStats.pl` | Winner statistics |
| `DataHelper.pl` | Data utilities |
| `Translate.pl` | Language/localization |
| `Keys.pl` | Key mapping |
| `State.pl` | Game state variables |
| `QuickSave.pl` / `Rewind.pl` | Replay/rewind system |
| `CollectStats.pl` | Statistics collection |

C++ calls into Perl via `CBackend::PerlEvalF()` (embedded interpreter). Perl communicates back by setting named scalars that C++ reads with `GetPerlInt()` / `GetPerlString()` and structured state via `ReadFromPerl()`.

---

## Character Data (`data/characters/`)

Each of the 15 fighters has:
- `NAME.pl` — Perl fighter definition (moves, animations, AI)
- `NAME.dat` — RLE-compressed sprites (loaded by `CRlePack`)
- `NAME.dat.txt` — human-readable DAT metadata
- `NAME.full.png` — full portrait (player select)
- `NAME.icon.png` — small icon
- `NAME.jpg` — photo/story image

**FighterEnum** (15 fighters):
`ULMAR, UPI, ZOLI, CUMI, SIRPI, MACI, BENCE, GRIZLI, DESCANT, SURBA, AMBRUS, DANI, KINGA, MISI`

---

## Global Constants

```cpp
MAXPLAYERS    = 4
MAXDOODADS    = 20
MAXSOUNDS     = 20
COLORSPERPLAYER         = 64
COLOROFFSETPLAYER1      = 112
COLOROFFSETPLAYER2      = 176
```

Default settings:
```cpp
ROUND_LENGTH  = 60 (seconds)
HIT_POINTS    = 100
GAME_SPEED    = 12 (ms/tick ≈ 83 fps)
MUSIC_VOLUME  = 50 (%)
SOUND_VOLUME  = 100 (%)
```

---

## Naming Conventions

| Type | Convention | Example |
|---|---|---|
| Classes | `CMixedCaps` | `CGame`, `CBackend` |
| Structs | `SMixedCaps` | `SState`, `SPlayerInfo` |
| Enums | `NameEnum` or `TNameEnum` | `FighterEnum`, `TGameMode` |
| Instance members | `m_<prefix>Name` | `m_iGameTick`, `m_poBackend` |
| Parameters | `a_<prefix>Name` | `a_iPlayer` |
| Output params | `a_<prefix>OutName` | `a_enOutFighter` |

**Type prefixes**: `i`=int, `d`=double, `c`=char, `s`=string, `p`=pointer, `r`=reference, `a`=array, `o`=object, `en`=enum, `b`=bool

---

## Game Flow

```
main()
  init()             SDL + fonts
  init2()            Perl interpreter
  DrawMainScreen()   character roster

  Main loop:
    IN_DEMO    → DoDemos()
    IN_CHAT    → ChatLoop() → DoOnlineChat() → GameLoop()
    IN_SINGLE
    IN_MULTI
    IN_NETWORK → GameLoop()
                   PlayerSelect.DoPlayerSelect()
                   DoGame()
                     CGame::Run()
                       for each round:
                         Backend::AdvancePerl() × N
                         handle input (CKeyQueue)
                         draw: Background → sprites → doodads → HUD
                   GameOver(winner)
```

---

## Rendering Pipeline (per frame)

1. `CBackground::Draw(x, y, offset)` — parallax background layers
2. Blit fighter sprites from `CRlePack::Draw()` for each player
3. Blit doodads (floating text, projectiles) from `Doodads.png` atlas
4. Draw HP bars, round counter, FPS counter (`gfx.cpp`)
5. `SDL_Flip()` — present framebuffer

Resolutions: 640×480 (default) or 800×600 (widescreen option)

---

## Network Architecture

Two separate network systems:

1. **Peer-to-peer game sync** (`MortalNetworkImpl`):
   - Direct TCP between two players
   - One acts as server (`m_bServer`), one as client
   - Master/slave alternates per round
   - Master sends Perl backend state each tick; slave sends only keystrokes
   - `CKeyQueue` in `CGame` queues keystrokes with timestamps for deterministic replay

2. **MortalNet lobby** (`OnlineChat`):
   - Central chat/matchmaking server
   - Players chat and challenge each other
   - After challenge accepted, transitions to peer-to-peer

---

## Platform Notes

| Platform | Config file | Detection |
|---|---|---|
| Linux | `~/.openmortalrc` | `LANG` env var |
| macOS | `~/Library/Preferences/OpenMortal.cfg` | CoreFoundation |
| Windows | Registry or `openmortal.ini` | Windows API |

Platform guards: `#ifdef _WINDOWS`, `#ifdef MACOSX`

---

## Build

```bash
./configure
make
make install   # optional

# With feature flags:
./configure --enable-characters --enable-additional --enable-thirdparty
```

The binary is `src/openmortal`. The editor is `editor/MortalEditor` (Qt required).

---

## Bugs Found and Fixed

The following bugs were identified via source audit and corrected. Each entry describes the file, the symptom, and the root cause.

### `Audio.cpp` — Wrong format specifier in `LoadSampleMap()`
- **Line 101**: `debug("File %d could not be read.\n", sFilename.c_str())` used `%d` (integer) for a `const char*`.
- **Effect**: Printed garbage/crash on systems where `int` and pointer sizes differ (64-bit).
- **Fix**: Changed `%d` → `%s`.

### `Audio.cpp` — `PlayFile()` ignored its parameter
- **Lines 221–235**: `PlayFile(const char* a_pcFileName)` hardcoded `Mix_LoadWAV("sample.wav")` instead of using `a_pcFileName`.
- **Effect**: The function was completely broken — it always played `sample.wav` regardless of the argument passed.
- **Fix**: Use `a_pcFileName` as the argument to `Mix_LoadWAV`.

### `State.cpp` — Windows language detection: missing `break` + wrong variable
- **Lines 136–144**: The `switch` on `iLangID` had no `break` statements, so every case fell through to `default`, always setting `pcLang = "en"`. Additionally the result was copied into `m_acNick` instead of `m_acLanguage`.
- **Effect**: On Windows, language detection always produced English regardless of system locale, and corrupted the nick field.
- **Fix**: Added `break` after each case; corrected destination to `m_acLanguage`.

### `State.cpp` — NULL dereference from `getenv("HOME")`
- **Lines 55, 57**: `getenv("HOME")` return value used directly in `std::string()` construction without a NULL check.
- **Effect**: Crash on startup in environments without a `HOME` environment variable (e.g. some containers or service accounts).
- **Fix**: Check return value; fall back to `"."` if NULL.

### `State.cpp` — NULL dereference from `getenv("USER")`
- **Line 201**: `strcpy(m_acNick, getenv("USER"))` — `getenv` can return NULL if `USER` is not set.
- **Effect**: Crash on startup when `getlogin_r` fails and `USER` is unset.
- **Fix**: Guard with NULL check; also switched to `strncpy` with explicit null-termination.

### `State.cpp` — Pointer comparison instead of string comparison in `SetLanguage()`
- **Line 248**: `if ( m_acLanguage != a_pcLanguage )` compared two `char*` pointers, not their contents.
- **Effect**: The guard condition was almost always true (different stack addresses), so the language was always set, but more critically it could silently skip the update if the same buffer was passed.
- **Fix**: Replaced with `strcmp( m_acLanguage, a_pcLanguage ) != 0`.

### `PlayerSelect.cpp` — NULL dereference in `SetPlayer()`
- **Lines 208–211**: `poPack->OffsetSprites(iOffset)` was called immediately after `LoadFighter()`, *before* the NULL check on the next line.
- **Effect**: If `LoadFighter()` returned NULL (fighter data file missing), the game crashed with a segfault.
- **Fix**: Moved the NULL check to immediately after `LoadFighter()`, before any use of `poPack`.

---

## Key Files Quick Reference

| Task | File |
|---|---|
| Add/modify game settings | `src/State.h`, `src/State.cpp` |
| Change game loop / phases | `src/Game.cpp` |
| Modify rendering | `src/gfx.cpp`, `src/RlePack.cpp` |
| Add sounds/music | `src/Audio.cpp`, `data/sound/soundmap.txt` |
| Modify menus | `src/menu.cpp` |
| Change input mapping | `src/Event.h`, `src/Joystick.cpp` |
| Fighter logic/moves | `data/characters/NAME.pl` |
| Game physics/collision | `data/script/Collision.pl`, `Damage.pl` |
| Network protocol | `src/MortalNetworkImpl.cpp` |
| Perl ↔ C++ bridge | `src/Backend.cpp` |
| Localization strings | `data/script/Translate.pl` |
| Background/arena | `src/Background.cpp`, `data/gfx/levelN.*` |
| Add a new character | New `.pl` + `.dat` in `data/characters/`, extend `FighterEnum` in `src/FighterEnum.h` |

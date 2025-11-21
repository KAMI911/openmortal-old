/***************************************************************************
                          Game.h  -  description
                             -------------------
    begin                : Mon Aug 27 2003
    copyright            : (C) 2003 by upi
    email                : upi@apocalypse.rulez.org
 ***************************************************************************/


#ifndef GAME_H
#define GAME_H

/**
\defgroup GameLogic Game logic (frontend + backend connection)

This group of classes implement the frontend part of the game logic, 
including the game object itself, menus, and connection to the backend.
*/

#include <string>
#include <vector>
#include <list>

struct SDL_Surface;
class Background;




/**
\ingroup GameLogic
CKeyQueue is used to introduce a certain amount of negative or positive
lag to keystrokes.

The keys are sent to the backend and the remote site set to a future time.
The time is measured in the backend's game ticks. So if the current key
is pressed at game time 80, and enqueued to game time 90, there will be an
artificial lag of 10 game ticks.

The actual length of game ticks depends on the game speed (set in the menu),
for a normal game its 1000/80 = 12.5 ms. This artificial lag is useful for
network games.
*/

class CKeyQueue
{
public:
	CKeyQueue();
	~CKeyQueue();

	void Reset();
	void EnqueueKey( int a_iAtTime, int a_iPlayer, int a_iKey, bool a_bDown );
	void DequeueKeys( int a_iToTime );

protected:
	struct SEnqueuedKey
	{
		int		iTime;
		int		iPlayer;
		int		iKey;
		bool	bDown;
	};

	typedef std::list<SEnqueuedKey> TEnqueuedKeyList;
	TEnqueuedKeyList m_oKeys;
};




/**
\ingroup GameLogic

The CGame class is for running the frontend of a game.

This involves reading the game state data from a source (be it a replay
file or the backend), handling the keystrokes and network, etc.
*/

class Game
{
public:
	Game( bool a_bIsReplay, bool m_bWide, bool a_bDebug );
	~Game();
	int Run();
	std::string& GetReplay();
	void DoReplay( const char* a_pcReplayFile );
	static int GetBackgroundNumber();
	
protected:
	void Draw();
	void DrawHitPointDisplay( int a_iPlayer);
	void DrawHitPointDisplays();
	void DrawBackground();
	void DrawDoodads();
	void DrawPoly( const char* a_pcName, int a_iColor );
	void AddBodyToBackground( int a_iPlayer );
	
	void DoOneRound();
	
	void Advance( int a_iNumFrames );
	int ProcessEvents();
	void HandleKey( int a_iPlayer, int a_iKey, bool a_bDown );
	void HandleKO();
	
	void HurryUp();
	void TimeUp();
	void InstantReplay( int a_iKoAt );

	bool IsTeamMode();
	bool IsNetworkGame();
	bool IsMaster();
	void ReadKeysFromNetwork();
	
protected:
	
	static int			mg_iBackgroundNumber;

	bool				m_bIsReplay;
	bool				m_bWide;		///< 800 or 640 pixel width.
	int					m_iYOffset;		///< For wide mode.
	bool				m_bDebug;
	Background*			m_poBackground;
	SDL_Surface*		m_poDoodads;

	int					m_aiHitPointDisplayX[MAXPLAYERS];
	int					m_aiHitPointDisplayY[MAXPLAYERS];
	bool				m_abHitPointDisplayLeft[MAXPLAYERS];
	
	int					m_aiRoundsWonByPlayer[MAXPLAYERS];
	int					m_iNumberOfRounds;
	int					m_iFrame;
	int					m_iGameTime;
	CKeyQueue			m_oKeyQueue;
	int					m_iEnqueueDelay;
	
	std::string			m_sReplayString;
	std::vector<int>	m_aReplayOffsets;
	
	enum TGamePhaseEnum		// This enum assumes its values during DoOneRound
	{
		Ph_START,			// "Round X" displayed, fighters getting ready
		Ph_NORMAL,			// During the fight
		Ph_TIMEUP,			// Time is up, no KO, no replay.
		Ph_KO,				// There is a KO, forward until the guy is down
		Ph_REWIND,			// There was a KO, rewinding until before the KO
		Ph_SLOWFORWARD,		// Playing back the KO
		
		Ph_REPLAY,			// Replay mode
	}					m_enGamePhase;
	
	SState::TGameMode	m_enInitialGameMode;	// must make sure it's still the same.
};

#endif

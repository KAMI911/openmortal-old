/***************************************************************************
                          State.h  -  description
                             -------------------
    begin                : Mon Aug 12 2003
    copyright            : (C) 2003 by upi
    email                : upi@apocalypse.rulez.org
 ***************************************************************************/


#ifndef STATE_H
#define STATE_H


#define MAXPLAYERS 4

/**
\ingroup GameLogic
SState aggregates all the state variables of OpenMortal that do not belong
to the backend. This includes transient variables such as the current
game mode (e.g. SStade::IN_DEMO) and configuration variables (such as the
keyboard layout).

SState is a singlular object, and is accessed with a global pointer, 
g_oState. All other frontend modules access the state through this object. 
The state is made persistent through it's methods, Load() and Save(). 
Load() is called on program start, Save() is called when the program exits.


The State is the way the CMenu communicate with the rest of the system. 
For example, if the user chooses "Quit" from the menu, the m_bQuitFlag is 
set to true, and the program will react accordingly.


The state's most important properties are:

\li m_enGameMode: The mode changes when a game is started or the game ends 
(either in the GameOver screen, or via the "Surrender Game" menu option).
\li m_bQuitFlag: This is set if the program receives a quit event from the operating system (e.g. KILL signal, window close event, etc), or the user chooses "Quit" from the menu. Once the quit flag is set, the program will abort. All main loops check the value of the quit flag, and will break as soon as it is true.
\li m_bFullScreen: Quite simply, it is true in fullscreen mode, and false in windowed mode. The user can change this via the menu. The State's ToggleFullscreen() method will switch between fullscreen and windowed mode. Maybe this functionality doesn't belong to the State? ...
\li Sound properties: Mixing rate, number of channels, volume, etc.
\li m_aiPlayerKeys: A double array of each player's keys. This is used most often in processing SDL key events: if the event's keysym matches a value in m_aiPlayerKeys, that means that a meaningful key was pushed or released.
*/

struct SState
{
	enum TGameMode {
		IN_DEMO,				///< The game is currently in "demo" mode: displaying the intro screens, etc.
		IN_SINGLE,				///< The game is in single-player mode.
		IN_MULTI,				///< The game is in multi-player mode.
		IN_NETWORK,				///< There is against a network opponent in progress
		IN_CHAT,				///< The user is on MortalNet
	} m_enGameMode;

	bool	m_bQuitFlag;		// true if quit event came
	const char* m_pcArgv0;		// Set by main to argv[0]
	

	// CONFIGURATION VARIABLES
	
	int		m_iNumPlayers;		// The number of players =2
	enum TTeamModeEnum {
		Team_ONE_VS_ONE,
		Team_GOOD_VS_EVIL,
		Team_CUSTOM,
	} m_enTeamMode;				// Team mode
	int		m_iTeamSize;		// The size of each team.
	int		m_bTeamMultiselect;	// Can the same player be selected twice?
	
	int		m_iGameTime;		// Time of rounds in seconds.
	int		m_iHitPoints;		// The initial number of hit points.
	int		m_iGameSpeed;		// The speed of the game (fps = 1000/GameSpeed)
	
	bool	m_bFullscreen;		// True in fullscreen mode.
	
	int		m_iChannels;		// 1: mono, 2: stereo
	int		m_iMixingRate;		// The mixing rate, in kHz
	int		m_iMixingBits;		// 1: 8bit, 2: 16bit
	int		m_iMusicVolume;		// Volume of music; 0: off, 100: max
	int		m_iSoundVolume;		// Volume of sound effects; 0: off, 100: max
	
	int		m_aiPlayerKeys[MAXPLAYERS][9];	// Player keysyms
	char	m_acLanguage[10];	// Language ID (en,hu,fr,es,..)
	int		m_iLanguageCode;	// Non-persistend language code (set by backend based on the language)
	
	char	m_acLatestServer[256];	// Last server
	bool	m_bServer;			// We were server in the last network game
	char	m_acNick[128];		// The user name on the last server.
	
	SState();
	
	void Load();
	void Save();
	void ToggleFullscreen();
	void SetLanguage( const char* a_pcLanguage );
	void SetServer( const char* a_pcServer );
};


extern SState g_oState;

#endif

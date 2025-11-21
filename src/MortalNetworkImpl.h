/***************************************************************************
                          MortalNetworkImpl.h  -  description
                             -------------------
    begin                : Sun Jan 25 2004
    copyright            : (C) 2004 by upi
    email                : upi@feel
 ***************************************************************************/

#ifndef MORTALNETWORKIMPL_H
#define MORTALNETWORKIMPL_H


#include "MortalNetwork.h"
#include "SDL_net.h"
#include <string>
#include <list>


typedef std::list<int> TIntList;
typedef std::list<std::string> TStringList;

/**
\ingroup Network
Implementation of the CMortalNetwork interface.

SUMMARY OF MESSAGES:

I <version> <username> - Introduction sent both ways on connection.
U <text>			- Remote user's name
M <text>			- Incoming Msg text.

S					- Ready for the next round (synch).
G <text>			- Update on the game backend data.
T <number> <number>	- Update the game time and game phase.
K <number> <bool>	- Key # up/down
H <number>			- Hurryup and other special messages
O <number> <bool>	- The round is over (who won, are there more rounds).

F <number>			- I have switched to fighter X.
R					- I have chosen a fighter.
Q <number>			- Is fighter X available?
A <number>			- Fighter A is available.
P <number> x3		- Game parameters

*/
 
class CMortalNetworkImpl: public CMortalNetwork
{
public:
	CMortalNetworkImpl();
	virtual ~CMortalNetworkImpl();
	
	// Connection's lifecycle

	bool		Start( const char* a_pcServerName );
	void		Stop();					// Disconnect
	bool		IsConnectionAlive();	// Is the connection still good?
	void		Update();				// Read network traffic. Might get disconnected...
	const char*	GetLastError();
	bool		IsMaster();				// Am I Master or Slave?

	// Msg related methods

	const char*	GetRemoteUsername();	// This is the name that is passed upon connection.
	void		SendMsg( const char* a_pcMsg );	// Prompt the user for a line of chat text
	bool		IsMsgAvailable();		// Returns true is a chatline has arrived
	const char* GetMsg();				// The next chatline, or NULL if there are no more.

	// Charater Selection methods

	bool		IsRemoteFighterAvailable( FighterEnum a_enFighter );	// Does the other computer have fighter X installed?
	FighterEnum	GetRemoteFighter();		// Returns the latest fighter chosen by the remote side.
	bool		IsRemoteSideReady();	// The other player is finished choosing.

	void		SendFighter( FighterEnum a_enFighter );	// Let the other side know that I switched to fighter X.
	void		SendReady();			// Let the other side know that I am ready.
	void		SendGameParams( int a_iGameSpeed, int a_iGameTime, int a_iHitPoints, int a_iBackgroundNumber );
	SGameParams	GetGameParams();

	// Game methods

	bool		SynchStartRound();
	void		SendGameData( const char* a_pcGameData );
	const char*	GetLatestGameData();
	
	void		SendKeystroke( int a_iTime, int a_iKey, bool a_bPressed );
	bool		GetKeystroke( int& a_riOutTime, int& a_riOutKey, bool& a_rbPressed );

	void		SendGameTick( int a_iGameTick );
	int			GetGameTick();

	void		SendHurryup( int a_iHurryUpCode );
	int			GetHurryup() ;

	void		SendRoundOver( int a_iWhoWon, bool a_bGameOver );
	int			GetWhoWon();
	bool		IsRoundOver();
	bool		IsGameOver();

protected:
	void		SendRawData( char a_cID, const void* a_pData, int a_iLength );
	
	void		ReceiveMsg( void* a_pData, int a_iLength );
	void		ReceiveRemoteUserName( void* a_pData, int a_iLength );
	void		ReceiveGameData( void* a_pData, int a_iLength );
	void		ReceiveKeystroke( void* a_pData, int a_iLength );
	void		ReceiveFighter( void* a_pData, int a_iLength );
	void		ReceiveReady( void* a_pData, int a_iLength );
	void		ReceiveRoundOver( void* a_pData, int a_iLength );
	void		ReceiveGameTick( void* a_pData, int a_iLength );
	void		ReceiveHurryup( void* a_pData, int a_iLength );
	void		ReceiveRemoteFighterAvailable( void* a_pData, int a_iLength );
	void		ReceiveRemoteFighterQuery( void* a_pData, int a_iLength );
	void		ReceiveGameParams( void* a_pData, int a_iLength );
	
protected:
	
	enum TNetworkState
	{
		NS_DISCONNECTED,
		NS_CHARACTER_SELECTION,
		NS_IN_GAME,
	};

	// Network METADATA
	
	bool					m_bNetworkAvailable;		///< Is the networking API initialized correctly?
	TNetworkState			m_enState;					///< The current state
	bool					m_bServer;					///< We are the server side.
	bool					m_bMaster;					///< We are the master side. (Initially the server side)
	TCPsocket				m_poSocket;					///< The TCP/IP network socket.
	SDLNet_SocketSet		m_poSocketSet;				///< SDLNet construct for watching the socket.

	char					m_acIncomingBuffer[2048];	///< Received data goes here.
	int						m_iIncomingBufferSize;		///< How much of the buffer is filled?
	
	std::string				m_sLastError;				///< The last error message from SDLNet
	TStringList				m_asMsgs;					///< Incoming chatlines
	
	// GAME DATA

	std::string				m_sRemoteUserName;
	TIntList				m_aiAvailableRemoteFighters;
	FighterEnum				m_enRemoteFighter;
	bool					m_bRemoteReady;
	SGameParams				m_oGameParams;
	
	std::string				m_sLatestGameData;
	TIntList				m_aiKeyTimes;
	TIntList				m_aiKeystrokes;
	TIntList				m_abKeystrokes;

	bool					m_bRoundOver;
	int						m_iWhoWon;
	bool					m_bGameOver;
	int						m_iGameTick;
	int						m_iHurryupCode;
	
	// REMOTE QUERY RESPONSES
	
	bool					m_bSynchQueryResponse;
};


#endif // MORTALNETWORKIMPL_H


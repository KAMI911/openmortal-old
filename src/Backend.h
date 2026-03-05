/***************************************************************************
                          Backend.h  -  description
                             -------------------
    begin                : Sun Dec 8 2002
    copyright            : (C) 2002 by upi
    email                : upi@apocalypse.rulez.org
 ***************************************************************************/


#ifndef BACKEND_H
#define BACKEND_H

#include <string>
#include "FighterEnum.h"

class RlePack;


#define MAXDOODADS 20
#define MAXSOUNDS 20


/**
\class CBackend
\ingroup GameLogic
\brief The CBackend class provides access to the perl game engine.

The backend maintains just about all game-relevant data, such as fighters,
player information, doodads, current game state, and so forth. CBackend
provides access for the frontend to the backend's variables and functions.
Some of this is done via custom methods (such as GetNumberOfFighters()),
but certain functions are only available via the "generic" perl interface,
PerlEvalF().

It is the CBackend's job to provide variables which describe the current
\i scene to the frontend. The backend can

\li Read the scene from the Perl backend.
\li Write the scene into a string
\li Read the scene from a string.

The string conversion routines are used for saving replays and instant 
playback.
*/

class Backend
{
public:

	// Lifecycle
	
	Backend();
	~Backend();
	bool Construct();
	
	// Miscellaneous
	
	const char* PerlEvalF( const char* a_pcFormat, ... );
	const char* GetPerlString( const char* a_pcScalarName );
	int GetPerlInt( const char* a_pcScalarName );

	// Fighter enumeration
	
	int GetNumberOfFighters();
	FighterEnum GetFighterID( int a_iIndex );
	int GetNumberOfAvailableFighters();
	
	// Game data
	
	void AdvancePerl();
	void ReadFromPerl();
	bool IsDead( int a_iPlayer );
	void PlaySounds();
	void WriteToString( std::string& a_rsOutString );
	void ReadFromString( const std::string& a_rsString );
	void ReadFromString( const char* a_pcBuffer );
	
	
public:
	int				m_iGameTick;
	int				m_iGameOver;
	bool			m_bKO;
	int				m_iBgX, m_iBgY;
	int				m_iNumDoodads;
	int				m_iNumSounds;
	
	struct SPlayer
	{
		int m_iX, m_iY, m_iFrame, m_iHitPoints, m_iRealHitPoints;
	}				m_aoPlayers[MAXPLAYERS];
	
	struct SDoodad
	{
		int m_iX, m_iY, m_iType, m_iFrame;
		int m_iDir, m_iGfxOwner;
		std::string m_sText;
	}				m_aoDoodads[ MAXDOODADS ];

	std::string		m_asSounds[ MAXSOUNDS ];
};

extern Backend g_oBackend;

int			DoGame( char* replay, bool isReplay, bool bDebug );

#endif

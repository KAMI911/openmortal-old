/***************************************************************************
                          common.cpp  -  description
                             -------------------
    begin                : Fri Aug 24 2001
    copyright            : (C) 2001 by upi
    email                : upi@apocalypse.rulez.org
 ***************************************************************************/

#include "config.h"

#include <stdio.h>
#include <stdarg.h>

#include "common.h"
#include "State.h"
#include "Joystick.h"
#include "SDL.h"
#include "Event.h"
#include "BossKey.h"


SDL_Surface* gamescreen = NULL;

void debug( const char* format, ... )
{
	va_list ap;
	va_start( ap, format );
	vfprintf( stderr, format, ap );
	va_end( ap );
}



bool FindPlayerKey( SDLKey a_enKey, int& a_riOutPlayer, int& a_riOutKey )
{
	for (int i=0; i<MSZ_MAXPLAYERS; i++)
	{
		for (int j=0; j<9; j++ )
		{
			if (g_oState.m_aiPlayerKeys[i][j] == a_enKey )
			{
				a_riOutPlayer = i;
				a_riOutKey = j;
				return true;
			}
		}
	}
	return false;
}



/**
TranslateEvent is an important function in the OpenMortal event processing
chain. It takes an SDL_Event and converts it to a game related event. This
allows for the "transparent" handling of joysticks and such.

\param a_poInEvent		The SDL event which is to be translated.
\param a_poOutEvent		The output event.

\returns true if the event could be translated (it was relevant for the game),
or false if it couldn't (a_poOutEvent will be set to Me_NOTHING).
*/

bool TranslateEvent( const SDL_Event* a_poInEvent, SMortalEvent* a_poOutEvent )
{
	a_poOutEvent->m_enType = Me_NOTHING;
	
	switch ( a_poInEvent->type )
	{
	case SDL_QUIT:
		a_poOutEvent->m_enType = Me_QUIT;
		g_oState.m_bQuitFlag = true;
		return true;
	
	case SDL_KEYDOWN:
	{
		SDLKey enKey = a_poInEvent->key.keysym.sym;
#ifdef MSZ_BOSS_KEY
		if ( enKey == SDLK_ESCAPE &&
		     (a_poInEvent->key.keysym.mod & (KMOD_LSHIFT | KMOD_RSHIFT)) )
		{
			ShowBossKey();
			a_poOutEvent->m_enType = Me_NOTHING;
			return false;
		}
#endif
		if ( enKey == SDLK_ESCAPE )
		{
			a_poOutEvent->m_enType = Me_MENU;
			return true;
		}
		if ( enKey == SDLK_F1 )
		{
			a_poOutEvent->m_enType = Me_SKIP;
			return true;
		}
		
		// Check the player keys
		int iPlayer;
		int iKey;
		bool bFound = FindPlayerKey( enKey, iPlayer, iKey );

		if ( bFound )
		{
			a_poOutEvent->m_enType = Me_PLAYERKEYDOWN;
			a_poOutEvent->m_iPlayer = iPlayer;
			a_poOutEvent->m_iKey = iKey;
			return true;
		}
		break;
	}
		
	case SDL_KEYUP:
	{
		SDLKey enKey = a_poInEvent->key.keysym.sym;
		int iPlayer;
		int iKey;
		bool bFound = FindPlayerKey( enKey, iPlayer, iKey );

		if ( bFound )
		{
			a_poOutEvent->m_enType = Me_PLAYERKEYUP;
			a_poOutEvent->m_iPlayer = iPlayer;
			a_poOutEvent->m_iKey = iKey;
			return true;
		}
		break;
	}

	case SDL_JOYAXISMOTION:
	case SDL_JOYBALLMOTION:
	case SDL_JOYHATMOTION:
	case SDL_JOYBUTTONDOWN:
	case SDL_JOYBUTTONUP:
	{
		return g_oJoystick.TranslateEvent( a_poInEvent, a_poOutEvent );
	}


	} // switch

	return false;
}



/** MortalPollEvent works like SDL_PollEvent, except that it
returns an SMortalEvent.
*/

bool MortalPollEvent( SMortalEvent& a_roOutEvent )
{
	SDL_Event oEvent;
	
	while ( SDL_PollEvent( &oEvent ) )
	{
		if ( TranslateEvent( &oEvent, &a_roOutEvent ) )
		{
			return true;
		}
	}

	a_roOutEvent.m_enType = Me_NOTHING;
	return false;
}


/** MortalWaitEvent works like SDL_WaitEvent, except that it
returns an SMortalEvent.
*/

void MortalWaitEvent( SMortalEvent& a_roOutEvent )
{
	SDL_Event oEvent;

	if ( SDL_WaitEvent( &oEvent ) )
	{
		if ( TranslateEvent( &oEvent, &a_roOutEvent ) )
		{
			return;
		}
	}

	a_roOutEvent.m_enType = Me_NOTHING;
	return;
}



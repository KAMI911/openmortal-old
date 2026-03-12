#ifndef PLAYERSELECTCONTROLLER_H
#define PLAYERSELECTCONTROLLER_H

#include "SDL.h"
#include "FighterEnum.h"
#include "common.h"

class CPlayerSelectView;

/**
Controller part model-view-controller architecture of the player selection.
\ingroup PlayerSelect
*/

class CPlayerSelectController
{
public:
	CPlayerSelectController( bool a_bNetworkGame );
	~CPlayerSelectController();

	void				DoPlayerSelect();

protected:
	void				HandleEvents();
	void				HandleKey( int a_iPlayer, int a_iKey );
	bool				HandleChatKey( SDL_Event& a_roEvent );
	void				HandleNetwork();
	void				GetThisTick();
	void				MarkFighters();

	void				SetPlayerActive( int a_iPlayer, bool a_bActive );
	void				ActivateNextPlayer( int a_iCurrentPlayer );
	int					GetTeamSize( int a_iPlayer );

	bool				IsFighterSelectable( FighterEnum a_enFighter );


protected:
	CPlayerSelectView*	m_poView;
	bool				m_bNetworkGame;
	bool				m_bTeamMode;
	bool				m_bChatActive;
	char				m_acChatMsg[256];
	int					m_iNumberOfSelectableFighters;

	bool				m_abPlayerActive[MSZ_MAXPLAYERS];

	int					m_iGameSpeed;
	int					m_iThisTick;
	int					m_iLastTick;
};


#endif // PLAYERSELECTCONTROLLER_H

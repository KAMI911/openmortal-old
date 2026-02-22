#include "PlayerSelectController.h"
#include "PlayerSelectView.h"
#include "PlayerSelect.h"
#include "Chooser.h"
#include "MortalNetwork.h"
#include "State.h"
#include "Backend.h"
#include "Event.h"
#include "Audio.h"

#include "TextArea.h"
#include "sge_tt_text.h"
#include "common.h"



int GetBackgroundNumber();



CPlayerSelectController::CPlayerSelectController( bool a_bNetworkGame )
{
	m_bNetworkGame = a_bNetworkGame;
	m_bTeamMode = SState::Team_CUSTOM == g_oState.m_enTeamMode;
	m_poView = new CPlayerSelectView( a_bNetworkGame, m_bTeamMode );
}


CPlayerSelectController::~CPlayerSelectController()
{
	delete m_poView;
	m_poView = NULL;
}



void CPlayerSelectController::GetThisTick()
{
	m_iLastTick = m_iThisTick;

	while (1)
	{
		m_iThisTick = SDL_GetTicks() / m_iGameSpeed;
		if ( m_iThisTick==m_iLastTick )
		{
			SDL_Delay(1);
		}
		else
		{
			break;
		}
	}
}



bool CPlayerSelectController::HandleChatKey( SDL_Event& a_roEvent )
{
	if ( !m_bNetworkGame )
	{
		return false;
	}

	SDLKey enKey = a_roEvent.key.keysym.sym;

	if ( enKey == SDLK_PAGEUP || enKey == SDLK_KP9 )
	{
		m_poView->GetTextArea()->ScrollUp();
		return true;
	}
	if ( enKey == SDLK_PAGEDOWN || enKey == SDLK_KP3 )
	{
		m_poView->GetTextArea()->ScrollDown();
		return true;
	}

	if ( !m_bChatActive && ( SDLK_TAB == enKey 
			|| SDLK_RETURN == enKey || SDLK_KP_ENTER == enKey ) )
	{
		// Activate chat.
		m_acChatMsg[0] = 0;
		m_poView->GetReadline()->Clear();
		m_poView->GetReadline()->Restart( m_acChatMsg, strlen(m_acChatMsg), 256, C_LIGHTCYAN, C_BLACK, 255 );
		m_bChatActive = true;
		return true;
	}

	if ( !m_bChatActive )
	{
		return false;
	}

	if ( SDLK_ESCAPE == enKey || SDLK_TAB == enKey )
	{
		// Deactivate chat.
		m_poView->GetReadline()->Clear();
		m_bChatActive = false;
		return true;
	}

	// other keys..

	m_poView->GetReadline()->HandleKeyEvent( a_roEvent );
	int iResult = m_poView->GetReadline()->GetResult();

	if ( iResult < 0 )
	{
		// Escape was pressed?
		m_poView->GetReadline()->Clear();
		m_bChatActive = false;
	}
	if ( iResult > 0 )
	{
		if ( strlen( m_acChatMsg ) )
		{
			g_poNetwork->SendMsg( m_acChatMsg );
			std::string sMsg = std::string("<") + g_oState.m_acNick + "> " + m_acChatMsg;
			m_poView->GetTextArea()->AddString( sMsg.c_str(), C_LIGHTCYAN );
			m_poView->GetTextArea()->Redraw();
			
			m_acChatMsg[0] = 0;
			m_poView->GetReadline()->Clear();
			m_poView->GetReadline()->Restart( m_acChatMsg, strlen(m_acChatMsg), 256, C_LIGHTCYAN, C_BLACK, 255 );
		}
		else
		{
			m_poView->GetReadline()->Clear();
			m_bChatActive = false;
		}
	}

	return true;
}



void CPlayerSelectController::HandleEvents()
{
	SDL_Event oSdlEvent;
	while (SDL_PollEvent(&oSdlEvent))
	{
		if ( m_bNetworkGame
			&& SDL_KEYDOWN == oSdlEvent.type )
		{
			if (HandleChatKey( oSdlEvent ))
			{
				continue;
			}
		}
		
		if ( m_bNetworkGame 
			&& SDL_KEYDOWN == oSdlEvent.type
			&& (oSdlEvent.key.keysym.sym == SDLK_RETURN || oSdlEvent.key.keysym.sym==SDLK_KP_ENTER) )
		{
			m_bChatActive = true;
			continue;
		}
		
		SMortalEvent oEvent;
		TranslateEvent( &oSdlEvent, &oEvent );

		switch ( oEvent.m_enType )
		{
			case Me_QUIT:
				g_oState.m_bQuitFlag = true;
				break;
			
			case Me_MENU:
				DoMenu();
				if ( m_bNetworkGame && g_poNetwork->IsMaster() )
				{
					g_poNetwork->SendGameParams( g_oState.m_iGameSpeed, g_oState.m_iGameTime, g_oState.m_iHitPoints, GetBackgroundNumber() );
				}
				break;
				
			case Me_PLAYERKEYDOWN:
				HandleKey( oEvent.m_iPlayer, oEvent.m_iKey );
				break;

			case Me_NOTHING:
			case Me_SKIP:
			case Me_PLAYERKEYUP:
				break;
		} // end of switch statement
	}
}



void CPlayerSelectController::HandleKey( int a_iPlayer, int a_iKey )
{
	if ( m_bNetworkGame )
	{
		a_iPlayer = g_poNetwork->IsMaster() ? 0 : 1;
	}
	
	if ( !m_abPlayerActive[a_iPlayer] )
	{
		return;
	}

	int iSetPlayer = a_iPlayer;
	if ( m_bTeamMode && !g_oState.m_bTeamMultiselect ) 
	{
		iSetPlayer = 0;
	}

	// MOVE THE CURSOR

	if ( a_iKey < 4 )
	{
		g_oChooser.MoveRectangle( a_iPlayer, a_iKey );
		FighterEnum enNewFighter = g_oChooser.GetCurrentFighter( a_iPlayer );
		if ( enNewFighter != g_oPlayerSelect.GetPlayerInfo( iSetPlayer ).m_enFighter )
		{
			Audio->PlaySample("PLAYER_SELECTION_CHANGES");
			if ( IsFighterSelectable(enNewFighter) )
			{
				if ( m_bNetworkGame )
				{
					g_poNetwork->SendFighter( enNewFighter );
				}

				g_oPlayerSelect.SetPlayer( iSetPlayer, enNewFighter );
			}
		}
		return;
	}
	
	// SELECT THE CURRENT FIGHTER
	
	FighterEnum enFighter = g_oChooser.GetCurrentFighter( a_iPlayer );
	if ( !IsFighterSelectable( enFighter ) )
	{
		// Current fighter cannot be selected.
		return;
	}
	
	Audio->PlaySample("PLAYER_SELECTED");
	g_oBackend.PerlEvalF( "PlayerSelected(%d);", iSetPlayer );
	if ( m_bNetworkGame )
	{
		g_poNetwork->SendFighter( enFighter );
		g_poNetwork->SendReady();
	}

	bool bKeepPlayerActive = false;

	PlayerInfo& roInfo = g_oPlayerSelect.EditPlayerInfo(a_iPlayer);
	const PlayerInfo& roOtherInfo = g_oPlayerSelect.GetPlayerInfo(1-a_iPlayer);
	int iTeamSize = roInfo.m_aenTeam.size() + 1;
	int iOtherTeamSize = roOtherInfo.m_aenTeam.size();

	if ( m_bTeamMode )
	{
		m_poView->AddFighterToTeam( a_iPlayer, enFighter );
		roInfo.m_aenTeam.push_back( enFighter );
		
		if ( g_oState.m_bTeamMultiselect )
		{
			SetPlayerActive( a_iPlayer, iTeamSize < g_oState.m_iTeamSize );
		}
		else
		{
			--m_iNumberOfSelectableFighters;
			g_oChooser.MarkFighter( enFighter, a_iPlayer ? C_LIGHTMAGENTA : C_LIGHTGREEN );
			ActivateNextPlayer( a_iPlayer );
		}
	}
	else
	{
		roInfo.m_aenTeam.clear();
		roInfo.m_aenTeam.push_back( enFighter );
		SetPlayerActive( a_iPlayer, false );
	}

}


void CPlayerSelectController::HandleNetwork()
{
	g_poNetwork->Update();

	// 1. READ CHAT MESSAGES

	bool bUpdateText = false;
	while ( g_poNetwork->IsMsgAvailable() )
	{
		const char* pcMsg = g_poNetwork->GetMsg();
		int iColor = C_YELLOW;
		if ( pcMsg[0] == '*' && pcMsg[1] == '*' && pcMsg[2] == '*' ) iColor = C_WHITE;
		m_poView->GetTextArea()->AddString( pcMsg, iColor );
		bUpdateText = true;
	}
	if ( bUpdateText )
	{
		Audio->PlaySample("NETWORK_MESSAGE");
		m_poView->GetTextArea()->Redraw();
	}

	// 2. READ PLAYER SELECTION ACTIONS

	bool bMaster = g_poNetwork->IsMaster();
	int iPlayer = bMaster ? 1 : 0;

	if ( !m_abPlayerActive[iPlayer] )
	{
		return;
	}

	FighterEnum enOldFighter = g_oPlayerSelect.GetPlayerInfo(iPlayer).m_enFighter;
	FighterEnum enRemoteFighter = g_poNetwork->GetRemoteFighter();

	if ( enOldFighter != enRemoteFighter
		&& enRemoteFighter != UNKNOWN )
	{
		Audio->PlaySample("PLAYER_SELECTION_CHANGES");
		g_oPlayerSelect.SetPlayer( iPlayer, enRemoteFighter );
		g_oChooser.SetRectangle( iPlayer, enRemoteFighter );
	}

	bool bDone = g_poNetwork->IsRemoteSideReady();
	if ( bDone )
	{
		m_abPlayerActive[iPlayer] = false;
		Audio->PlaySample("PLAYER_SELECTED");
		g_oBackend.PerlEvalF( "PlayerSelected(%d);", iPlayer );

		PlayerInfo& roInfo = g_oPlayerSelect.EditPlayerInfo( iPlayer );

		roInfo.m_aenTeam.clear();
		roInfo.m_aenTeam.push_back( enRemoteFighter );
		SetPlayerActive( iPlayer, false );
	}
}



void CPlayerSelectController::MarkFighters()
{
	int iNumberOfFighters = g_oBackend.GetNumberOfFighters();
	int i;
	
	for ( i=0; i<iNumberOfFighters; ++i )
	{
		FighterEnum enFighter = g_oBackend.GetFighterID( i );
		if ( !g_oPlayerSelect.IsLocalFighterAvailable( enFighter ) )
		{
			g_oChooser.MarkFighter( enFighter, C_LIGHTRED );
		}
		else if ( !g_oPlayerSelect.IsFighterAvailable( enFighter ) )
		{
			g_oChooser.MarkFighter( enFighter, C_LIGHTBLUE );
		}
	}
}



void CPlayerSelectController::SetPlayerActive( int a_iPlayer, bool a_bActive )
{
	m_abPlayerActive[a_iPlayer] = a_bActive;
	g_oChooser.SetRectangleVisible( a_iPlayer, a_bActive );
}


/** This method finds the next player and lets him choose a fighter.
This only should be called in team mode without multiselect.
*/

void CPlayerSelectController::ActivateNextPlayer( int a_iCurrentPlayer )
{
	// Go in the following order:
	// 1 2 3 4 4 3 2 1 1 2 3 4 ...

	int iNextPlayer = a_iCurrentPlayer;
	
	if ( m_iNumberOfSelectableFighters <= 0 )
	{
		SetPlayerActive( a_iCurrentPlayer, false );
		return;
	}
	
	if ( a_iCurrentPlayer == 0 ) 
	{
		iNextPlayer = 1;
	}
	else if ( g_oState.m_iNumPlayers-1 == a_iCurrentPlayer )
	{
		iNextPlayer = a_iCurrentPlayer - 1;
	}
	else
	{
		iNextPlayer = GetTeamSize(a_iCurrentPlayer-1) > GetTeamSize(a_iCurrentPlayer+1) ?
			a_iCurrentPlayer+1 : a_iCurrentPlayer-1;
	}

	if ( GetTeamSize(a_iCurrentPlayer) <= GetTeamSize(iNextPlayer) )
	{
		iNextPlayer = a_iCurrentPlayer;
	}
	
	if ( iNextPlayer != a_iCurrentPlayer )
	{
		SetPlayerActive( a_iCurrentPlayer, false );
	}
	SetPlayerActive( iNextPlayer, GetTeamSize(iNextPlayer) < g_oState.m_iTeamSize );
}


int CPlayerSelectController::GetTeamSize( int a_iPlayer )
{
	return g_oPlayerSelect.GetPlayerInfo( a_iPlayer ).m_aenTeam.size();
}



bool CPlayerSelectController::IsFighterSelectable( FighterEnum a_enFighter )
{
	if ( !g_oPlayerSelect.IsFighterAvailable( a_enFighter ) )
	{
		return false;
	}

	if ( m_bTeamMode && !g_oState.m_bTeamMultiselect && g_oPlayerSelect.IsFighterInTeam(a_enFighter) )
	{
		return false;
	}

	return true;
}



void CPlayerSelectController::DoPlayerSelect()
{
	
	// 1. INITIALIZE

	if ( SState::IN_NETWORK == g_oState.m_enGameMode )
	{
		g_oState.m_enTeamMode = SState::Team_ONE_VS_ONE;
		g_oState.m_iNumPlayers = 2;
	}

	int i;
	for ( i=0; i<g_oState.m_iNumPlayers; ++i )
	{
		g_oPlayerSelect.SetPlayer( i, g_oChooser.GetCurrentFighter(i) );
		SetPlayerActive( i, !m_bTeamMode || g_oState.m_bTeamMultiselect || 0 == i );
	}
	m_iNumberOfSelectableFighters = g_oBackend.GetNumberOfAvailableFighters();
	MarkFighters();

	if ( m_bTeamMode )
	{
		for ( int i=0; i<g_oState.m_iNumPlayers; ++i )
		{
			g_oPlayerSelect.EditPlayerInfo(i).m_aenTeam.clear();
		}
	}

	if ( m_bNetworkGame && g_poNetwork->IsMaster() )
	{
		g_poNetwork->SendGameParams( g_oState.m_iGameSpeed, g_oState.m_iGameTime, g_oState.m_iHitPoints, GetBackgroundNumber() );
	}

	g_oBackend.PerlEvalF( "SelectStart(%d);", g_oState.m_iNumPlayers );

	m_bChatActive = false;

	m_iGameSpeed = 12 ;
	m_iThisTick = SDL_GetTicks() / m_iGameSpeed;
	m_iLastTick = m_iThisTick - 1;

	// 2. RUN THE PLAYER SELECTION LOOP

	while (1)
	{
		// 2.1. Wait for the next tick (on extremely fast machines..)
		GetThisTick();
		int iAdvance = m_iThisTick - m_iLastTick;
		if ( iAdvance > 5 )
			iAdvance = 5;

		// 2.2. Handle events
		HandleEvents();
		if ( m_bNetworkGame )
		{
			HandleNetwork();
		}

		// 2.3. Update the view
		m_poView->Advance( iAdvance );
		m_poView->Draw();

		if ( g_oState.m_bQuitFlag || SState::IN_DEMO == g_oState.m_enGameMode) 
			break;
		
		//@ FIX THIS
		
		if ( !m_abPlayerActive[0] && !m_abPlayerActive[1] && m_poView->IsOver() )
			break;
	}
}

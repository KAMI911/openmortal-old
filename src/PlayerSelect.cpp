/***************************************************************************
                          PlayerSelect.cpp  -  description
                             -------------------
    begin                : Sun Dec 8 2002
    copyright            : (C) 2002 by upi
    email                : upi@apocalypse.rulez.org
 ***************************************************************************/

#include <stdio.h>

#include "PlayerSelect.h"
#include "PlayerSelectController.h"
 
#include "SDL.h"
#include "SDL_video.h"
#include "SDL_image.h"
#include "sge_primitives.h"
#include "sge_surface.h"
 
#include "common.h"
#include "Audio.h"
#include "sge_bm_text.h"
#include "gfx.h"
#include "RlePack.h"
#include "Backend.h"
#include "State.h"
#include "MortalNetwork.h"
#include "Chooser.h"
#include "sge_tt_text.h"
#include "TextArea.h"
#include "Event.h"


#ifndef NULL
#define NULL 0
#endif


/***************************************************************************
                     PUBLIC EXPORTED VARIABLES
***************************************************************************/


PlayerSelect g_oPlayerSelect;








PlayerSelect::PlayerSelect()
{
	for ( int i=0; i<MAXPLAYERS; ++i )
	{
		m_aoPlayers[i].m_enFighter = UNKNOWN;
		m_aoPlayers[i].m_enTint = NO_TINT;
		m_aoPlayers[i].m_poPack = NULL;
	}
}



/*************************************************************************
							GENERAL PLAYER INFO
*************************************************************************/

/**
Returns the info of a given player. The most important piece in the info 
is the player's CRlePack which is used for drawing the player in the Game, 
the GameOver and the FighterInfo screens.
*/

const PlayerInfo& PlayerSelect::GetPlayerInfo( int a_iPlayer )
{
	return m_aoPlayers[ a_iPlayer ];
}


PlayerInfo& PlayerSelect::EditPlayerInfo( int a_iPlayer )
{
	return m_aoPlayers[ a_iPlayer ];
}


const char* PlayerSelect::GetFighterName( int a_iPlayer )
{
	return m_aoPlayers[ a_iPlayer ].m_sFighterName.c_str();
}


int PlayerSelect::GetFighterNameWidth( int a_iPlayer )
{
	return m_aiFighterNameWidth[ a_iPlayer ];
}

/**
Returns true if the given fighter can be used. 
Some fighters are just not in the data files yet, or not installed.
*/

bool PlayerSelect::IsFighterAvailable( FighterEnum a_enFighter )
{
	if ( a_enFighter <= UNKNOWN )
	{
		return false;
	}
	
	bool bLocalAvailable = IsLocalFighterAvailable( a_enFighter );

	if ( SState::IN_NETWORK != g_oState.m_enGameMode || !bLocalAvailable )
	{
		return bLocalAvailable;
	}
	
	// Check the remote site
	return g_poNetwork->IsRemoteFighterAvailable( a_enFighter );
}


bool PlayerSelect::IsLocalFighterAvailable( FighterEnum a_enFighter )
{
	if ( a_enFighter <= UNKNOWN )
	{
		return false;
	}

	g_oBackend.PerlEvalF("GetFighterStats(%d);", a_enFighter);
	const char* pcDatafile = g_oBackend.GetPerlString("Datafile");
	return pcDatafile && *pcDatafile;
}



bool PlayerSelect::IsFighterInTeam( FighterEnum a_enFighter )
{
	std::vector<FighterEnum>::const_iterator it;
	for ( int i=0; i<g_oState.m_iNumPlayers; ++i )
	{
		std::vector<FighterEnum>& roTeam = m_aoPlayers[i].m_aenTeam;
		for ( it=roTeam.begin(); it!=roTeam.end(); ++it )
		{
			if ( a_enFighter == *it )
			{
				return true;
			}
		}
	}

	return false;
}



/** LoadFighter simply looks up the filename associated with the given
fighter, loads it, and returns the RlePack.

\return The freshly loaded RlePack, or NULL if it could not be loaded.
*/

RlePack* PlayerSelect::LoadFighter( FighterEnum m_enFighter )		// static
{
	char a_pcFilename[FILENAME_MAX+1];
	const char* s;

	g_oBackend.PerlEvalF( "GetFighterStats(%d);", m_enFighter );
	s = g_oBackend.GetPerlString( "Datafile" );

	strcpy( a_pcFilename, DATADIR );
	strcat( a_pcFilename, "/characters/" );
	strcat( a_pcFilename, s );

	RlePack* pack = new RlePack( a_pcFilename, COLORSPERPLAYER );
	if ( pack->Count() <= 0 )
	{
		debug( "Couldn't load RlePack: '%s'\n", a_pcFilename );
		delete pack;
		return NULL;
	}

	return pack;
}



/** SetPlayer loads the given fighter for the given player.

The RlePack is loaded first. If that succeeds, then the perl backend is
set too. The tint and palette of both players are set. */

void PlayerSelect::SetPlayer( int a_iPlayer, FighterEnum a_enFighter )
{
	if ( m_aoPlayers[a_iPlayer].m_enFighter == a_enFighter )
	{
		if ( m_aoPlayers[a_iPlayer].m_poPack )
		{
			m_aoPlayers[a_iPlayer].m_poPack->ApplyPalette();
		}
		return;
	}
	if ( !IsFighterAvailable( a_enFighter ) )
	{
		return;
	}

	int iOffset = COLOROFFSETPLAYER1 + a_iPlayer*64;
	RlePack* poPack = LoadFighter( a_enFighter );
	poPack->OffsetSprites( iOffset );

	if ( NULL == poPack )
	{
		debug( "SetPlayer(%d,%d): Couldn't load RlePack\n", a_iPlayer, a_enFighter );
		return;
	}

	delete m_aoPlayers[a_iPlayer].m_poPack;
	m_aoPlayers[a_iPlayer].m_poPack = poPack;
	m_aoPlayers[a_iPlayer].m_enFighter = a_enFighter;

	g_oBackend.PerlEvalF( "SetPlayerNumber(%d,%d);", a_iPlayer, a_enFighter );
	m_aoPlayers[a_iPlayer].m_sFighterName = g_oBackend.GetPerlString( "PlayerName" );
	m_aiFighterNameWidth[a_iPlayer] = sge_BF_TextSize( fastFont, GetFighterName(a_iPlayer) ).w;

	TintEnum enTint = NO_TINT;

	//@ CLASHES WITH OTHER PLAYERS NEED TO BE HANDLED
	if ( m_aoPlayers[0].m_enFighter == m_aoPlayers[1].m_enFighter )
	{
		enTint = TintEnum( (rand() % 4) + 1 );
	}
	SetTint( 1, enTint );
	m_aoPlayers[a_iPlayer].m_poPack->ApplyPalette();
}



void PlayerSelect::SetTint( int a_iPlayer, TintEnum a_enTint )
{
	m_aoPlayers[a_iPlayer].m_enTint = a_enTint;
	if ( m_aoPlayers[a_iPlayer].m_poPack )
	{
		m_aoPlayers[a_iPlayer].m_poPack->SetTint( a_enTint );
		m_aoPlayers[a_iPlayer].m_poPack->ApplyPalette();
	}
}


/*************************************************************************
			METHODS RELATED TO THE FIGHTER SELECTION PROCESS
*************************************************************************/


/*
bool PlayerSelect::IsNetworkGame()
{
	return SState::IN_NETWORK == g_oState.m_enGameMode;
}



FighterEnum PlayerSelect::GetFighterCell( int a_iIndex )
{
	if ( IsNetworkGame() )
	{
		return ChooserCellsChat[a_iIndex/m_iChooserCols][a_iIndex%m_iChooserCols];
	}
	else
	{
		return ChooserCells[a_iIndex/m_iChooserCols][a_iIndex%m_iChooserCols];
	}
}



void PlayerSelect::HandleKey( int a_iPlayer, int a_iKey )
{
	// If we are in network mode, all keys count as the local player's...
	if ( IsNetworkGame() )
	{
		a_iPlayer = g_poNetwork->IsMaster() ? 0 : 1;
	}

	int& riP = a_iPlayer ? m_iP2 : m_iP1;
	int iOldP = riP;

	bool& rbDone = a_iPlayer ? m_bDone2 : m_bDone1;
	if ( rbDone )
	{
		return;
	}

	switch ( a_iKey )
	{
		case 0:		// up
			if ( riP >= m_iChooserCols ) riP -= m_iChooserCols;
			break;
		case 1:		// down
			if ( (riP/m_iChooserCols) < (m_iChooserRows-1) ) riP += m_iChooserCols;
			break;
		case 2:		// left
			if ( (riP % m_iChooserCols) > 0 ) riP--;
			break;
		case 3:		// right
			if ( (riP % m_iChooserCols) < (m_iChooserCols-1) ) riP++;
			break;
		default:
			if ( IsFighterAvailable( GetFighterCell(riP) ) )
			{
				Audio->PlaySample("magic.voc");

				rbDone = true;
				g_oBackend.PerlEvalF( "PlayerSelected(%d);", a_iPlayer );
				if ( IsNetworkGame() )
				{
					g_poNetwork->SendFighter( GetFighterCell(riP) );
					g_poNetwork->SendReady();
				}
				return;
			}
	}

	if ( iOldP != riP )
	{
		Audio->PlaySample("strange_quack.voc");
		if ( IsFighterAvailable( GetFighterCell(riP) ) )
		{
			if ( IsNetworkGame() )
			{
				g_poNetwork->SendFighter( GetFighterCell(riP) );
			}
			SetPlayer( a_iPlayer, GetFighterCell(riP) );
		}
	}
}


void PlayerSelect::HandleNetwork()
{
	g_poNetwork->Update();

	bool bUpdateText = false;
	while ( g_poNetwork->IsMsgAvailable() )
	{
		const char* pcMsg = g_poNetwork->GetMsg();
		int iColor = C_YELLOW;
		if ( pcMsg[0] == '*' && pcMsg[1] == '*' && pcMsg[2] == '*' ) iColor = C_WHITE;
		m_poTextArea->AddString( pcMsg, iColor );
		bUpdateText = true;
	}
	if ( bUpdateText )
	{
		Audio->PlaySample("pop.wav");
		m_poTextArea->Redraw();
	}

	bool bMaster = g_poNetwork->IsMaster();
	int iPlayer = bMaster ? 1 : 0;
	int& riP = bMaster ? m_iP2 : m_iP1;
	bool& rbDone = bMaster ? m_bDone2 : m_bDone1;

	if ( rbDone )
	{
		return;
	}

	int iOldP = riP;
	FighterEnum enOldFighter = GetFighterCell(iOldP);
	FighterEnum enRemoteFighter = g_poNetwork->GetRemoteFighter();

	if ( enOldFighter != enRemoteFighter
		&& enRemoteFighter != UNKNOWN )
	{
		Audio->PlaySample("strange_quack.voc");
		SetPlayer( iPlayer, enRemoteFighter );
		int i, j;
		for ( i=0; i<m_iChooserRows; ++i )
		{
			for ( int j=0; j<m_iChooserCols; ++j )
			{
				if ( ChooserCellsChat[i][j] == enRemoteFighter )
				{
					riP = i * m_iChooserCols + j;
					break;
				}
			}
		}
	}

	bool bDone = g_poNetwork->IsRemoteSideReady();
	if ( bDone )
	{
		rbDone = true;
		Audio->PlaySample("magic.voc");
		g_oBackend.PerlEvalF( "PlayerSelected(%d);", iPlayer );
	}
}


void PlayerSelect::DrawRect( int a_iPos, int a_iColor )
{
	int iRow = a_iPos / m_iChooserCols;
	int iCol = a_iPos % m_iChooserCols;
	SDL_Rect r, r1;

	r.x = m_iChooserLeft + iCol * m_iChooserWidth;
	r.y = m_iChooserTop  + iRow * m_iChooserHeight;
	r.w = m_iChooserWidth + 5;
	r.h = 5;
	r1 = r;
	SDL_FillRect( gamescreen, &r1, a_iColor );

	r.y += m_iChooserHeight;
	r1 = r;
	SDL_FillRect( gamescreen, &r1, a_iColor );

	r.y -= m_iChooserHeight;
	r.w = 5;
	r.h = m_iChooserHeight + 5;
	r1 = r;
	SDL_FillRect( gamescreen, &r1, a_iColor );

	r.x += m_iChooserWidth;
	r1 = r;
	SDL_FillRect( gamescreen, &r1, a_iColor );
}


void PlayerSelect::CheckPlayer( SDL_Surface* a_poBackground, int a_iRow, int a_iCol, int a_iColor )
{
	int x1, y1;

	x1 = m_iChooserLeft + a_iCol * m_iChooserWidth +5;
	y1 = m_iChooserTop  + a_iRow * m_iChooserHeight +5;

	sge_Line(a_poBackground, x1+5, y1+5, x1 + m_iChooserWidth-10, y1 + m_iChooserHeight-10, a_iColor);
	sge_Line(a_poBackground, x1 + m_iChooserWidth-10, y1+5, x1+5, y1 + m_iChooserHeight-10, a_iColor);
	x1++;
	sge_Line(a_poBackground, x1+5, y1+5, x1 + m_iChooserWidth-10, y1 + m_iChooserHeight-10, a_iColor);
	sge_Line(a_poBackground, x1 + m_iChooserWidth-10, y1+5, x1+5, y1 + m_iChooserHeight-10, a_iColor);
	y1++;
	sge_Line(a_poBackground, x1+5, y1+5, x1 + m_iChooserWidth-10, y1 + m_iChooserHeight-10, a_iColor);
	sge_Line(a_poBackground, x1 + m_iChooserWidth-10, y1+5, x1+5, y1 + m_iChooserHeight-10, a_iColor);
	x1--;
	sge_Line(a_poBackground, x1+5, y1+5, x1 + m_iChooserWidth-10, y1 + m_iChooserHeight-10, a_iColor);
	sge_Line(a_poBackground, x1 + m_iChooserWidth-10, y1+5, x1+5, y1 + m_iChooserHeight-10, a_iColor);
}
*/



#if 0

void PlayerSelect::DoPlayerSelect()
{
	// 1. Set up: Load background, mark unavailable fighters

	bool bNetworkMode = IsNetworkGame();

	if ( bNetworkMode )
	{
		m_iChooserLeft = 158;
		m_iChooserTop = 26;
		m_iChooserHeight = 64;
		m_iChooserWidth = 64;
		m_iChooserRows = 4;
		m_iChooserCols = 5;
	}
	else
	{
		m_iChooserLeft = 158;
		m_iChooserTop = 74;
		m_iChooserHeight = 80;
		m_iChooserWidth = 80;
		m_iChooserRows = 5;
		m_iChooserCols = 4;
	}

	SDL_FillRect( gamescreen, NULL, C_BLACK );
	SDL_Flip( gamescreen );

	SDL_Surface* poBackground = LoadBackground( bNetworkMode ? "PlayerSelect_chat.png" : "PlayerSelect.png", 111 );

	if ( !bNetworkMode )
	{
		DrawGradientText( "Choose A Fighter Dammit", titleFont, 10, poBackground );
	}
	//g_oChooser.Draw( poBackground );
	
	int i, j;
	for ( i=0; i<m_iChooserRows; ++i )
	{
		for ( int j=0; j<m_iChooserCols; ++j )
		{
			if ( bNetworkMode )
			{
				if ( !IsLocalFighterAvailable(ChooserCellsChat[i][j]) &&
					UNKNOWN != ChooserCellsChat[i][j] )
				{
					CheckPlayer( poBackground, i, j, C_LIGHTRED );
				}
				else if ( !IsFighterAvailable(ChooserCellsChat[i][j]) &&
					UNKNOWN != ChooserCellsChat[i][j] )
				{
					CheckPlayer( poBackground, i, j, C_LIGHTBLUE );
				}
			}
			else
			{
				if ( !IsFighterAvailable(ChooserCells[i][j]) &&
					UNKNOWN != ChooserCells[i][j] )
				{
					CheckPlayer( poBackground, i, j, C_LIGHTRED );
				}
			}
		}
	}

	for ( i=0; i<2; ++i )
	{
		if ( m_aoPlayers[i].m_poPack ) m_aoPlayers[i].m_poPack->ApplyPalette();
	}

	SetPlayer( 0, GetFighterCell(m_iP1) );
	SetPlayer( 1, GetFighterCell(m_iP2) );

	if ( bNetworkMode && g_poNetwork->IsMaster() )
	{
		g_poNetwork->SendGameParams( g_oState.m_iGameSpeed, g_oState.m_iGameTime, g_oState.m_iHitPoints, GetBackgroundNumber() );
	}

	// 2. Run selection screen

	g_oBackend.PerlEvalF( "SelectStart(%d);", g_oState.m_iNumPlayers );

	m_bDone1 = m_bDone2 = false;

	int thisTick, lastTick, gameSpeed;

	gameSpeed = 12 ;
	thisTick = SDL_GetTicks() / gameSpeed;
	lastTick = thisTick - 1;

			i		= 0;
	int		over	= 0;

	int		iCourtain = 0;
	int		iCourtainSpeed = 0;
	int		iCourtainTime = 80;

	SDL_Event event;

	// Chat is 165:318 - 470:470
	char acMsg[256];
	sprintf( acMsg, "Press Enter to chat, Page Up/Page Down to scroll..." );
	bool bDoingChat = false;

	if ( bNetworkMode )
	{
		m_poReadline = new CReadline( IsNetworkGame() ? poBackground : NULL, chatFont,
			acMsg, strlen(acMsg), 256, 15, 465, 610, C_LIGHTCYAN, C_BLACK, 255 );
		m_poTextArea = new CTextArea( poBackground, chatFont, 15, 313, 610, 32*4 );
	}
	else
	{
		m_poReadline = NULL;
		m_poTextArea = NULL;
	}

	while (1)
	{
		// 1. Wait for the next tick (on extremely fast machines..)
		while (1)
		{
			thisTick = SDL_GetTicks() / gameSpeed;
			if ( thisTick==lastTick )
			{
				SDL_Delay(1);
			}
			else
			{
				break;
			}
		}

		// 2. Advance as many ticks as necessary..

		if ( iCourtainTime > 0 )
		{
			int iAdvance = thisTick - lastTick;
			if ( iAdvance > 5 ) iAdvance = 5;

			if ( iCourtain + iCourtainSpeed * iCourtainTime /2 < 320 * 4 )
				iCourtainSpeed += iAdvance;
			else
				iCourtainSpeed -= iAdvance;

			iCourtain += iCourtainSpeed * iAdvance;
			iCourtainTime -= iAdvance;

			if ( iCourtainTime > 0 )
			{
				SDL_Rect oRect;
				oRect.x = 320 - iCourtain/4; oRect.y = 0;
				oRect.w = iCourtain / 2; oRect.h = gamescreen->h;
				if ( oRect.x < 0 ) oRect.x = 0;
				if ( oRect.w > gamescreen->w ) oRect.w = gamescreen->w;
				SDL_SetClipRect( gamescreen, &oRect );
			}
			else
			{
				SDL_SetClipRect( gamescreen, NULL );
			}
		}

		int iNumFrames = thisTick - lastTick;
		if ( iNumFrames>5 ) iNumFrames = 5;
		for ( i=0; i<iNumFrames; ++i )
		{
			g_oBackend.AdvancePerl();
		}
		lastTick = thisTick;

		while (SDL_PollEvent(&event))
		{
			if ( SDL_QUIT == event.type )
			{
				g_oState.m_bQuitFlag = true;
				break;
			}

			// HANDLE SCROLLING THE TEXT AREA

			if ( event.type == SDL_KEYDOWN && IsNetworkGame() )
			{
				SDLKey enKey = event.key.keysym.sym;
				if ( enKey == SDLK_PAGEUP || enKey == SDLK_KP9 )
				{
					m_poTextArea->ScrollUp();
					continue;
				}
				if ( enKey == SDLK_PAGEDOWN || enKey == SDLK_KP3 )
				{
					m_poTextArea->ScrollDown();
					continue;
				}

			}
			
			// HANDLE CHATTING
			
			if ( bDoingChat && SDL_KEYDOWN==event.type )
			{
				// The chat thingy will handle this event.
				m_poReadline->HandleKeyEvent( event );
				int iResult = m_poReadline->GetResult();
				if ( iResult < 0 )
				{
					// Escape was pressed?
					m_poReadline->Clear();
					bDoingChat = false;
				}
				if ( iResult > 0 )
				{
					if ( strlen( acMsg ) )
					{
						g_poNetwork->SendMsg( acMsg );
						std::string sMsg = std::string("<") + g_oState.m_acNick + "> " + acMsg;
						m_poTextArea->AddString( sMsg.c_str(), C_LIGHTCYAN );
						m_poTextArea->Redraw();
						m_poReadline->Clear();
						acMsg[0] = 0;
						m_poReadline->Restart( acMsg, strlen(acMsg), 256, C_LIGHTCYAN, C_BLACK, 255 );
					}
					else
					{
						m_poReadline->Clear();
						bDoingChat = false;
					}
				}
				continue;
			}

			// HANDLE OTHER TYPES OF EVENTS

			if ( IsNetworkGame() && bDoingChat == false &&
				SDL_KEYDOWN == event.type &&
				(event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym==SDLK_KP_ENTER) )
			{
				bDoingChat = true;
				acMsg[0] = 0;
				m_poReadline->Clear();
				m_poReadline->Restart( acMsg, strlen(acMsg), 256, C_LIGHTCYAN, C_BLACK, 255 );
				break;
			}
				
			SMortalEvent oEvent;
			TranslateEvent( &event, &oEvent );
			
			switch ( oEvent.m_enType )
			{
				case Me_QUIT:
					g_oState.m_bQuitFlag = true;
					break;
				
				case Me_MENU:
					DoMenu();
					if ( IsNetworkGame() && g_poNetwork->IsMaster() )
					{
						g_poNetwork->SendGameParams( g_oState.m_iGameSpeed, g_oState.m_iGameTime, g_oState.m_iHitPoints, GetBackgroundNumber() );
					}
					break;
					
				case Me_PLAYERKEYDOWN:
					DrawRect( m_iP1, C_BLACK );
					DrawRect( m_iP2, C_BLACK );
					HandleKey( oEvent.m_iPlayer, oEvent.m_iKey );
					break;

				case Me_NOTHING:
				case Me_SKIP:
				case Me_PLAYERKEYUP:
					break;
			} // end of switch statement
			
		}	// Polling events

		if ( IsNetworkGame() )
		{
			HandleNetwork();
		}

		g_oBackend.ReadFromPerl();
		over = g_oBackend.m_iGameOver;

		SDL_BlitSurface( poBackground, NULL, gamescreen, NULL );
		if ( !m_bDone1) DrawRect( m_iP1, C_LIGHTGREEN );
		if ( !m_bDone2) DrawRect( m_iP2, C_LIGHTMAGENTA );

		for ( i=0; i<2; ++i )
		{
			int iYOffset = bNetworkMode ? -130 : 0;
			if ( g_oBackend.m_aoPlayers[i].m_iFrame )
			{
				m_aoPlayers[i].m_poPack->Draw(
					ABS(g_oBackend.m_aoPlayers[i].m_iFrame)-1,
					g_oBackend.m_aoPlayers[i].m_iX, g_oBackend.m_aoPlayers[i].m_iY + iYOffset,
					g_oBackend.m_aoPlayers[i].m_iFrame < 0 );
			}
			int x = ( m_iChooserLeft - m_aiFighterNameWidth[i] ) / 2;
			if ( x<10 ) x = 10;
			if ( i ) x = gamescreen->w - x - m_aiFighterNameWidth[i];
			
			sge_BF_textout( gamescreen, fastFont, GetFighterName(i),
				x, gamescreen->h - 30 + iYOffset - (bNetworkMode ? 40 : 0) );
		}
		
		SDL_Flip( gamescreen );
		
		if (over || g_oState.m_bQuitFlag || SState::IN_DEMO == g_oState.m_enGameMode) break;
	}

	delete m_poReadline;
	delete m_poTextArea;
	SDL_FreeSurface( poBackground );
	SDL_SetClipRect( gamescreen, NULL );
	return;
}

#else

void PlayerSelect::DoPlayerSelect()
{
	CPlayerSelectController oController( SState::IN_NETWORK == g_oState.m_enGameMode );
	oController.DoPlayerSelect();
}

#endif

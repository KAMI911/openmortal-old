/***************************************************************************
                         GameOver.cpp  -  description
                             -------------------
    begin                : Wed Aug 21 20:25:30 CEST 2003
    copyright            : (C) 2003 by upi
    email                : upi@apocalypse.rulez.org
 ***************************************************************************/

#include "PlayerSelect.h"
#include "SDL.h"
#include "gfx.h"
#include "Backend.h"
#include "common.h"
#include "State.h"
#include "RlePack.h"
#include "Audio.h"
#include "Event.h"

#include <stdio.h>





void DrawPlayer( int i )
{
	int iFrame = g_oBackend.m_aoPlayers[i].m_iFrame;
	if ( 0 != iFrame )
	{
		g_oPlayerSelect.GetPlayerInfo(i).m_poPack->Draw( 
			ABS(iFrame)-1,
			g_oBackend.m_aoPlayers[i].m_iX,
			g_oBackend.m_aoPlayers[i].m_iY - 15,
			iFrame<0 );
	}
}



void GameOver( int a_iPlayerWon )
{
	SDL_Surface* poBackground = LoadBackground( "GameOver.jpg", 112 );
	DrawGradientText( "Final Judgement", titleFont, 20, poBackground );
	DrawTextMSZ( "Continue?", inkFont, 320, 100, AlignHCenter, C_LIGHTCYAN, poBackground );
	SDL_Surface* poFoot = LoadBackground( "Foot.jpg", 112, 0, true );
	
	SDL_BlitSurface( poBackground, NULL, gamescreen, NULL );
	
	g_oBackend.PerlEvalF( "JudgementStart(%d);", a_iPlayerWon );
	
	int thisTick, lastTick, firstTick, iGameSpeed;
	iGameSpeed = 14 ;
	firstTick = thisTick = SDL_GetTicks() / iGameSpeed;
	lastTick = thisTick - 1;
	
	char acString[100];
	int iTimeLeft = 8000 / iGameSpeed;
	int FOOTHEIGHT = poFoot->h;
	int GROUNDLEVEL = 440;
	int iFootY = -FOOTHEIGHT;
	bool bTimeUp = false;
	bool bKeyPressed = false;
	int iCounter = -1;
	
	while (1)
	{
		// 1. Wait for the next tick (on extremely fast machines..)
		while (1)
		{
			thisTick = SDL_GetTicks() / iGameSpeed;
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
		
		if ( thisTick - lastTick > 5 )
		{
			iTimeLeft -= 5;
		}
		else
		{
			iTimeLeft = iTimeLeft - thisTick + lastTick;
		}
		
		if ( iTimeLeft < 0 && !bTimeUp )
		{
			bTimeUp = true;
			Audio->FadeMusic( 1500 );
		}
		
		if ( bTimeUp )
		{
			iFootY += 12 * (thisTick - lastTick );
			if ( iFootY > GROUNDLEVEL - FOOTHEIGHT )
			{
				break;
			}
		}

		int iNumFrames = thisTick - lastTick;
		for ( int i=0; i<iNumFrames; ++i )
		{
			g_oBackend.AdvancePerl();
		};
		lastTick = thisTick;

		SMortalEvent oEvent;
		while (MortalPollEvent(oEvent))
		{
			switch (oEvent.m_enType)
			{
				case Me_QUIT:
					g_oState.m_bQuitFlag = true;
					break;

				case Me_MENU:
					DoMenu();
					break;
				
				case Me_PLAYERKEYDOWN:
					if ( 1-a_iPlayerWon == oEvent.m_iPlayer
						&& oEvent.m_iKey>= 4 )
					{
						bKeyPressed = true;
					}
					break;

				case Me_NOTHING:
				case Me_PLAYERKEYUP:
				case Me_SKIP:
					break;
			}	// switch statement
		}	// Polling events
		
		g_oBackend.ReadFromPerl();
		
		SDL_BlitSurface( poBackground, NULL, gamescreen, NULL );
		
		DrawPlayer( a_iPlayerWon );
		
		if ( bTimeUp )
		{
			SDL_Rect oRect;
			oRect.x = 0;
			oRect.y = iFootY + FOOTHEIGHT - 10;
			oRect.w = gamescreen->w;
			oRect.h = gamescreen->h - oRect.y;
			SDL_SetClipRect(gamescreen, &oRect);
		}
		
		DrawPlayer( 1-a_iPlayerWon );

		SDL_SetClipRect( gamescreen, NULL );
		
		if ( !bTimeUp )
		{
			int iNewCounter = iTimeLeft * iGameSpeed / 1000;
			sprintf( acString, "%d", iNewCounter );
			DrawTextMSZ( acString, inkFont, 320, 130, AlignHCenter, C_LIGHTCYAN, gamescreen );
			if ( iNewCounter != iCounter )
			{
				Audio->PlaySample( "GAME_OVER_COUNTDOWN" );
				iCounter = iNewCounter;
			}
		}
		else
		{
			SDL_Rect oRect;
			oRect.x = 40;
			oRect.y = iFootY;
			SDL_BlitSurface( poFoot, NULL, gamescreen, &oRect );
		}
		
		SDL_Flip( gamescreen );
		
		if ( g_oState.m_bQuitFlag || 
			SState::IN_DEMO == g_oState.m_enGameMode || 
			bKeyPressed )
		{
			break;
		}
	}
	
	if ( g_oState.m_bQuitFlag || 
		SState::IN_DEMO == g_oState.m_enGameMode )
	{
		SDL_FreeSurface( poBackground );
		return;
	}
	
	
	SDL_FillRect( gamescreen, NULL, C_BLACK );
	if ( bTimeUp )
	{
		SDL_UnlockSurface( gamescreen );
		DrawGradientText( "SPLAT!", titleFont, 220, gamescreen );
		Audio->PlaySample( "GAME_OVER_SPLAT" );
		SDL_Flip( gamescreen );
		SDL_Delay( 1500 );
		g_oState.m_enGameMode = SState::IN_DEMO;
	}
	else
	{
		SDL_Flip( gamescreen );
	}

	SDL_FreeSurface( poBackground );
	return;
}



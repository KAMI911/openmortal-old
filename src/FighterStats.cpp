/***************************************************************************
                          FighterStats.cpp  -  description
                             -------------------
    begin                : Tue Dec 10 2002
    copyright            : (C) 2002 by upi
    email                : upi@apocalypse.rulez.org
 ***************************************************************************/

#include <stdio.h>
#include <string>

#include "PlayerSelect.h"
#include "FlyingChars.h"
#include "SDL.h"
#include "SDL_video.h"
#include "SDL_image.h"
#include "sge_tt_text.h"

#include "common.h"
#include "gfx.h"
#include "RlePack.h"
#include "Backend.h"
#include "State.h"
#include "FighterStats.h"

#include "MszPerl.h"


extern PerlInterpreter*	my_perl;





#define LEFTMARGIN		160
#define TOPMARGIN		70
#define RIGHTMARGIN		630
#define LINEHEIGHT		35
#define GAPWIDTH		20
#define DESCMARGIN		50


#define LEFTMARGIN2		((LEFTMARGIN+RIGHTMARGIN+GAPWIDTH)/2)

/*
void DrawMultiLineText( const char* text, _sge_TTFont* font, int x, int y,
	int fg, SDL_Surface* target )
{
	const char* s1, * s2;
	char line[1024];
	
	s1 = s2 = text;
	while (1)
	{
		while ( isspace(*s2) ) s2++;
		if ( !*s2 )
			break;

		s1 = s2;	// s1: start of line.
		while ( (*s2 != '\n') && *s2 ) s2++;
		strncpy( line, s1, s2-s1 );
		line[s2-s1] = 0;
		DrawTextMSZ( line, font, x, y, 0, fg, target );
		y += sge_TTF_FontLineSkip( font );
	}
}
*/





int CFighterStatsDemo::mg_iLastFighter = -1;
FighterEnum CFighterStatsDemo::mg_aenFighterOrder[LASTFIGHTER-1];


CFighterStatsDemo::CFighterStatsDemo( FighterEnum a_iFighter )
{
	m_iTimeLeft = 500;
	m_poStaff = NULL;
	
	m_poBackground = LoadBackground( "FighterStats.jpg", 64 );
	DrawGradientText( "Fighter Stats", titleFont, 10, m_poBackground );

	SDL_BlitSurface( m_poBackground, NULL, gamescreen, NULL );
	SDL_Flip( gamescreen );
	
	if ( mg_iLastFighter < 0 )
	{
		// First run; create shuffled array of fighters.
		mg_iLastFighter = 0;
		
		int i, j;
		FighterEnum k;
		for ( i=0; i<LASTFIGHTER-1; ++i )
		{
			mg_aenFighterOrder[i] = (FighterEnum)(i+1);
		}
		for ( i=0; i<LASTFIGHTER-1; ++i )
		{
			j = rand() % (LASTFIGHTER-1);
			k = mg_aenFighterOrder[i]; 
			mg_aenFighterOrder[i] = mg_aenFighterOrder[j]; 
			mg_aenFighterOrder[j] = k;
		}
	}
	
	if ( a_iFighter <= UNKNOWN )
	{
		mg_iLastFighter = (mg_iLastFighter+1) % (LASTFIGHTER-1);
		m_enFighter = mg_aenFighterOrder[mg_iLastFighter];
	}
	else
	{
		m_enFighter = a_iFighter;
	}

	if ( g_oPlayerSelect.IsFighterAvailable( m_enFighter ) )
	{
		g_oPlayerSelect.SetPlayer( 0, m_enFighter );
		g_oBackend.PerlEvalF( "SelectStart(%d);", 2 );
	}
	else
	{
		std::string sStaffFilename = DATADIR;
		sStaffFilename += "/characters/staff.dat";
		m_poStaff = new CRlePack( sStaffFilename.c_str(), 255 );
	}

	g_oBackend.PerlEvalF("GetFighterStats(%d);", m_enFighter );
	_sge_TTFont* font = impactFont;
	int y = TOPMARGIN;


	AV *StatTags = get_av( "StatTags", FALSE );

	char *s, *sTag;

	s = SvPV_nolen(get_sv("Name", FALSE));
	DrawTextMSZ( s, inkFont, (LEFTMARGIN + RIGHTMARGIN)/2, y, AlignHCenter, C_WHITE, m_poBackground );
	y+= 10;

	s = SvPV_nolen(get_sv("Team", FALSE ));
	sTag = SvPV_nolen( *av_fetch( StatTags, 1, false ) );
	int i = DrawTextMSZ( sTag, font, LEFTMARGIN, y+=LINEHEIGHT, 0, C_YELLOW, m_poBackground );
	DrawTextMSZ( s, font, LEFTMARGIN+i, y, 0, C_ORANGE, m_poBackground, false );

	s = SvPV_nolen(get_sv("Style", FALSE ));
	sTag = SvPV_nolen( *av_fetch( StatTags, 2, false ) );
	i = DrawTextMSZ( sTag, font, LEFTMARGIN2, y, 0, C_YELLOW, m_poBackground );
	DrawTextMSZ( s, font, LEFTMARGIN2+i, y, 0, C_ORANGE, m_poBackground, false );

	s = SvPV_nolen(get_sv("Age", FALSE ));
	sTag = SvPV_nolen( *av_fetch( StatTags, 3, false ) );
	i = DrawTextMSZ( sTag, font, LEFTMARGIN, y+=LINEHEIGHT, 0, C_YELLOW, m_poBackground );
	DrawTextMSZ( s, font, LEFTMARGIN+i, y, 0, C_ORANGE, m_poBackground, false );

	s = SvPV_nolen(get_sv("Weight", FALSE ));
	sTag = SvPV_nolen( *av_fetch( StatTags, 4, false ) );
	i = DrawTextMSZ( sTag, font, LEFTMARGIN2, y, 0, C_YELLOW, m_poBackground );
	DrawTextMSZ( s, font, LEFTMARGIN2+i, y, 0, C_ORANGE, m_poBackground, false );

	s = SvPV_nolen(get_sv("Height", FALSE ));
	sTag = SvPV_nolen( *av_fetch( StatTags, 5, false ) );
	i = DrawTextMSZ( sTag, font, LEFTMARGIN, y+=LINEHEIGHT, 0, C_YELLOW, m_poBackground );
	DrawTextMSZ( s, font, LEFTMARGIN+i, y, 0, C_ORANGE, m_poBackground, false );

	s = SvPV_nolen(get_sv("Shoe", FALSE ));
	sTag = SvPV_nolen( *av_fetch( StatTags, 6, false ) );
	i = DrawTextMSZ( sTag, font, LEFTMARGIN2, y, 0, C_YELLOW, m_poBackground );
	DrawTextMSZ( s, font, LEFTMARGIN2+i, y, 0, C_ORANGE, m_poBackground, false );

	m_sStory = SvPV_nolen(get_sv("Story", FALSE ));
	SDL_Rect oFlyingRect;
	oFlyingRect.x = LEFTMARGIN; 
	oFlyingRect.y = y+DESCMARGIN;
	oFlyingRect.w = gamescreen->w - oFlyingRect.x - 20;
	oFlyingRect.h = gamescreen->h - oFlyingRect.y - 10;
	m_poFlyingChars = new CFlyingChars( creditsFont, oFlyingRect );
	m_poFlyingChars->AddText( m_sStory.c_str(), CFlyingChars::FC_AlignJustify, false );

	if ( g_oPlayerSelect.IsFighterAvailable( m_enFighter ) )
	{
		m_sKeys = SvPV_nolen(get_sv("Keys", TRUE ));
		m_poFlyingChars->AddText( "\n\nKEYS\n", CFlyingChars::FC_AlignCenter, true );
		m_poFlyingChars->AddText( m_sKeys.c_str(), CFlyingChars::FC_AlignCenter, true );
	}
	else
	{
		m_sKeys = Translate("Unfortunately this fighter is not yet playable.");
		m_poFlyingChars->AddText( m_sKeys.c_str(), CFlyingChars::FC_AlignLeft, true );
	}
}



CFighterStatsDemo::~CFighterStatsDemo()
{
	delete m_poStaff;
}



int CFighterStatsDemo::Advance( int a_iNumFrames, bool a_bFlip )
{
	if ( a_iNumFrames > 5 ) a_iNumFrames = 5;
	
	if ( m_poFlyingChars->IsDone() )
	{
		m_iTimeLeft -= a_iNumFrames;
	}
	
	AdvanceFlyingChars( a_iNumFrames );
	SDL_BlitSurface( m_poBackground, NULL, gamescreen, NULL );
	m_poFlyingChars->Draw();
	
	// 2. Advance as many ticks as necessary..
	
	if ( g_oPlayerSelect.IsFighterAvailable( m_enFighter ) )
	{
		for (int i=0; i<a_iNumFrames; ++i )
		{
			g_oBackend.AdvancePerl();
		}
		int p1x = SvIV(get_sv("p1x", TRUE));
		int p1y = SvIV(get_sv("p1y", TRUE));
		int p1f = SvIV(get_sv("p1f", TRUE));
		if (p1f) g_oPlayerSelect.GetPlayerInfo(0).m_poPack->Draw( omABS(p1f)-1, p1x, p1y, p1f<0 );
	}
	
	if ( SState::IN_DEMO != g_oState.m_enGameMode )
	{
		sge_BF_textout( gamescreen, fastFont, Translate("Press F1 to skip..."), 230, 450 );
	}
	
	SDL_Flip( gamescreen );
	
	return (m_iTimeLeft > 0) ? 0 : 1;
}


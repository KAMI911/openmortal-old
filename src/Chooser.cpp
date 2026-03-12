/***************************************************************************
                          Chooser.cpp  -  description
                             -------------------
    begin                : Tue Jan 27 2004
    copyright            : (C) 2004 by upi
    email                : upi@feel
 ***************************************************************************/


#include "Chooser.h"
#include "PlayerSelect.h"
#include "Backend.h"
#include "Event.h"
#include "State.h"

#include "SDL_image.h"
#include "gfx.h"
#include "common.h"
#include "sge_primitives.h"


CChooser g_oChooser;


CChooser::CChooser()
{
	m_iNumberOfFighters = -1;
	m_poScreen = NULL;
	for ( int i=0; i<MAXPLAYERS; ++i )
	{
		m_abRectangleVisible[i] = false;
		m_aiPlayerPosition[i] = 0;
	}
	x1=y1=0;
	x2=y2=1;
	m_iRows = m_iCols = 1;
}


CChooser::~CChooser()
{
	// Should free the portraits..
}


void CChooser::Init()
{
	if ( m_iNumberOfFighters >= 0 )
	{
		// Should only run once.
		return;
	}
	
	m_aiColors[0] = C_LIGHTGREEN;
	m_aiColors[1] = C_LIGHTMAGENTA;
	m_aiColors[2] = C_YELLOW;
	m_aiColors[3] = C_LIGHTBLUE;

	// 1. Query the list of fighters from PlayerSelect.
	
	m_iNumberOfFighters = g_oBackend.GetNumberOfFighters();
	if ( m_iNumberOfFighters > 100 )
	{
		m_iNumberOfFighters = 100;
	}
	if ( m_iNumberOfFighters < 0 )
	{
		m_iNumberOfFighters = 0;
	}

	char pcFilename[FILENAME_MAX+1];
	const char* s;
	int i;
	
	for ( i=0; i<m_iNumberOfFighters; ++i )
	{
		// Load the info of fighter #i
		FighterEnum enFighter = g_oBackend.GetFighterID( i );
		debug( "Fighter %d ID is %d\n", i, enFighter );
		m_aenFighters[i] = enFighter;
		
		// Load the portrait of fighter #i
		g_oBackend.PerlEvalF( "GetFighterStats(%d);", enFighter );
		s = g_oBackend.GetPerlString( "Portrait" );
		
		snprintf( pcFilename, FILENAME_MAX+1, "%s/characters/%s", OM_DATADIR, s );
		
		m_apoPortraits[i] = IMG_Load( pcFilename );
		if ( m_apoPortraits[i] ) SDL_SetColorKey( m_apoPortraits[i], 0, 0 );
	}
	for ( i=m_iNumberOfFighters; i<100; ++i )
	{
		m_aenFighters[i] = UNKNOWN;
		m_apoPortraits[i] = NULL;
	}
	
	Resize( 158, 74, 483, 479 );

	for ( i=0; i<MAXPLAYERS; ++i )
	{
		m_aiPlayerPosition[i] = FighterToPosition( g_oPlayerSelect.GetPlayerInfo(i).m_enFighter );
	}
}


#define LINEWIDTH 5

void CChooser::Draw()
{
	Init();
	
	int i;

	for ( i = 0; i<m_iCols * m_iRows; ++i )
	{

		// Blit portrait # i

		if ( i >= m_iNumberOfFighters
			|| NULL == m_apoPortraits[i] )
		{
			DrawRectangle( i, C_BLACK );
			continue;
		}
		
		// Calculate the bounding rectangle of the character portrait
		
		SDL_Rect oDst = GetRect(i);
		SDL_Rect oSrc;
		oSrc.x = (m_apoPortraits[i]->w - oDst.w) / 2;
		oSrc.y = (m_apoPortraits[i]->h - oDst.h) / 2;
		oSrc.w = oDst.w;
		oSrc.h = oDst.h;

		Uint32 iBgColor = m_aenFighters[i] < 100 ? C_YELLOW: C_LIGHTBLUE;
		sge_FilledRectAlpha( m_poScreen, oDst.x, oDst.y, oDst.x+oDst.w, oDst.y+oDst.h, iBgColor, 64 );
		SDL_BlitSurface( m_apoPortraits[i], &oSrc, m_poScreen, &oDst );
		DrawRectangle( i, C_BLACK );
	}
}



void CChooser::Start( SDL_Surface* a_poScreen )
{
	Init();
	m_poScreen = a_poScreen;
}



void CChooser::Stop()
{
	m_poScreen = NULL;
}



void CChooser::Resize( int a_x1, int a_y1, int a_x2, int a_y2 )
{
	Init();
	x1 = a_x1;
	y1 = a_y1;
	x2 = a_x2 + LINEWIDTH;
	y2 = a_y2 + LINEWIDTH;
	m_iRows = 1;
	m_iCols = 1;

	while ( m_iRows * m_iCols < m_iNumberOfFighters )
	{
		double dColDensity = double(x2-x1) / double(m_iCols);
		double dRowDensity = double(y2-y1) / double(m_iRows);

		if ( dRowDensity > dColDensity *1.1 )
		{
			m_iRows ++;
		}
		else
		{
			m_iCols ++;
		}
	}
}


void CChooser::MarkFighter( FighterEnum a_enFighter, Uint32 a_iColor )
{
	SDL_Rect oRect = GetFighterRect( a_enFighter );
	if ( oRect.w <= 0 )
	{
		return;
	}

	int x1 = oRect.x;
	int y1 = oRect.y;
	int w = oRect.w;
	int h = oRect.h;

	sge_Line(m_poScreen, x1+5, y1+5, x1 + w-10, y1 + h-10, a_iColor);
	sge_Line(m_poScreen, x1 + w-10, y1+5, x1+5, y1 + h-10, a_iColor);
	x1++;
	sge_Line(m_poScreen, x1+5, y1+5, x1 + w-10, y1 + h-10, a_iColor);
	sge_Line(m_poScreen, x1 + w-10, y1+5, x1+5, y1 + h-10, a_iColor);
	y1++;
	sge_Line(m_poScreen, x1+5, y1+5, x1 + w-10, y1 + h-10, a_iColor);
	sge_Line(m_poScreen, x1 + w-10, y1+5, x1+5, y1 + h-10, a_iColor);
	x1--;
	sge_Line(m_poScreen, x1+5, y1+5, x1 + w-10, y1 + h-10, a_iColor);
	sge_Line(m_poScreen, x1 + w-10, y1+5, x1+5, y1 + h-10, a_iColor);
}



SDL_Surface* CChooser::GetPortrait( FighterEnum a_enFighter )
{
	Init();

	for ( int i=0; i<m_iNumberOfFighters; ++i )
	{
		if ( m_aenFighters[i] == a_enFighter )
		{
			return m_apoPortraits[i];
		}
	}
	return NULL;
}



void CChooser::DrawPortrait( FighterEnum a_enFighter, SDL_Surface* a_poScreen, const SDL_Rect& a_roRect )
{
	SDL_Surface* poPortrait = GetPortrait( a_enFighter );

	if ( NULL == poPortrait )
	{
		return ;
	}

	SDL_Rect oDst = a_roRect;
	SDL_Rect oSrc;
	oSrc.x = (poPortrait->w - oDst.w) / 2;
	oSrc.y = (poPortrait->h - oDst.h) / 2;
	oSrc.w = oDst.w;
	oSrc.h = oDst.h;

	SDL_BlitSurface( poPortrait, &oSrc, a_poScreen, &oDst );
}



FighterEnum CChooser::GetCurrentFighter( int a_iPlayer )
{
	return PositionToFighter( m_aiPlayerPosition[ a_iPlayer ] );
}


void CChooser::MoveRectangle( int a_iPlayer, int a_iDirection )
{
	int& riPlayerPosition = m_aiPlayerPosition[ a_iPlayer ];

	int iNew = riPlayerPosition;

	switch ( a_iDirection )
	{
	case Mk_UP:		if ( iNew >= m_iCols ) iNew -= m_iCols; break;
	case Mk_DOWN:	if ( iNew / m_iCols < m_iRows-1 ) iNew += m_iCols; break;
	case Mk_LEFT:	if ( iNew % m_iCols > 0 ) --iNew; break;
	case Mk_RIGHT:	if ( iNew % m_iCols < m_iCols-1 ) ++iNew; break;
	}
	
	if ( iNew != riPlayerPosition )
	{
		ClearRectangle( a_iPlayer );
		riPlayerPosition = iNew;
		DrawRectangle( a_iPlayer );
	}
}


void CChooser::SetRectangle( int a_iPlayer, FighterEnum a_enFighter )
{
	int& riPlayerPosition = m_aiPlayerPosition[ a_iPlayer ];
	int iNew = FighterToPosition( a_enFighter );

	if ( iNew != riPlayerPosition )
	{
		ClearRectangle( a_iPlayer );
		riPlayerPosition = iNew;
		DrawRectangle( a_iPlayer );
	}
}


void CChooser::SetRectangleVisible( int a_iPlayer, bool a_bVisible )
{
	m_abRectangleVisible[ a_iPlayer ] = a_bVisible;
	if ( a_bVisible )
	{
		DrawRectangle( a_iPlayer );
	}
	else
	{
		ClearRectangle( a_iPlayer );
	}
}


bool CChooser::IsRectangleVisible( int a_iPlayer )
{
	return m_abRectangleVisible[ a_iPlayer ];
}


void CChooser::DrawRectangles( int a_iStartingWith )
{
	a_iStartingWith = a_iStartingWith % g_oState.m_iNumPlayers;
	int i = a_iStartingWith;
	
	while (1)
	{
		DrawRectangle(i);
		i = (i+1) % g_oState.m_iNumPlayers;

		if ( i==a_iStartingWith)
			break;
	}
}



int CChooser::FighterToPosition( FighterEnum a_enFighter )
{
	for ( int i=0; i<m_iNumberOfFighters; ++i )
	{
		if ( m_aenFighters[i] == a_enFighter )
		{
			return i;
		}
	}

	return 0;
}


FighterEnum CChooser::PositionToFighter( int a_iPosition )
{
	if ( a_iPosition < 0
		|| a_iPosition >= m_iNumberOfFighters )
	{
		return UNKNOWN;
	}
	return m_aenFighters[a_iPosition];
}


SDL_Rect CChooser::GetFighterRect( FighterEnum a_enFighter )
{
	return GetRect( FighterToPosition( a_enFighter ) );
}


SDL_Rect CChooser::GetRect( int a_iPosition )
{
	SDL_Rect oRect;
	oRect.x = oRect.y = oRect.w = oRect.h = 0;

	if ( a_iPosition < 0 || a_iPosition >= m_iRows*m_iCols )
		return oRect;

	int iRow = a_iPosition / m_iCols;
	int iCol = a_iPosition % m_iCols;
	oRect.x = (x2-x1-LINEWIDTH) * iCol / m_iCols + LINEWIDTH + x1;
	oRect.y = (y2-y1-LINEWIDTH) * iRow / m_iRows + LINEWIDTH + y1;
	oRect.w = (x2-x1-LINEWIDTH) * (iCol+1) / m_iCols + LINEWIDTH + x1 - oRect.x;
	oRect.h = (y2-y1-LINEWIDTH) * (iRow+1) / m_iRows + LINEWIDTH + y1 - oRect.y;
	
	return oRect;
}


void CChooser::DrawRectangle( int a_iPlayer )
{
	if ( ! m_abRectangleVisible[a_iPlayer] )
	{
		return;
	}
	
	DrawRectangle( m_aiPlayerPosition[a_iPlayer], m_aiColors[a_iPlayer] );
}


void CChooser::ClearRectangle( int a_iPlayer )
{
	DrawRectangle( m_aiPlayerPosition[a_iPlayer], C_BLACK );
}


void CChooser::DrawRectangle( int a_iPosition, Uint32 a_iColor )
{
	SDL_Rect oRect = GetRect( a_iPosition );
	oRect.x -= LINEWIDTH - 1;
	oRect.y -= LINEWIDTH - 1;

/*	===1====
	|      |
	3      4
	|      |
	===2====	*/

	SDL_Rect r = oRect;
	r.h = LINEWIDTH;
	r.w += LINEWIDTH;
	SDL_Rect r1 = r;
	SDL_FillRect( m_poScreen, &r1, a_iColor );
	
	r.y += oRect.h;
	r1 = r;
	SDL_FillRect( m_poScreen, &r1, a_iColor );

	r.y = oRect.y;
	r.w = LINEWIDTH;
	r.h = oRect.h;
	r1 = r;
	SDL_FillRect( m_poScreen, &r1, a_iColor );

	r.x += oRect.w;
	r1 = r;
	SDL_FillRect( m_poScreen, &r1, a_iColor );
}


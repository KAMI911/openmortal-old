/***************************************************************************
                          TextArea.cpp  -  description
                             -------------------
    begin                : Wed Jan 28 2004
    copyright            : (C) 2004 by upi
    email                : upi@feel
 ***************************************************************************/

#include "TextArea.h"
#include "sge_surface.h"
#include "sge_tt_text.h"
#include "sge_primitives.h"
#include "gfx.h"
#include "common.h"

#include <ctype.h>


CTextArea::CTextArea( SDL_Surface* a_poScreen, _sge_TTFont* a_poFont, int a_x, int a_y, int a_w, int a_h )
{
	m_poScreen = a_poScreen;
	m_poFont = a_poFont;
	m_iScrollOffset = 0;

	m_oClipRect.x = x = a_x;
	m_oClipRect.y = y = a_y;
	m_oClipRect.w = w = a_w;
	m_oClipRect.h = h = a_h;
	
	m_poBackground = sge_copy_surface( a_poScreen, x, y, w, h );
}


CTextArea::~CTextArea()
{
	SDL_FreeSurface( m_poBackground );
	m_poBackground = NULL;
}


void CTextArea::Clear()
{
	m_aiRowColors.clear();
	m_asRowTexts.clear();
	m_iScrollOffset = 0;
}


void CTextArea::TintBackground( int a_iColor, int a_iAlpha )
{
	if ( NULL == m_poBackground )
	{
		return;
	}
	sge_FilledRectAlpha( m_poBackground, 0, 0, m_poBackground->w, m_poBackground->h, a_iColor, a_iAlpha );
}


void CTextArea::AddString( const char* a_poText, int a_iColor )
{
	while ( a_poText && *a_poText )
	{
		// 1. FORMAT THE TEXT SO IT FITS US NICELY
		
		SDL_Rect oRect;
		int i, j;
		
		for ( i=0; a_poText[i]; ++i )
		{
			oRect = sge_TTF_TextSize( m_poFont, a_poText, i );
			if ( oRect.w > w )
			{
				if ( i>0 )--i;
				break;
			}
		}
		
		// i now points to the character AFTER the last good character.
		//if ( a_poText[i] && i>1 ) --i;
		
		// Trace back to the first space BEFORE i
		if ( a_poText[i] )
		{
			for ( j = i; j>0; --j )
			{
				if ( isspace( a_poText[j] ) )
				{
					break;
				}
			}
			if ( j>0 )
			{
				i = j;
			}
		}
		
		// 2. ADD IT.
		
		m_asRowTexts.push_front( std::string(a_poText,i) );
		m_aiRowColors.push_front( a_iColor );
		if ( m_iScrollOffset > 0 )
		{
			++m_iScrollOffset;
		}
		
		a_poText += i;
		debug( "Added '%s', number of texts is %d\n", m_asRowTexts.front().c_str(), m_asRowTexts.size() );
	}
}


void CTextArea::ScrollUp()
{
	if ( m_iScrollOffset < (int) m_asRowTexts.size() - h / sge_TTF_FontHeight(m_poFont) )
	{
		++m_iScrollOffset;
		Redraw();
	}
}


void CTextArea::ScrollDown()
{
	if ( m_iScrollOffset > 0 )
	{
		--m_iScrollOffset;
		Redraw();
	}
}


void CTextArea::Redraw()
{
	TStringList::const_iterator itString = m_asRowTexts.begin();
	TIntList::const_iterator itColors = m_aiRowColors.begin();

	for ( int i=0; i<m_iScrollOffset; ++i )
	{
		++itString;
		++itColors;
	}

	SDL_Rect oOldClipRect;
	SDL_GetClipRect( m_poScreen, &oOldClipRect );
	SDL_SetClipRect( m_poScreen, &m_oClipRect );
	sge_TTF_AAOn();
	
	sge_Blit( m_poBackground, m_poScreen, 0, 0, x, y, w, h );

	int iRows = h / sge_TTF_FontHeight( m_poFont );
	int yPos = y + (iRows-1) * sge_TTF_FontHeight( m_poFont );
	
	for ( ; yPos >= y; yPos -= sge_TTF_FontHeight( m_poFont ) )
	{
		if ( itColors == m_aiRowColors.end() )
		{
			break;
		}

		// Print the current text
		sge_tt_textout_UTF8( m_poScreen, m_poFont, itString->c_str(), x, yPos + sge_TTF_FontAscent(m_poFont), *itColors, C_BLACK, 255 );
		++itString;
		++itColors;
	}
	
	sge_UpdateRect( m_poScreen, x, y, w, h );
	SDL_SetClipRect( m_poScreen, &oOldClipRect );
	sge_TTF_AAOff();
}

/*
int CTextArea::RenderLine( SDL_Surface* a_poScreen, const char* a_poText, int a_iLength, int a_iColor )
{
}
*/


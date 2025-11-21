/***************************************************************************
                          FlyingChars.cpp  -  description
                             -------------------
    begin                : Mon Aug 12 2003
    copyright            : (C) 2003 by upi
    email                : upi@apocalypse.rulez.org
 ***************************************************************************/

#include "FlyingChars.h"
#include "sge_surface.h"
#include "common.h"


int g_iLineTime = 100;
int g_iCharTime = 80;


FlyingChars::FlyingChars( sge_bmpFont* a_poFont, const SDL_Rect& a_roRect, int a_iFontDisplacement )
{
	m_poFont = a_poFont;
	m_oRect = a_roRect;
	m_iFontDisplacement = a_iFontDisplacement;
	
	m_bDone = true;
	m_iTimeToNextLine = 0;
	m_iDelay = 0;
	m_iLastLineY = a_roRect.y;
	m_pcText = NULL;
	m_enAlignment = FC_AlignLeft;
	m_iTextOffset = 0;
	
	m_bScrolling = false;
	m_dScrollupRate = (double)(m_poFont->CharHeight+2) / (double)g_iLineTime;
	m_dScrollup = 0.0;
}


FlyingChars::~FlyingChars()
{
}
	

void FlyingChars::AddText( const char* a_pcText,
	TextAlignment a_enAlignment, bool a_bOneByOne )
{
	
	EnqueuedText oNewText;
	oNewText.m_pcText = a_pcText;
	oNewText.m_enAlignment = a_enAlignment;
	m_oEnqueuedTexts.push_back( oNewText );
	
	if ( a_bOneByOne && m_iLastLineY <= m_oRect.y )
	{
		m_iLastLineY = m_oRect.y + m_oRect.h - m_poFont->CharHeight;
	}
	else if ( 0 == m_pcText 
		|| m_iLastLineY <= m_oRect.y + m_oRect.h - m_poFont->CharHeight )
	{
		DequeueText();
	}
}


bool FlyingChars::IsDone()
{
	if ( m_oEnqueuedTexts.size() == 0
		&& ( NULL == m_pcText || 0 == m_pcText[ m_iTextOffset] )
		&& ( m_bDone ) )
	{
		return true;
	}
	return false;
}


void FlyingChars::DequeueText()
{
	if ( 0 == m_oEnqueuedTexts.size() )
	{
		return;
	}
	
	EnqueuedText& oEnqueuedText = m_oEnqueuedTexts.front();
	
	m_pcText = (unsigned char*) oEnqueuedText.m_pcText;
	m_enAlignment = oEnqueuedText.m_enAlignment;
	m_iTextOffset = 0;
	
	while ( m_iLastLineY <= m_oRect.y + m_oRect.h - m_poFont->CharHeight )
	{
		AddNextLine();
		m_iTimeToNextLine += g_iLineTime;
		
		if ( 0 == m_pcText[m_iTextOffset] )
		{
			break;
		}
	}
	
	m_oEnqueuedTexts.pop_front();
}


void FlyingChars::Advance( int a_iNumFrames )
{

	if ( a_iNumFrames > 5 )	a_iNumFrames = 5;
	if ( a_iNumFrames <= 0 ) a_iNumFrames = 0;
	
	m_bDone = true;
	
	m_iTimeToNextLine -= a_iNumFrames;
	if ( m_iTimeToNextLine < 0 )
	{
		m_iDelay = 0;
		
		if ( !m_pcText 
			|| 0 == m_pcText[m_iTextOffset]  )
		{
			DequeueText();
		}
		else
		{
			m_iTimeToNextLine += g_iLineTime;
			AddNextLine();
		}
	}
	
	m_dScrollup += a_iNumFrames * m_dScrollupRate;
	int iScrollup = (int) m_dScrollup;
	m_dScrollup -= iScrollup;
	iScrollup *= 2;

	for ( FlyingLetterIterator it=m_oLetters.begin(); it!=m_oLetters.end(); ++it )
	{
		FlyingLetter& roLetter = *it;
		if ( m_bScrolling )
		{
		    roLetter.m_iDY -= iScrollup;
			roLetter.m_iY -= iScrollup;
			
			if ( roLetter.m_iDY < m_oRect.y * 2
				&& roLetter.m_iDY >= 0 )
			{
				roLetter.m_iDY = -100;
				roLetter.m_iTime = 40;
			}
		}
		
		if (roLetter.m_iDelay > 0)
		{
			roLetter.m_iDelay -= a_iNumFrames;
			continue;
		}
		
		if ( roLetter.m_iTime > 0 )
		{
			m_bDone = false;
			
			int iEstX = roLetter.m_iSX * roLetter.m_iTime / 2 + roLetter.m_iX ;
			if ( iEstX > roLetter.m_iDX )
			{
				roLetter.m_iSX -= a_iNumFrames;
			}
			else if ( iEstX < roLetter.m_iDX )
			{
				roLetter.m_iSX += a_iNumFrames;
			}
			roLetter.m_iX += roLetter.m_iSX * a_iNumFrames;
			if ( roLetter.m_iSY * roLetter.m_iTime / 2 + roLetter.m_iY >= roLetter.m_iDY )
			{
				roLetter.m_iSY -= a_iNumFrames;
			}
			else
			{
				roLetter.m_iSY += a_iNumFrames;
			}
			roLetter.m_iY += roLetter.m_iSY * a_iNumFrames;
			
			roLetter.m_iTime -= a_iNumFrames;
			
			if ( roLetter.m_iTime <= 0 )
			{
				roLetter.m_iX = roLetter.m_iDX;
				roLetter.m_iY = roLetter.m_iDY;
				roLetter.m_iSX = roLetter.m_iSY = 0;
				roLetter.m_iTime = 0;
			}
		}		
	}
	
}


void FlyingChars::Draw()
{
	for ( FlyingLetterIterator it=m_oLetters.begin(); it!=m_oLetters.end(); ++it )
	{
		FlyingLetter& roLetter = *it;
		int iDestX, iDestY;
		
		if (roLetter.m_iDelay > 0)
		{
			continue;
		}
		else if ( roLetter.m_iTime > 0 )
		{
			iDestX = roLetter.m_iX;
			iDestY = roLetter.m_iY;
		}
		else
		{	
			iDestX = roLetter.m_iX;
			iDestY = roLetter.m_iY;
		}
		
		int iSrcX, iSrcW;
		
		if ( ! m_poFont->CharPos )
		{
			iSrcX = roLetter.m_cLetter * m_poFont->CharWidth;
			iSrcW = m_poFont->CharWidth;
		}
		else
		{
			int iOfs = ( ((unsigned int)roLetter.m_cLetter)-33)*2 + 1;
			iSrcX = m_poFont->CharPos[iOfs];
			iSrcW = m_poFont->CharPos[iOfs+1] - iSrcX;
		}
		
		//debug( "Letter %c at %d,%d\n", roLetter.m_cLetter, iDestX/2, iDestY/2 );
		
		sge_Blit( m_poFont->FontSurface, gamescreen, iSrcX, m_poFont->yoffs,
			iDestX/2, iDestY/2, iSrcW, m_poFont->CharHeight );		
	}
}


void FlyingChars::AddNextLine()
{
	if ( NULL == m_pcText )
	{
		return;
	}
	
	// 1. SCROLL UP EVERYTHING IF NECESSARY
	
	if ( m_iLastLineY > m_oRect.y + m_oRect.h - m_poFont->CharHeight )
	{
		// scroll up every character
		if ( !m_bScrolling )
		{
			m_bScrolling = true;
			m_iTimeToNextLine = int( (m_iLastLineY - (m_oRect.y + m_oRect.h - m_poFont->CharHeight)) / m_dScrollupRate );
			return;
		}
		m_iLastLineY = m_oRect.y + m_oRect.h - m_poFont->CharHeight;
	}
	
	SDL_Rect oRect;
	const unsigned char* pcLineStart = m_pcText + m_iTextOffset;
	if ( '\n' == *pcLineStart ) ++pcLineStart;
	while (*pcLineStart == 32 || *pcLineStart == '\t' ) ++pcLineStart;
	if ( 0 == *pcLineStart )
	{
		m_iTextOffset = pcLineStart - m_pcText;
		return;
	}
	
	// 2. CALCULATE LINE WIDTH AND CONTENTS
	
	const unsigned char* pcLineEnd = pcLineStart;
	const unsigned char* pcNextWord = pcLineEnd;
	int iNumWords = 0;
	int iLineWidth = 0;
	int iWidth = 0;
	
	while (1)
	{
		++iNumWords;
		if ( '\n' == *pcNextWord 
			|| 0 == *pcNextWord)
		{
			break;
		}
		
		// Skip the next 'white space' part
	  	while (*pcNextWord == 32 || *pcNextWord == '\t' )
		{
			iWidth += GetCharWidth( *pcNextWord );
			++pcNextWord;
		}
		// Skip the next 'non-whitespace' part
	  	while (*pcNextWord != 32 && *pcNextWord != '\t' 
			&& *pcNextWord != '\n' && *pcNextWord != 0 ) 
		{
			iWidth += GetCharWidth( *pcNextWord );
			++pcNextWord;
		}
		
		if ( iWidth > m_oRect.w )
		{
			// overflow
			break;
		}
		pcLineEnd = pcNextWord;
		iLineWidth = iWidth;
	}
	
	if ( pcLineEnd == pcLineStart ) 
	{	
		pcLineEnd = pcNextWord;
		iLineWidth = iWidth;
	}
	
	// 3. ADD LETTERS IN LINE
	
	double dX = m_oRect.x;
	double dSpaceLength = 0.0;
	
	switch ( m_enAlignment )
	{	
		case FC_AlignJustify:
			if ( '\n' == *pcLineEnd
				|| 0 == *pcLineEnd )
			{
			}
			else
			{
				dSpaceLength = (m_oRect.w - iLineWidth) / double( iNumWords > 2 ? iNumWords-2 : 1 );
			}
			
			break;
		
		case FC_AlignCenter:
			dX += (m_oRect.w - iLineWidth) /2;
			break;
			
		case FC_AlignRight:
			dX += (m_oRect.w - iLineWidth);
			break;
		
		default:
			break;
	}
	
	FlyingLetter oLetter;
	oLetter.m_iDY = m_iLastLineY * 2;
	
	for ( const unsigned char *pcChar = pcLineStart; pcChar<pcLineEnd; ++pcChar )
	{
		if ( *pcChar < 33 ) 
		{
			if ( *pcChar == 32 || *pcChar == '\t' )
			{
				dX += dSpaceLength;
				// debug( "dX = %3.2f dSpaceLength = %2.2f\n", (float)dX, (float)dSpaceLength );
			}
			
			int iWidth = GetCharWidth( *pcChar );
			dX += iWidth;
			//debug( "dX = %3.2f iWidth = %d\n", (float)dX, iWidth );
			continue;
		}
		
		oRect = sge_BF_TextSize( m_poFont, (char*) pcLineStart, pcChar-pcLineStart );
		
		oLetter.m_iDX = (int) dX * 2;
		oLetter.m_iX = rand() % (gamescreen->w * 2);
		oLetter.m_iY = gamescreen->h * 2;
		oLetter.m_iSX = 0;
		oLetter.m_iSY = -45 + rand() % 15 ;
		oLetter.m_iDelay = m_iDelay++;
		oLetter.m_iTime = g_iCharTime;
		oLetter.m_cLetter = *pcChar;
		
		m_oLetters.push_back(oLetter);
		dX += GetCharWidth( *pcChar );
	}
	
	m_iTextOffset = pcLineEnd - m_pcText;
	m_iLastLineY += m_poFont->CharHeight + 2;
}


int FlyingChars::GetCharWidth( unsigned char a_cChar )
{
	if ( a_cChar == 0 )
	{
		return 0;
	}
	else if ( m_poFont->CharPos )
	{
		if ( a_cChar < 33 )
		{
			return m_poFont->CharPos[3] - m_poFont->CharPos[2] + 1;
		}
		else
		{
			int iOfs = ( ((unsigned int)a_cChar) - 33) * 2 + 1;
			return m_poFont->CharPos[iOfs+1] - m_poFont->CharPos[iOfs] + m_iFontDisplacement;
		}
	}

	return m_poFont->CharWidth;
}

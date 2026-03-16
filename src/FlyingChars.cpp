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


// Decode one UTF-8 codepoint from *pp and advance *pp past the sequence.
// Returns the codepoint. On an invalid or truncated sequence, consumes one
// byte and returns its raw value so the rest of the string remains parseable.
static uint32_t utf8_next( const unsigned char** pp )
{
	const unsigned char* p = *pp;
	uint32_t cp;

	if ( (*p & 0x80) == 0 )
	{
		cp = *p++;
	}
	else if ( (*p & 0xE0) == 0xC0
		&& (p[1] & 0xC0) == 0x80 )
	{
		cp = (uint32_t)(*p & 0x1F) << 6 | (p[1] & 0x3F);
		p += 2;
	}
	else if ( (*p & 0xF0) == 0xE0
		&& (p[1] & 0xC0) == 0x80
		&& (p[2] & 0xC0) == 0x80 )
	{
		cp = (uint32_t)(*p & 0x0F) << 12 | (uint32_t)(p[1] & 0x3F) << 6 | (p[2] & 0x3F);
		p += 3;
	}
	else if ( (*p & 0xF8) == 0xF0
		&& (p[1] & 0xC0) == 0x80
		&& (p[2] & 0xC0) == 0x80
		&& (p[3] & 0xC0) == 0x80 )
	{
		cp = (uint32_t)(*p & 0x07) << 18 | (uint32_t)(p[1] & 0x3F) << 12
			| (uint32_t)(p[2] & 0x3F) << 6 | (p[3] & 0x3F);
		p += 4;
	}
	else
	{
		cp = *p++;   // invalid byte — consume it and move on
	}

	*pp = p;
	return cp;
}


#ifdef USE_TTF_FLYINGCHARS
#include <ft2build.h>
#include FT_FREETYPE_H

// Blit one BGRA color glyph bitmap from FreeType onto an SDL surface.
// The bitmap rows are top-to-bottom, each pixel is 4 bytes: B G R A.
static void blit_bgra_glyph( SDL_Surface* dst, FT_Bitmap* bmp, int x, int y )
{
	if ( SDL_MUSTLOCK(dst) ) SDL_LockSurface(dst);

	int bpp = dst->format->BytesPerPixel;

	for ( int by = 0; by < (int)bmp->rows; ++by )
	{
		int dy = y + by;
		if ( dy < 0 || dy >= dst->h ) continue;

		const Uint8* src_row = bmp->buffer + by * bmp->pitch;
		Uint8* dst_row = (Uint8*)dst->pixels + dy * dst->pitch;

		for ( int bx = 0; bx < (int)bmp->width; ++bx )
		{
			int dx = x + bx;
			if ( dx < 0 || dx >= dst->w ) continue;

			// FreeType BGRA: pixel_mode == FT_PIXEL_MODE_BGRA
			Uint8 sb = src_row[bx * 4 + 0];
			Uint8 sg = src_row[bx * 4 + 1];
			Uint8 sr = src_row[bx * 4 + 2];
			Uint8 sa = src_row[bx * 4 + 3];
			if ( sa == 0 ) continue;

			Uint8* dp = dst_row + dx * bpp;
			if ( bpp == 4 )
			{
				Uint32 existing = *(Uint32*)dp;
				Uint8 dr, dg, db;
				SDL_GetRGB( existing, dst->format, &dr, &dg, &db );
				dr = (Uint8)( (sr * sa + dr * (255 - sa)) / 255 );
				dg = (Uint8)( (sg * sa + dg * (255 - sa)) / 255 );
				db = (Uint8)( (sb * sa + db * (255 - sa)) / 255 );
				*(Uint32*)dp = SDL_MapRGB( dst->format, dr, dg, db );
			}
			else if ( bpp == 2 )
			{
				Uint16 existing = *(Uint16*)dp;
				Uint8 dr, dg, db;
				SDL_GetRGB( existing, dst->format, &dr, &dg, &db );
				dr = (Uint8)( (sr * sa + dr * (255 - sa)) / 255 );
				dg = (Uint8)( (sg * sa + dg * (255 - sa)) / 255 );
				db = (Uint8)( (sb * sa + db * (255 - sa)) / 255 );
				*(Uint16*)dp = (Uint16)SDL_MapRGB( dst->format, dr, dg, db );
			}
		}
	}

	if ( SDL_MUSTLOCK(dst) ) SDL_UnlockSurface(dst);
}


// Render one Unicode codepoint using the FreeType color pipeline.
// Returns true if the glyph was rendered as color (BGRA), false if caller
// should fall back to the normal SGE grayscale path.
static bool render_ft_color_glyph( SDL_Surface* screen, _sge_TTFont* font,
	uint32_t cp, int x, int y )
{
	FT_Face face = (FT_Face)sge_TTF_GetFTFace( font );
	FT_UInt glyph_index = FT_Get_Char_Index( face, cp );
	if ( glyph_index == 0 ) return false;

	FT_Error err = FT_Load_Glyph( face, glyph_index,
		FT_LOAD_COLOR | FT_LOAD_RENDER );
	if ( err ) return false;

	FT_GlyphSlot slot = face->glyph;
	if ( slot->bitmap.pixel_mode != FT_PIXEL_MODE_BGRA )
		return false;  // monochrome font – let SGE handle it

	int blit_x = x + slot->bitmap_left;
	int blit_y = y - slot->bitmap_top;
	blit_bgra_glyph( screen, &slot->bitmap, blit_x, blit_y );
	return true;
}


// Encode one Unicode codepoint into a NUL-terminated UTF-8 buffer (at least 5 bytes).
static void utf8_encode( uint32_t cp, char* buf )
{
	if ( cp < 0x80 )
	{
		buf[0] = (char)cp; buf[1] = 0;
	}
	else if ( cp < 0x800 )
	{
		buf[0] = (char)(0xC0 | (cp >> 6));
		buf[1] = (char)(0x80 | (cp & 0x3F));
		buf[2] = 0;
	}
	else if ( cp < 0x10000 )
	{
		buf[0] = (char)(0xE0 | (cp >> 12));
		buf[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
		buf[2] = (char)(0x80 | (cp & 0x3F));
		buf[3] = 0;
	}
	else
	{
		buf[0] = (char)(0xF0 | (cp >> 18));
		buf[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
		buf[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
		buf[3] = (char)(0x80 | (cp & 0x3F));
		buf[4] = 0;
	}
}
#endif // USE_TTF_FLYINGCHARS


// Translate a Unicode codepoint to the ISO-8859-2 (Latin-2) byte used as the
// glyph index in the bitmap fonts.  The fonts were created with Latin-2
// encoding, so all standard Latin-1 codepoints (< 256) map to themselves.
// Only the four Hungarian-specific characters that live above U+00FF need
// an explicit translation.
static uint32_t unicode_to_sfont( uint32_t cp )
{
	switch ( cp )
	{
		case 0x0150: return 0xD5;  // Ő  (Latin-2 0xD5)
		case 0x0151: return 0xF5;  // ő  (Latin-2 0xF5)
		case 0x0170: return 0xDB;  // Ű  (Latin-2 0xDB)
		case 0x0171: return 0xFB;  // ű  (Latin-2 0xFB)
		default:     return cp;
	}
}


int g_iLineTime = 100;
int g_iCharTime = 80;


FlyingChars::FlyingChars( sge_bmpFont* a_poFont, const SDL_Rect& a_roRect, int a_iFontDisplacement )
{
	m_poFont = a_poFont;
#ifdef USE_TTF_FLYINGCHARS
	m_poTTFont = NULL;
	m_iTTColor = 0xFFFFFFFF;
#endif
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
	m_dScrollupRate = (double)(GetFontHeight()+2) / (double)g_iLineTime;
	m_dScrollup = 0.0;
}


#ifdef USE_TTF_FLYINGCHARS
FlyingChars::FlyingChars( _sge_TTFont* a_poTTFont, const SDL_Rect& a_roRect, Uint32 a_iColor )
{
	m_poFont = NULL;
	m_poTTFont = a_poTTFont;
	m_iTTColor = a_iColor;
	m_oRect = a_roRect;
	m_iFontDisplacement = 0;

	m_bDone = true;
	m_iTimeToNextLine = 0;
	m_iDelay = 0;
	m_iLastLineY = a_roRect.y;
	m_pcText = NULL;
	m_enAlignment = FC_AlignLeft;
	m_iTextOffset = 0;

	m_bScrolling = false;
	m_dScrollupRate = (double)(GetFontHeight()+2) / (double)g_iLineTime;
	m_dScrollup = 0.0;
}
#endif // USE_TTF_FLYINGCHARS


int FlyingChars::GetFontHeight() const
{
#ifdef USE_TTF_FLYINGCHARS
	if ( m_poTTFont )
		return sge_TTF_FontHeight( m_poTTFont );
#endif
	return m_poFont->CharHeight;
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
		m_iLastLineY = m_oRect.y + m_oRect.h - GetFontHeight();
	}
	else if ( 0 == m_pcText
		|| m_iLastLineY <= m_oRect.y + m_oRect.h - GetFontHeight() )
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

	while ( m_iLastLineY <= m_oRect.y + m_oRect.h - GetFontHeight() )
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

#ifdef USE_TTF_FLYINGCHARS
		if ( m_poTTFont )
		{
			uint32_t cp = roLetter.m_cLetter;
			if ( cp < 33 ) continue;
			int rx = iDestX/2;
			int ry = iDestY/2 + sge_TTF_FontAscent( m_poTTFont );
			if ( !render_ft_color_glyph( gamescreen, m_poTTFont, cp, rx, ry ) )
			{
				// No COLR glyph – fall back to SGE grayscale renderer
				char buf[8] = {};
				utf8_encode( cp, buf );
				sge_TTF_AAOn();
				sge_tt_textout_UTF8( gamescreen, m_poTTFont, buf,
					rx, ry, m_iTTColor, C_BLACK, 255 );
				sge_TTF_AAOff();
			}
			continue;
		}
#endif // USE_TTF_FLYINGCHARS

		int iSrcX, iSrcW;

		if ( ! m_poFont->CharPos )
		{
			iSrcX = roLetter.m_cLetter * m_poFont->CharWidth;
			iSrcW = m_poFont->CharWidth;
		}
		else
		{
			uint32_t uLetter = unicode_to_sfont( roLetter.m_cLetter );
			if ( uLetter < 33 ) continue;
			int iCharIdx = (int)(uLetter - 33);
			if ( iCharIdx >= m_poFont->Chars ) continue;
			int iOfs = iCharIdx * 2 + 1;
			iSrcX = m_poFont->CharPos[iOfs];
			iSrcW = m_poFont->CharPos[iOfs+1] - iSrcX;
		}

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

	if ( m_iLastLineY > m_oRect.y + m_oRect.h - GetFontHeight() )
	{
		// scroll up every character
		if ( !m_bScrolling )
		{
			m_bScrolling = true;
			m_iTimeToNextLine = int( (m_iLastLineY - (m_oRect.y + m_oRect.h - GetFontHeight())) / m_dScrollupRate );
			return;
		}
		m_iLastLineY = m_oRect.y + m_oRect.h - GetFontHeight();
	}

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
		// Skip the next 'non-whitespace' part (UTF-8 aware)
	  	while (*pcNextWord != 32 && *pcNextWord != '\t'
			&& *pcNextWord != '\n' && *pcNextWord != 0 )
		{
			uint32_t cp = utf8_next( &pcNextWord );
			iWidth += GetCharWidth( cp );
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

	const unsigned char* pcChar = pcLineStart;
	while ( pcChar < pcLineEnd )
	{
		uint32_t cp = utf8_next( &pcChar );

		if ( cp < 33 )
		{
			if ( cp == 32 || cp == '\t' )
			{
				dX += dSpaceLength;
			}

			int iWidth = GetCharWidth( cp );
			dX += iWidth;
			continue;
		}

		oLetter.m_iDX = (int) dX * 2;
		oLetter.m_iX = rand() % (gamescreen->w * 2);
		oLetter.m_iY = gamescreen->h * 2;
		oLetter.m_iSX = 0;
		oLetter.m_iSY = -45 + rand() % 15;
		oLetter.m_iDelay = m_iDelay++;
		oLetter.m_iTime = g_iCharTime;
		oLetter.m_cLetter = cp;

		m_oLetters.push_back(oLetter);
		dX += GetCharWidth( cp );
	}

	m_iTextOffset = pcLineEnd - m_pcText;
	m_iLastLineY += GetFontHeight() + 2;
}


int FlyingChars::GetCharWidth( uint32_t a_cChar )
{
	if ( a_cChar == 0 )
	{
		return 0;
	}

#ifdef USE_TTF_FLYINGCHARS
	if ( m_poTTFont )
	{
		if ( a_cChar < 33 )
		{
			// approximate space width as ~1/3 of line height
			return GetFontHeight() / 3;
		}
		char buf[8] = {};
		utf8_encode( a_cChar, buf );
		SDL_Rect r = sge_TTF_TextSize( m_poTTFont, buf );
		return r.w;
	}
#endif // USE_TTF_FLYINGCHARS

	if ( m_poFont->CharPos )
	{
		if ( a_cChar < 33 )
		{
			return m_poFont->CharPos[3] - m_poFont->CharPos[2] + 1;
		}
		else
		{
			uint32_t uChar = unicode_to_sfont( a_cChar );
			int iCharIdx = (int)(uChar - 33);
			if ( iCharIdx >= m_poFont->Chars )
				return 0;  // codepoint not represented in this bitmap font
			int iOfs = iCharIdx * 2 + 1;
			return m_poFont->CharPos[iOfs+1] - m_poFont->CharPos[iOfs] + m_iFontDisplacement;
		}
	}

	return m_poFont->CharWidth;
}

/***************************************************************************
                          gfx.h  -  description
                             -------------------
    begin                : Tue Apr 10 2001
    copyright            : (C) 2001 by UPi
    email                : upi@apocalypse.rulez.org
 ***************************************************************************/


#ifndef _GFX_H
#define _GFX_H

/**
\defgroup Media Sound and graphics
Classes that deal with sound, graphics, input, etc.
*/


struct _sge_TTFont;

#include "SDL.h"

enum GFX_Constants {
	AlignHCenter	= 1,
	AlignVCenter	= 2,
	AlignCenter		= 3,
	UseTilde		= 4,
	UseShadow		= 8,
};

int				DrawTextMSZ( const char* text, _sge_TTFont* font, int x, int y,
					int flags, int fg, SDL_Surface* target, bool a_bTranslate = true );

void			DrawGradientText( const char* text, _sge_TTFont* font, int y,
					SDL_Surface* target, bool a_bTranslate = true );

SDL_Color		MakeColor( Uint8 r, Uint8 g, Uint8 b );

SDLKey			GetKey( bool a_bTranslate );

SDL_Surface*	LoadBackground( const char* a_pcFilename, int a_iNumColors, int a_iPaletteOffset=0, bool a_bTransparent = false );
SDL_Surface*	LoadImage( const char* a_pcFilename );

bool			SetVideoMode( bool a_bLarge, bool a_bFullscreen, int a_iAdditionalFlags=0 );

extern SDL_Surface* gamescreen;

/**
\ingroup Media
*/
class CSurfaceLocker
{
public:
	inline CSurfaceLocker()
	{
		if ( 0 == m_giLockCount )
		{
			if (SDL_MUSTLOCK(gamescreen)) { 
				SDL_LockSurface( gamescreen );
			}
		}
		++m_giLockCount;
	}
	inline ~CSurfaceLocker()
	{
		--m_giLockCount;
		if ( 0 == m_giLockCount )
		{
			if (SDL_MUSTLOCK(gamescreen)) {
				SDL_UnlockSurface( gamescreen );
			}
		}
	}

protected:
	static int m_giLockCount;
};

extern _sge_TTFont* titleFont;		// Largest font, for titles
extern _sge_TTFont* inkFont;		// Medium-size front, headings
extern _sge_TTFont* impactFont;		// Smallest font, for long descriptions
extern _sge_TTFont* chatFont;		// small but legible.
extern _sge_TTFont* bungeeSpiceFont;	// FighterStats: flying story text
extern _sge_TTFont* comboFont;		// FighterStats: stats labels/values

#ifdef sge_bm_text_H
extern sge_bmpFont* fastFont;		// In-game text, e.g. combo text
extern sge_bmpFont* creditsFont;
extern sge_bmpFont* storyFont;
#endif






#endif

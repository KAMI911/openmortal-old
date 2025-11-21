/***************************************************************************
                          gfx.cpp  -  description
                             -------------------
    begin                : Tue Apr 10 2001
    copyright            : (C) 2001 by UPi
    email                : upi@apocalypse.rulez.org
 ***************************************************************************/

#include <string.h>
#include <malloc.h>


#include "SDL.h"
#include "SDL_video.h"
#include "SDL_image.h"
#include "sge_tt_text.h"
#include "sge_surface.h"

#ifndef  __COMMON_H
#include "common.h"
#endif
#ifndef  _GFX_H
#include "gfx.h"
#endif
#include "State.h"
#include "Event.h"


int CSurfaceLocker::m_giLockCount = 0;



Uint16 *UTF8_to_UNICODE(Uint16 *unicode, const char *utf8, int len);


void sge_TTF_SizeText( _sge_TTFont*font, const char* text, int* x, int* y )
{
#ifdef MSZ_USES_UTF8
	Uint16 *unicode_text;
	int unicode_len;

	/* Copy the UTF-8 text to a UNICODE text buffer */
	unicode_len = strlen(text);
	unicode_text = (Uint16 *)malloc( (unicode_len+1) * sizeof (Uint16) );
	if ( unicode_text == NULL )
	{
		SDL_SetError("SGE - Out of memory");
		*x = *y = 0;
		return;
	}
	UTF8_to_UNICODE(unicode_text, text, unicode_len);
	
	/* Render the new text */
	SDL_Rect r = sge_TTF_TextSizeUNI(font, unicode_text);

	/* Free the text buffer and return */
	free(unicode_text);
#else
	SDL_Rect r = sge_TTF_TextSize( font, text );
#endif
	*x = r.w;
	*y = r.h;
}

int DrawTextMSZ( const char* string, _sge_TTFont* font, int x, int y, int flags,
	int fg, SDL_Surface* target, bool a_bTranslate )
{
    int retval = 0;

	if (!string || !*string) return retval;

	if ( a_bTranslate )
	{
		string = Translate( string );
	}

	if (flags & UseTilde)
	{
		char *str2 = strdup( string );
		char onechar[2];
		char *c1, *c2, *c3;
		int w = 0;
		int i, j;
		bool notend;

		if (flags & AlignHCenter)
		{
			// Determine width of the string without the stupid tildes
			c1 = c2 = str2;
			notend = true;


			while (notend)
			{
				c2 = c1;								// c1: start of this run
				while (*c2 && (*c2!='~')) c2++;			// c2: end of this run
				notend = *c2;
				*c2 = 0;

				sge_TTF_SizeText( font, c1, &i, &j);
				w += i;

				if (notend) { *c2='~'; c1=c2+1; }		// next run..
			}

			x -= w/2;
		}

		flags &= ~(UseTilde | AlignHCenter);

		c1 = str2;
		onechar[1]=0;
		notend = true;

		while (1)
		{
			c2 = c1;
			while (*c2 && (*c2!='~')) c2++;			// c2: end of this run
			notend = *c2;
			*c2 = 0;

			sge_TTF_SizeText( font, c1, &i, &j);
			DrawTextMSZ( c1, font, x, y, flags, fg, target, false );
			x += i;

			// At this point, we're either at a ~ or end of the text (notend)
			if (!notend) break;

			onechar[0]= *++c2;						// Could be 0, if ~ at end.
			if (!onechar[0]) break;
			sge_TTF_SizeText( font, onechar, &i, &j);
			DrawTextMSZ( onechar, font, x, y, flags, C_LIGHTCYAN, target, false );
			x += i;
			retval += i;
			c1 = c2+1;
			

			if (!*c1) break;						// ~X was end of string
		}
		
		delete( str2 );
		return retval;
	}
	
	SDL_Rect dest;
	int w, h;
	sge_TTF_SizeText( font, string, &w, &h );
	dest.w = retval = w;
	dest.h = h;
	dest.x = flags & AlignHCenter ? x-dest.w/2 : x;
	dest.y = flags & AlignVCenter ? y-dest.h/2 : y;

	//debug( "X: %d, Y: %d, W: %d, H: %d\n", dest.x, dest.y, dest.w, dest.h );

	CSurfaceLocker oLock;

	if ( flags & UseShadow )
	{
#ifdef MSZ_USES_UTF8
		sge_tt_textout_UTF8( target, font, string, dest.x+2, dest.y+2+sge_TTF_FontAscent(font), C_BLACK, C_BLACK, 255 );
#else
		sge_tt_textout( target, font, string, dest.x+2, dest.y+2+sge_TTF_FontAscent(font), C_BLACK, C_BLACK, 255 );
#endif
	}

	sge_TTF_AAOn();
#ifdef MSZ_USES_UTF8
	dest = sge_tt_textout_UTF8( target, font, string, dest.x, dest.y+sge_TTF_FontAscent(font), fg, C_BLACK, 255 );
#else
	dest = sge_tt_textout( target, font, string, dest.x, dest.y+sge_TTF_FontAscent(font), fg, C_BLACK, 255 );
#endif
	sge_TTF_AAOff();
	
	return dest.w;
}



void DrawGradientText( const char* text, _sge_TTFont* font, int y, SDL_Surface* target, bool a_bTranslate )
{
	int i, j;

	if ( a_bTranslate )
	{
		text = Translate( text );
	}

	// 1. CREATE OFFSCREEN SURFACE
	
	SDL_Rect size = sge_TTF_TextSize( font, (char*)text );
	size.w += 2;
	size.h += 2;
	size.x = 320 - size.w / 2;
	if ( size.x < 0 ) size.x = 0;
	size.y = y;
	SDL_Surface* surface = SDL_CreateRGBSurface( SDL_SRCCOLORKEY, size.w, size.h, 8, 0,0,0,0 );


	if ( NULL == surface )
	{
		debug( "DrawGradientText: Couldn't allocate %d by %d surface!\n", size.w, size.h );
		return;
	}

	// 2. SET OFFSCREEN SURFACE COLORS

	SDL_SetColorKey( surface, SDL_SRCCOLORKEY, 0 );
	SDL_Color colors[256];
	colors[0].r = colors[0].g = colors[0].b = 0;
	colors[1] = colors[0];

	// The rest is red->yellow gradient.

	for ( i=2; i<255; ++i )
	{
		int j = i > 25 ? i-25 : 0;
		colors[i].r = 255;
		colors[i].g = 255-j;
		colors[i].b = 0;
	}

	SDL_SetColors( surface, colors, 0, 256 );

	// 3. DRAW TEXT, APPLY BORDER, APPLY GRADIENT.

	int y1 = sge_TTF_FontAscent(font);
	sge_tt_textout( surface, font, text,
		1, y1, 255, 0, 255);


	if ( SDL_MUSTLOCK(surface) ) SDL_LockSurface(surface);
	for ( y=1; y<size.h-1; ++y )
	{
		int color = 254 * y / (size.h-1) + 1;
		unsigned char *p0, *p1, *p2;
		p1 = (unsigned char*) surface->pixels;
		p1 += surface->pitch * y + 1;
		p0 = p1 - surface->pitch;
		p2 = p1 + surface->pitch;
		
		for ( int x=1; x<size.w-1; ++x, ++p0, ++p1, ++p2 )
		{
			if ( *p1 > 2 )
			{
				*p1 = color;
			}
			else
			{
				if ( (*(p1-1) > 2) || (*(p1+1) > 2) || *p0 > 2 || *p2 > 2 )
				{
					*p1 = 1;
				}
			}
		}
	}
	if ( SDL_MUSTLOCK(surface) ) SDL_UnlockSurface(surface);

	// 4. FINALLY

	SDL_BlitSurface( surface, NULL, target, &size );
	SDL_FreeSurface( surface );
	SDL_UpdateRect( target, size.x, size.y, size.w, size.h );
}



SDL_Color MakeColor( Uint8 r, Uint8 g, Uint8 b )
{
	SDL_Color color;
	color.r = r; color.g = g; color.b = b; color.unused = 0;
	return color;
}


/**
Waits for a key event and returns it.

\param a_bTranslate		If this is true, then keypad events will also be
read and processed info keys (cursor, return and escape).
*/

SDLKey GetKey( bool a_bTranslate )
{
	SDL_Event oSdlEvent;
	SMortalEvent oEvent;
	
	while (SDL_WaitEvent(&oSdlEvent))
	{
		if ( SDL_KEYDOWN == oSdlEvent.type )
		{
			return oSdlEvent.key.keysym.sym;
		}
		if ( SDL_QUIT == oSdlEvent.type )

		{
			g_oState.m_bQuitFlag = true;
			return SDLK_ESCAPE;
		}

		if ( ! a_bTranslate )
		{
			continue;
		}

		// Handle gamepad and others
		TranslateEvent( &oSdlEvent, &oEvent );
		
		switch (oEvent.m_enType)
		{
			case Me_QUIT:
				g_oState.m_bQuitFlag = true;
				return SDLK_ESCAPE;
			
			case Me_PLAYERKEYDOWN:
				switch ( oEvent.m_iKey ) {
					case Mk_UP:		return SDLK_UP;
					case Mk_DOWN:	return SDLK_DOWN;
					case Mk_LEFT:	return SDLK_LEFT;
					case Mk_RIGHT:	return SDLK_RIGHT;
					default:		return SDLK_RETURN;
				}
				break;

			case Me_MENU:
				return SDLK_ESCAPE;

			default:
				break;
		}	// switch statement
	}	// Polling events

	// Code will never reach this point, unless there's an error.
	return SDLK_ESCAPE;
}

/**
\TODO Remove a_iNumcolors, a_iPaletteOffset
*/

SDL_Surface* LoadBackground( const char* a_pcFilename, int a_iNumColors, int a_iPaletteOffset, bool a_bTransparent )
{
	char acFilepath[FILENAME_MAX+1];
	strcpy( acFilepath, DATADIR );
	strcat( acFilepath, "/gfx/" );
	strcat( acFilepath, a_pcFilename );

	SDL_Surface* poBackground = IMG_Load( acFilepath );
	if (!poBackground)
	{
		debug( "Can't load file: %s\n", acFilepath );
		return NULL;
	}
	
	SDL_Palette* pal = poBackground->format->palette;
	if ( pal && gamescreen->format->palette )
	{
		int ncolors = pal->ncolors;
		if (ncolors>a_iNumColors) ncolors = a_iNumColors;
		if (ncolors+a_iPaletteOffset > 255) ncolors = 255 - a_iPaletteOffset;
		SDL_SetColors( gamescreen, pal->colors, a_iPaletteOffset, ncolors );
	}
		
	SDL_Surface* poRetval = SDL_DisplayFormat( poBackground );
	SDL_FreeSurface( poBackground );
	
	// 2. TRY TO LOAD AN IMAGE MASK
	// This means trying to load a .png file which acts as a mask for the
	// original [jpg] image.
	// If the original file is <Basename>.jpg, the mask is <Basename>.mask.png
	
	int iLength = strlen( acFilepath );
	char acMaskFilename[FILENAME_MAX+1];
	strncpy( acMaskFilename, acFilepath, iLength-4 );
	acMaskFilename[iLength-4] = 0;
	strcat( acMaskFilename, ".mask.png" );
	
	SDL_Surface* poMask = IMG_Load( acMaskFilename );
	if ( !poMask )
	{
		// No mask.
		return poRetval;
	}
	
	if ( poMask->w < poRetval->w
		|| poMask->h < poRetval->h )
	{
		debug( "Error loading mask for %s: mask is too small.\n", acFilepath );
		SDL_FreeSurface( poMask );
		return poRetval;
	}
	
	debug( "Loading mask for %s.\n", acFilepath );
	
	Uint32 iTransparent = SDL_MapRGB( gamescreen->format, 255, 217, 0 ); // an unlikely color in openmortal..
	Uint32 iMask = sge_GetPixel( poMask, 0, 0 );
	Uint32 iPixel;
	
	for ( int y = 0; y < poRetval->h; ++y ) {
		for ( int x=0; x< poRetval->w; ++x ) {
			iPixel = sge_GetPixel( poMask, x, y );
//			debug( "%d ", iPixel );
			if ( iPixel == iMask ) { 
				sge_PutPixel( poRetval, x, y, iTransparent );
			}
		}
//		debug( "\n" );
	}
	
	SDL_FreeSurface( poMask );
	
	SDL_SetColorKey( poRetval, SDL_SRCCOLORKEY, iTransparent );
	
	return poRetval;
}


SDL_Surface *LoadImage( const char* a_pcFilename )
{
	return LoadBackground(a_pcFilename, 0, 0);
}


bool SetVideoMode( bool a_bLarge, bool a_bFullScreen, int a_iAdditionalFlags )
{
	// SET THE PARAMETERS FOR THE VIDEO MODE

	int iBpp = 16;		// Try the display's BPP first.
	if ( NULL != gamescreen )
	{
		iBpp = gamescreen->format->BitsPerPixel;
	}

	int iFlags = a_iAdditionalFlags;
	if ( a_bFullScreen )
	{
		iFlags |= SDL_FULLSCREEN;
	}

	// CALL SDL_SetVideoMode

	int iWidth = a_bLarge ? 800 : 640;
	int iHeight = a_bLarge ? 600 : 480;
//	if ( !a_bFullScreen ) iHeight = 480;

	gamescreen = SDL_SetVideoMode( iWidth, iHeight, iBpp, iFlags );
	if ( NULL == gamescreen ) 
	{
		debug( "SDL_SetVideoMode( %d, %d, %d, %d ) failed.\n", iWidth, iHeight, iBpp, iFlags );
		return false;
	}

	// IF THE DISPLAY IS 24BPP OR 8 BPP OR LESS, EMULATE 16 BPP INSTEAD
	// (because we are lazy and won't write 8bpp and 24bpp code anymore)

	if ( gamescreen->format->BytesPerPixel != 2
		&& gamescreen->format->BytesPerPixel != 4 )
	{
		gamescreen = SDL_SetVideoMode( iWidth, iHeight, 16, iFlags );
		if ( NULL == gamescreen )
		{
			debug( "SDL_SetVideoMode( %d, %d, %d, %d ) failed.\n", iWidth, iHeight, 16, iFlags );
			return false;
		}
	}
	
	return true;
}


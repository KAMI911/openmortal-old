/***************************************************************************
                          RlePack.cpp  -  description
                             -------------------
    begin                : Mon Sep 24 2001
    copyright            : (C) 2001 by upi
    email                : upi@apocalypse.rulez.org
 ***************************************************************************/

#include "RlePack.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "SDL.h"
#include "gfx.h"
#include "common.h"


/// Sanity: This is the maximal number of entries in a .DAT file.
#define MAXDATACOUNT	65530


inline void ChangeEndian32( Uint32& a_riArg )
{
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
	char* pcArg = (char*)&a_riArg;
	char cTemp;
	cTemp = pcArg[0]; pcArg[0] = pcArg[3]; pcArg[3] = cTemp;
	cTemp = pcArg[1]; pcArg[1] = pcArg[2]; pcArg[2] = cTemp;
#endif
}


inline void ChangeEndian16( Uint16& a_riArg )
{
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
	char* pcArg = (char*)&a_riArg;
	char cTemp;
	cTemp = pcArg[0]; pcArg[0] = pcArg[1]; pcArg[1] = cTemp;
#endif
}


inline Uint32 ConvertEndian32( Uint32 a_iArg )
{
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
	char* pcArg = (char*)&a_iArg;
	char cTemp;
	cTemp = pcArg[0]; pcArg[0] = pcArg[3]; pcArg[3] = cTemp;
	cTemp = pcArg[1]; pcArg[1] = pcArg[2]; pcArg[2] = cTemp;
#endif
	return a_iArg;
}


inline Uint16 ConvertEndian16( Uint16 a_iArg )
{
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
	char* pcArg = (char*)&a_iArg;
	char cTemp;
	cTemp = pcArg[0]; pcArg[0] = pcArg[1]; pcArg[1] = cTemp;
#endif
	return a_iArg;
}



/**
\ingroup Media
\brief CRleSprite is a runlength encoded sprite that can be blitted flipped.

CRleSprite originally comes from Allegro. I use it in OpenMortal because
it is relatively fast to blit and is economical on the memory too.
*/
typedef struct RLE_SPRITE           /* a RLE compressed sprite */
{
	Uint16 dummy;					// For better alignment... NASTY NASTY HACK!!
	Uint16 color_depth;                 /* color depth of the image */
	Uint16 w, h;                        /* width and height in pixels */
	Uint32 size;
	signed char dat[0];
} RLE_SPRITE;


/**
\ingroup Media
\brief Internal data of CRlePack
*/
struct RlePack_P
{
	SDL_Color		m_aoPalette[256];
	SDL_Color		m_aoTintedPalette[256];
	TintEnum		m_enTint;
	int				m_iCount;
	int				m_iArraysize;
	RLE_SPRITE**	m_pSprites;
	void*			m_pData;
	
	int				m_iColorCount;
	int				m_iColorOffset;
	Uint32			m_aiRGBPalette[256];

	void			draw_rle_sprite8( RLE_SPRITE* a_poSprite, int a_dx, int a_dy );
	void			draw_rle_sprite_v_flip8( RLE_SPRITE* a_poSprite, int a_dx, int a_dy );
	void			draw_rle_sprite16( RLE_SPRITE* a_poSprite, int a_dx, int a_dy );
	void			draw_rle_sprite_v_flip16( RLE_SPRITE* a_poSprite, int a_dx, int a_dy );
	void			draw_rle_sprite24( RLE_SPRITE* a_poSprite, int a_dx, int a_dy );
	void			draw_rle_sprite_v_flip24( RLE_SPRITE* a_poSprite, int a_dx, int a_dy );
	void			draw_rle_sprite32( RLE_SPRITE* a_poSprite, int a_dx, int a_dy );
	void			draw_rle_sprite_v_flip32( RLE_SPRITE* a_poSprite, int a_dx, int a_dy );
};



RlePack::RlePack( const char* a_pcFilename, int a_iNumColors )
{
	p = new RlePack_P;
	p->m_enTint = NO_TINT;
	p->m_iCount = 0;
	p->m_iArraysize = 0;
	p->m_pSprites = NULL;
	p->m_pData = NULL;
	
	p->m_iColorCount = 0;
	p->m_iColorOffset = 0;
	
	// Load file and stuff
	
	LoadFile( a_pcFilename, a_iNumColors );
}


RlePack::~RlePack()
{
	if (!p)
		return;
	
	if (p->m_pSprites)
	{
		delete[] p->m_pSprites;
		p->m_pSprites = NULL;
	}
	
	free( p->m_pData );
	delete( p );
	p = NULL;
}


void RlePack::Clear()
{
	if ( p && p->m_pSprites )
	{
		delete[] p->m_pSprites;
		p->m_pSprites = NULL;
	}
}


int RlePack::LoadFile( const char* a_pcFilename, int a_iNumColors )
{
	FILE* f;
	
	f = fopen( a_pcFilename, "rb" );
	if (f==NULL)
	{
		debug( "Can't open file '%s'.\n", a_pcFilename );
		return -1;
	}
	
	fseek( f, 0, SEEK_END );
	long iFileSize = ftell ( f );
	p->m_pData = malloc( iFileSize );
	if ( NULL == p->m_pData )
	{
		fclose( f );
		return -1;
	}
	
	fseek( f, 0, SEEK_SET );
	int iRead = fread( p->m_pData, 1, iFileSize, f );
	fclose( f );
	
	p->m_iColorCount = a_iNumColors;
	
	if ( iFileSize != iRead )
	{
		debug( "Warning RlePack(): iFileSize=%d, iRead=%d\n", iFileSize, iRead );
	}
	
	struct SHeader
	{
		char	acDummy[8];
		Uint32	iDatacount;
	} *poHeader = (SHeader*) p->m_pData;
	
	ChangeEndian32( poHeader->iDatacount );
	debug( "File '%s' contains %d entries.\n", a_pcFilename, poHeader->iDatacount );
	
	if (poHeader->iDatacount>MAXDATACOUNT) poHeader->iDatacount = MAXDATACOUNT;		// Sanity
	
	p->m_iArraysize = poHeader->iDatacount;
	p->m_pSprites = new RLE_SPRITE*[ poHeader->iDatacount ];
	
	char* pcNext = ((char*)p->m_pData) + sizeof(SHeader);
	char* pcEnd = ((char*)p->m_pData) + iFileSize;
	
	while ( pcNext < pcEnd - 4 )
	{
		if ( 0 == strncmp( pcNext, "prop", 4 ) )
		{
			struct SProperty
			{
				char	acName[4];
				Uint32	iSize;
			} *poProperty = (SProperty*) (pcNext+4);
			ChangeEndian32( poProperty->iSize );
			
			pcNext += 4 + sizeof(SProperty) + poProperty->iSize;
		}
		else if ( 0 == strncmp( pcNext, "RLE ", 4 ) )
		{
			struct SRLE
			{
				RLE_SPRITE	oSprite;
			} *poRle = (SRLE*) (pcNext+10);
			poRle->oSprite.color_depth = ConvertEndian16(poRle->oSprite.color_depth);
			poRle->oSprite.w = ConvertEndian16(poRle->oSprite.w);
			poRle->oSprite.h = ConvertEndian16(poRle->oSprite.h);
			poRle->oSprite.size = ConvertEndian32(poRle->oSprite.size);
			
			p->m_pSprites[p->m_iCount] = &(poRle->oSprite);
			p->m_iCount++;
			pcNext += 10 + sizeof( SRLE ) + poRle->oSprite.size;
		}
		else if ( 0 == strncmp( pcNext, "PAL ", 4 ) )
		{
			struct SPAL
			{
				Uint32		iLength1;
				Uint32		iLength;
				SDL_Color	aoColors[256];
			} *poPal = (SPAL*) (pcNext+4);
			ChangeEndian32( poPal->iLength );
			
			int iNumColors = poPal->iLength>1024 ? 1024 : poPal->iLength;
			iNumColors /= 4;
			
			for (int i=0; i< iNumColors; i++)
			{
				p->m_aoPalette[i].r = poPal->aoColors[i].r*4;
				p->m_aoPalette[i].g = poPal->aoColors[i].g*4;
				p->m_aoPalette[i].b = poPal->aoColors[i].b*4;
				p->m_aoPalette[i].unused = 0;
				p->m_aoTintedPalette[i] = p->m_aoPalette[i];
			}
			
			pcNext += 4 + 8 + poPal->iLength;
		}
		else
		{
			struct SUnknown
			{
				Uint32	iSize;
			} *poUnknown = (SUnknown*) (pcNext+4);
			ChangeEndian32( poUnknown->iSize );
			
			debug( "Unknown: '%4s', size: %d\n", pcNext, poUnknown->iSize );
			
			pcNext += 4 + sizeof(SUnknown) + poUnknown->iSize;
		}
	}
	
	return p->m_iCount;
	
#if 0
// #
// # Here's the old implementation or read, left here for reference only.
// #
	
	int datacount;
	
	#define READDW(I) {														\
		unsigned char data[4];														\
		fread( data, 4, 1, f );												\
		(I) = (data[0]<<24) + (data[1]<<16) + (data[2]<<8) + data[3]; }
	#define READW(I) {														\
		unsigned char data[2];														\
		fread( data, 2, 1, f );												\
		(I) = (data[0]<<8) + data[1]; }
	#define READCH(S,C) {													\
		fread( S, C, 1, f ); S[C] = 0; }
	
	fseek( f, 8, SEEK_SET );		// Skip header
	READDW( datacount );
	
	debug( "File '%s' contains %d entries.\n", filename, datacount );
	if (datacount>500) datacount = 500;			// Sanity
	
	p->arraysize = datacount;
	p->sprites = new RLE_SPRITE*[ datacount ];
	
	while( (!feof(f)) && (!ferror(f)) && (datacount>0) )
	{
		char s[10];
		READCH( s, 4 );
		if ( !strcmp( s, "prop" ))				// Found a property
		{
			fseek( f, 4, SEEK_CUR );
			unsigned int propsize;
			READDW( propsize );
			fseek( f, propsize, SEEK_CUR );
		}
		else if (!strcmp( s, "RLE " ))			// Found an RLE_SPRITE
		{
			datacount--;
			
			unsigned int length, bpp, width, height, size;
			
			READDW( length );
			READDW( length );
			READW( bpp );
			READW( width );
			READW( height );
			READDW( size );
			
			RLE_SPRITE* sprite = (RLE_SPRITE*) malloc( sizeof(RLE_SPRITE) + size );
			p->sprites[ p->count ] = sprite;
			(p->count)++;
			sprite->w = width;
			sprite->h = height;
			sprite->color_depth = bpp;
			sprite->size = size;
			fread( sprite->dat, 1, size, f );
		}
		else if (!strcmp( s, "PAL "))			// Found a palette
		{
			datacount--;
			
			unsigned int length, pallength;
			READDW( length );
			READDW( length );
			pallength = length>1024 ? 1024 : length;
			pallength /= 4;
			
			for (unsigned int i=0; i< pallength; i++)
			{
				char c[4];
				fread( c, 4, 1, f );
				p->palette[i].r = c[0]*4;
				p->palette[i].g = c[1]*4;
				p->palette[i].b = c[2]*4;
				p->palette[i].unused = 0;
			}
			
			fseek( f, length - pallength*4, SEEK_CUR );
		}
		else									// Found something else
		{
			debug( "Unknown: %s.", s );
			datacount--;
		
			unsigned int length;
			READDW( length );
			READDW( length );
			fseek( f, length, SEEK_CUR );
		}
	}
	
	fclose( f );
#endif
}




int RlePack::Count()
{
	return p->m_iCount;
}


/** Worker method of RlePack::OffsetSprites() .*/

void OffsetRLESprite( RLE_SPRITE* spr, int offset ) // Static method
{
	if (!spr || !offset) return;
	
	signed char *s = spr->dat;
	signed char c;
	int y;
	
	for (y=0; y<spr->h; y++)
	{
		c = *s++;
		
		while (c)
		{
			// For positive c: solid pixels.
			for ( ; c>0; c-- )
			{
				*s = (*s) + offset;
				s++;
			}
			c = *s++;
		}
	}
}


/** Offsets the sprites "logical" palette values by the given offset.
This is only relevant in 8BPP mode; in other color depths this is a
no-op.

Explanation: RlePacks have an internal palette which contains up to 256
colors. These colors are always indexed from 0 up. However, if you load
two RlePacks with different palettes, the palettes will collide, and one
RlePack will be displayed with an incorrect palette.

To work around this, you can offset one of the sprites palette. For example,
you might load an RlePack with 16 colors, and another with 64 colors. You
can offset the second RlePack by 16 colors; the total effect is that the
two RlePacks now use 80 colors of the available 256 colors, the first using
colors 0-15, the second using colors 16-79.
*/

void RlePack::OffsetSprites( int a_iOffset )
{
	if ( (a_iOffset<=0) || (a_iOffset>255) || (8!=gamescreen->format->BitsPerPixel) )
		return;

	p->m_iColorOffset = a_iOffset;

	int i;
	
	// Offset every RLE_SPRITE
	
	for ( i=0; i<p->m_iCount; ++i )
	{
		OffsetRLESprite( p->m_pSprites[i], a_iOffset );
	}
}


void RlePack::SetTint( TintEnum a_enTint )
{
	int i;

	switch( a_enTint )
	{
		case ZOMBIE_TINT:
		{
			for ( i=0; i<p->m_iColorCount; ++i )
			{
				p->m_aoTintedPalette[i].r = 0;
				p->m_aoTintedPalette[i].g = p->m_aoPalette[i].g;
				p->m_aoTintedPalette[i].b = 0;
			}
			break;
		}

		case GRAY_TINT:
		{
			int j;
			for ( i=0; i<p->m_iColorCount; ++i )
			{
				j = (p->m_aoPalette[i].r + p->m_aoPalette[i].g + p->m_aoPalette[i].b)/4;
				p->m_aoTintedPalette[i].r = j;
				p->m_aoTintedPalette[i].g = j;
				p->m_aoTintedPalette[i].b = j;
			}
			break;
		}

		case DARK_TINT:
		{
			for ( i=0; i<p->m_iColorCount; ++i )
			{
				p->m_aoTintedPalette[i].r = int(p->m_aoPalette[i].r) * 2 / 3;
				p->m_aoTintedPalette[i].g = int(p->m_aoPalette[i].g) * 2 / 3;
				p->m_aoTintedPalette[i].b = int(p->m_aoPalette[i].b) * 2 / 3;
			}
			break;
		}

		case INVERTED_TINT:
		{
			for ( i=0; i<p->m_iColorCount; ++i )
			{
				p->m_aoTintedPalette[i].r = 255 - p->m_aoPalette[i].r;
				p->m_aoTintedPalette[i].g = 255 - p->m_aoPalette[i].g;
				p->m_aoTintedPalette[i].b = 255 - p->m_aoPalette[i].b;
			}
			break;
		}

		case NO_TINT:
		default:
		{
			for ( i=0; i<p->m_iColorCount; ++i )
			{
				p->m_aoTintedPalette[i] = p->m_aoPalette[i];
			}
			break;
		}
		
	} // end of switch( a_enTint )

}


/** Loads the palette of the RlePack to the gamescreen.
This only works in 8BPP mode; in other modes the palette is always considered
to be loaded, and this is a no-operation.
*/

void RlePack::ApplyPalette()
{
	if ( 8 == gamescreen->format->BitsPerPixel )
	{
		SDL_SetColors( gamescreen, p->m_aoTintedPalette, p->m_iColorOffset, p->m_iColorCount );
	}
	else
	{
		// Now is the time to compile m_aiRGBPalette
		for ( int i=0; i<p->m_iColorCount; ++i )
		{
			SDL_Color& roColor = p->m_aoTintedPalette[i];
			p->m_aiRGBPalette[i] = SDL_MapRGB( gamescreen->format, roColor.r, roColor.g, roColor.b );
		}
	}
}


/** Returns the width of a given sprite in pixels.

\param a_iIndex The index of the sprite, 0 <= a_iIndex < Count()
*/

int RlePack::GetWidth( int a_iIndex )
{
	if ( (a_iIndex<0) || (a_iIndex>=p->m_iCount) )
		return -1;

	RLE_SPRITE* poSprite = p->m_pSprites[a_iIndex];
	if (!poSprite)
		return -1;

	return poSprite->w;
}


/** Returns the height of a given sprite in pixels.

\param a_iIndex The index of the sprite, 0 <= a_iIndex < Count()
*/

int RlePack::GetHeight( int a_iIndex )
{
	if ( (a_iIndex<0) || (a_iIndex>=p->m_iCount) )
		return -1;

	RLE_SPRITE* poSprite = p->m_pSprites[a_iIndex];
	if (!poSprite)
		return -1;

	return poSprite->h;
}



#define METHODNAME				RlePack_P::draw_rle_sprite8
#define METHODNAME_FLIP			RlePack_P::draw_rle_sprite_v_flip8
#define PIXEL_PTR				unsigned char*
#define PUT_PIXEL(p,c)			(*((unsigned char *)(p)) = (c))
#define PITCH					(dst->pitch)
#include "DrawRle.h"
#undef METHODNAME
#undef METHODNAME_FLIP
#undef PIXEL_PTR
#undef PUT_PIXEL
#undef PITCH

#define METHODNAME				RlePack_P::draw_rle_sprite16
#define METHODNAME_FLIP			RlePack_P::draw_rle_sprite_v_flip16
#define PIXEL_PTR				Uint16*
#define PUT_PIXEL(p,c)			(*((PIXEL_PTR )(p)) = (m_aiRGBPalette[c]))
#define PITCH					(dst->pitch / 2)
#include "DrawRle.h"

#undef METHODNAME
#undef METHODNAME_FLIP
#undef PIXEL_PTR
#undef PUT_PIXEL
#undef PITCH

#define METHODNAME				RlePack_P::draw_rle_sprite32
#define METHODNAME_FLIP			RlePack_P::draw_rle_sprite_v_flip32
#define PIXEL_PTR				Uint32*
#define PUT_PIXEL(p,c)			(*((PIXEL_PTR )(p)) = (m_aiRGBPalette[c]))
#define PITCH					(dst->pitch / 4)
#include "DrawRle.h"




void RlePack::Draw( int a_iIndex, int a_iX, int a_iY, bool a_bFlipped )
{
	if ( (a_iIndex<0) || (a_iIndex>=p->m_iCount) )
		return;
	
	RLE_SPRITE* poSprite = p->m_pSprites[a_iIndex];
	if (!poSprite)
		return;
	
	CSurfaceLocker oLock;

	if ( a_bFlipped )
	{
		switch (gamescreen->format->BitsPerPixel)
		{
		case 8: 
			p->draw_rle_sprite_v_flip8( poSprite, a_iX, a_iY ); break;
		case 15: 
		case 16:
			p->draw_rle_sprite_v_flip16( poSprite, a_iX, a_iY ); break;
		case 32:
			p->draw_rle_sprite_v_flip32( poSprite, a_iX, a_iY ); break;
		}
	}
	else
	{
		switch (gamescreen->format->BitsPerPixel)
		{
		case 8:
			p->draw_rle_sprite8( poSprite, a_iX, a_iY ); break;
		case 15: 
		case 16:
			p->draw_rle_sprite16( poSprite, a_iX, a_iY ); break;
		case 32:
			p->draw_rle_sprite32( poSprite, a_iX, a_iY ); break;
		}
	}
}


SDL_Surface* RlePack::CreateSurface( int a_iIndex, bool a_bFlipped )
{
	if ( (a_iIndex<0) || (a_iIndex>=p->m_iCount) )
		return NULL;
	
	RLE_SPRITE* poSprite = p->m_pSprites[a_iIndex];
	if (!poSprite)
		return NULL;

	SDL_Surface* poSurface = SDL_CreateRGBSurface( SDL_SWSURFACE, poSprite->w, poSprite->h, gamescreen->format->BitsPerPixel,
		gamescreen->format->Rmask, gamescreen->format->Gmask, gamescreen->format->Bmask, gamescreen->format->Amask );

	if ( NULL == poSurface )
	{
		return NULL;
	}
	
	if ( gamescreen->format->BitsPerPixel <= 8 )
	{
		SDL_SetColors( poSurface, gamescreen->format->palette->colors, 0, gamescreen->format->palette->ncolors );
	}

	SDL_FillRect( poSurface, NULL, 0 );// C_LIGHTGREEN );
	SDL_SetColorKey( poSurface, SDL_SRCCOLORKEY, 0 ); //C_LIGHTGREEN );
	
	SDL_Surface* poTemp = gamescreen;
	gamescreen = poSurface;
	Draw( a_iIndex, 0, 0, a_bFlipped );
	gamescreen = poTemp;

	return poSurface;
}

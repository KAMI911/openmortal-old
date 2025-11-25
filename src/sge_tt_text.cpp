/*
*	SDL Graphics Extension
*	Text/TrueType font functions
*
*	Started 990815
*
*	License: LGPL v2+ (see the file LICENSE)
*	(c)1999-2001 Anders Lindström
*
*	Uses the excellent FreeType 2 library, available at:
*	http://www.freetype.org/
*/

/*********************************************************************
 *  This library is free software; you can redistribute it and/or    *
 *  modify it under the terms of the GNU Library General Public      *
 *  License as published by the Free Software Foundation; either     *
 *  version 2 of the License, or (at your option) any later version. *
 *********************************************************************/

/*
*  Most of this code is taken from the SDL ttf lib by Sam Lantinga
*  <slouken@devolution.com>
*/	


#include "SDL.h"
#include <stdlib.h>
//[segabor]
#ifndef MACOSX
#include <malloc.h>
#endif
#include <memory.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include "sge_surface.h"
#include "sge_primitives.h"
#include "sge_tt_text.h"

#ifndef _SGE_NOTTF
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_TRUETYPE_IDS_H

/* The structure used to hold glyph information (cached) */
struct glyph {
	int stored;
	FT_UInt index;
	//FT_Bitmap bitmap;
	FT_Bitmap pixmap;
	int minx;
	int maxx;
	int miny;
	int maxy;
	int yoffset;
	int advance;
	Uint16 cached;
};

/* the truetype font structure */
struct _sge_TTFont{
	FT_Face face;

	/* Font metrics */
	int height;
	int ascent;
	int descent;
	int lineskip;

	/* The font style */
	Uint8 style;

	/* Extra width in glyph bounds for text styles */
	int glyph_overhang;
	float glyph_italics;

	/* Information in the font for underlining */
	int underline_offset;
	int underline_height;

	/* For now, support Latin-1 character set caching */
	glyph *current;
	glyph cache[256];
	glyph scratch;
};


/* Macro to convert a character to a Unicode value -- assume already Unicode */
//Should really make a proper convert algorithm someday
#define UNICODE(c)	c

/* FIXME: Right now we assume the gray-scale renderer Freetype is using
   supports 256 shades of gray, but we should instead key off of num_grays
   in the result FT_Bitmap after the FT_Render_Glyph() call. */
#define NUM_GRAYS       256

/* Handy routines for converting from fixed point */
#define FT_FLOOR(X)	((X & -64) / 64)
#define FT_CEIL(X)	(((X + 63) & -64) / 64)

#define CACHED_METRICS	0x10
#define CACHED_BITMAP	0x01
#define CACHED_PIXMAP	0x02

/* The FreeType font engine/library */
static FT_Library _sge_library;
static int _sge_TTF_initialized = 0;

Uint8 _sge_TTF_AA=1;     //Rendering mode: 0-OFF, 1-AA, 2-Alpha


/**********************************************************************************/
/**                          Open/misc font functions                            **/
/**********************************************************************************/

//==================================================================================
// Turns TTF AntiAliasing On/Off or alpha (nice but slow) (Default: On)
//==================================================================================
void sge_TTF_AAOff(void)
{
	_sge_TTF_AA=0;
}
void sge_TTF_AAOn(void)
{
	_sge_TTF_AA=1;
}
void sge_TTF_AA_Alpha(void)
{
	_sge_TTF_AA=2;
}


//==================================================================================
// Closes the ttf engine, done by exit
//==================================================================================
void sge_TTF_Quit(void)
{
	if ( _sge_TTF_initialized ) {
		FT_Done_FreeType( _sge_library );
	}
	_sge_TTF_initialized = 0;
}


//==================================================================================
// Starts the ttf engine, must be called first
//==================================================================================
int sge_TTF_Init(void)
{
	FT_Error error;

	error = FT_Init_FreeType( &_sge_library );
	if ( error ) {
		SDL_SetError("SGE - Couldn't init FreeType engine");
		return(-1);
	} else {
		_sge_TTF_initialized = 1;
	}
	atexit(sge_TTF_Quit); //dont't trust the user...
	return(0);
}


//==================================================================================
// Some helper functions
//==================================================================================
void Flush_Glyph(glyph *glyph)
{
	glyph->stored = 0;
	glyph->index = 0;
	//if( glyph->bitmap.buffer ) {
	//	free( glyph->bitmap.buffer );
	//	glyph->bitmap.buffer = 0;
	//}
	if( glyph->pixmap.buffer ) {
		free( glyph->pixmap.buffer );
		glyph->pixmap.buffer = 0;
	}
	glyph->cached = 0;
}
		
void Flush_Cache(sge_TTFont *font)
{
	int i;
	int size = sizeof( font->cache ) / sizeof( font->cache[0] );

	for( i = 0; i < size; ++i ) {
		if( font->cache[i].cached ) {
			Flush_Glyph( &font->cache[i] );
		}

	}
	if( font->scratch.cached ) {
		Flush_Glyph( &font->scratch );
	}
}


//==================================================================================
// Remove font from memory
//==================================================================================
void sge_TTF_CloseFont(sge_TTFont *font)
{
	Flush_Cache( font );
	FT_Done_Face( font->face );
	free( font );
}


//==================================================================================
// Open the TT font file and returns the font with pt size
//==================================================================================
sge_TTFont *sge_TTF_OpenFont(const char *file, int ptsize)
{
	sge_TTFont *font;
	FT_Error error;
	FT_Face face;
	FT_Fixed scale;

	font = (sge_TTFont *)malloc(sizeof(*font));
	if ( font == NULL ) {
		SDL_SetError("SGE - Out of memory");
		return(NULL);
	}
	memset(font, 0, sizeof(*font));

	/* Open the font and create ancillary data */
	error = FT_New_Face( _sge_library, file, 0, &font->face );
	if ( error ) {
		sge_SetError("SGE - Couldn't load font file: %s",file);
		free(font);
		return(NULL);
	}
	face = font->face;
	
	/* Make sure that our font face is scalable (global metrics) */
	if ( ! FT_IS_SCALABLE(face) ) {
		sge_SetError("SGE - Font face is not scalable: %s",file);
		sge_TTF_CloseFont( font );
		return NULL;
	}
		
	/* Set the character size and use 96 DPI */
	error = FT_Set_Char_Size( font->face, 0, ptsize * 64, 96, 96 );
	//error = FT_Set_Pixel_Sizes( font->face, 0, ptsize );   
	if( error ) {
		sge_SetError("SGE - Couldn't set font size: %s",file);
		sge_TTF_CloseFont( font );
		return NULL;
	}
	
	/* Get the scalable font metrics for this font */
	scale = face->size->metrics.y_scale;
	font->ascent  = FT_CEIL(FT_MulFix(face->bbox.yMax, scale));
	font->descent = FT_CEIL(FT_MulFix(face->bbox.yMin, scale));
	font->height  = font->ascent - font->descent + /* baseline */ 1;
	font->lineskip = FT_CEIL(FT_MulFix(face->height, scale));
	font->underline_offset = FT_FLOOR(FT_MulFix(face->underline_position, scale));
	font->underline_height = FT_FLOOR(FT_MulFix(face->underline_thickness, scale));
	if ( font->underline_height < 1 ) {
		font->underline_height = 1;
	}
	
	/* Set the default font style */
	font->style = SGE_TTF_NORMAL;
	font->glyph_overhang = face->size->metrics.y_ppem / 10;
	/* x offset = cos(((90.0-12)/360)*2*M_PI), or 12 degree angle */
	font->glyph_italics = 0.207f;
	font->glyph_italics *= font->height;
	
	return font;
}


//==================================================================================
// Load a glyph
//==================================================================================
FT_Error Load_Glyph(sge_TTFont *font, Uint16 ch, glyph *cached, int want )
{
	FT_Face face;
	FT_Error error;
	FT_GlyphSlot glyph;
	FT_Glyph_Metrics* metrics;
	FT_Outline* outline;
	
	//assert( font );
	//assert( font->face );

	face = font->face;

	/* Load the glyph */
	if ( ! cached->index ) {
		cached->index = FT_Get_Char_Index( face, ch );
	}
	error = FT_Load_Glyph( face, cached->index, FT_LOAD_DEFAULT );
	if( error ) {
		return error;
	}
	
	/* Get our glyph shortcuts */
	glyph = face->glyph;
	metrics = &glyph->metrics;
	outline = &glyph->outline;
	
	/* Get the glyph metrics if desired */
	if ( (want & CACHED_METRICS) && !(cached->stored & CACHED_METRICS) ) {
		/* Get the bounding box */
		cached->minx = FT_FLOOR(metrics->horiBearingX);
		cached->maxx = cached->minx + FT_CEIL(metrics->width);
		cached->maxy = FT_FLOOR(metrics->horiBearingY);
		cached->miny = cached->maxy - FT_CEIL(metrics->height);
		cached->yoffset = font->ascent - cached->maxy;
		cached->advance = FT_CEIL(metrics->horiAdvance);

		/* Adjust for bold and italic text */
		if ( font->style & SGE_TTF_BOLD ) {
			cached->maxx += font->glyph_overhang;
		}
		if ( font->style & SGE_TTF_ITALIC ) {
			cached->maxx += (int)ceil(font->glyph_italics);
		}
		cached->stored |= CACHED_METRICS;
	}

	if ( ((want & CACHED_BITMAP) && !(cached->stored & CACHED_BITMAP)) ||
	     ((want & CACHED_PIXMAP) && !(cached->stored & CACHED_PIXMAP)) ) {
		//int mono = (want & CACHED_BITMAP);
		int i;
		FT_Bitmap* src;
		FT_Bitmap* dst;

		/* Handle the italic style */
		if( font->style & SGE_TTF_ITALIC ) {
			FT_Matrix shear;

			shear.xx = 1 << 16;
			shear.xy = (int) ( font->glyph_italics * ( 1 << 16 ) ) / font->height;
			shear.yx = 0;
			shear.yy = 1 << 16;

			FT_Outline_Transform( outline, &shear );
		}

		/* Render the glyph */
		//if ( mono ) {
		//	error = FT_Render_Glyph( glyph, ft_render_mode_mono );
		//} else {
			error = FT_Render_Glyph( glyph, ft_render_mode_normal );
		//}
		if( error ) {
			return error;
		}

		/* Copy over information to cache */
		src = &glyph->bitmap;
		//if ( mono ) {
		//	dst = &cached->bitmap;
		//} else {
			dst = &cached->pixmap;
		//}
		memcpy( dst, src, sizeof( *dst ) );
		//if ( mono ) {
		//	dst->pitch *= 8;
		//}

		/* Adjust for bold and italic text */
		if( font->style & SGE_TTF_BOLD ) {
			int bump = font->glyph_overhang;
			dst->pitch += bump;
			dst->width += bump;
		}
		if( font->style & SGE_TTF_ITALIC ) {
			int bump = (int)ceil(font->glyph_italics);
			dst->pitch += bump;
			dst->width += bump;
		}

		dst->buffer = (unsigned char *)malloc( dst->pitch * dst->rows );
		if( !dst->buffer ) {
			return FT_Err_Out_Of_Memory;
		}
		memset( dst->buffer, 0, dst->pitch * dst->rows );

		for( i = 0; i < src->rows; i++ ) {
			int soffset = i * src->pitch;
			int doffset = i * dst->pitch;
			/*if ( mono ) {
				unsigned char *srcp = src->buffer + soffset;
				unsigned char *dstp = dst->buffer + doffset;
				int j;
				for ( j = 0; j < src->width; j += 8 ) {
					unsigned char ch = *srcp++;
					*dstp++ = (ch&0x80) >> 7;
					ch <<= 1;
					*dstp++ = (ch&0x80) >> 7;
					ch <<= 1;
					*dstp++ = (ch&0x80) >> 7;
					ch <<= 1;
					*dstp++ = (ch&0x80) >> 7;
					ch <<= 1;
					*dstp++ = (ch&0x80) >> 7;
					ch <<= 1;
					*dstp++ = (ch&0x80) >> 7;
					ch <<= 1;
					*dstp++ = (ch&0x80) >> 7;
					ch <<= 1;
					*dstp++ = (ch&0x80) >> 7;
				}
			} else {*/
				memcpy(dst->buffer+doffset,
				       src->buffer+soffset, src->pitch);
			//}
		}

		/* Handle the bold style */
		if ( font->style & SGE_TTF_BOLD ) {
			int row;
			int col;
			int offset;
			int pixel;
			Uint8* pixmap;

			/* The pixmap is a little hard, we have to add and clamp */
			for( row = dst->rows - 1; row >= 0; --row ) {
				pixmap = (Uint8*) dst->buffer + row * dst->pitch;
				for( offset=1; offset <= font->glyph_overhang; ++offset ) {
					for( col = dst->width - 1; col > 0; --col ) {
						pixel = (pixmap[col] + pixmap[col-1]);
						if( pixel > NUM_GRAYS - 1 ) {
							pixel = NUM_GRAYS - 1;
						}
						pixmap[col] = (Uint8) pixel;
					}
				}
			}
		}

		/* Mark that we rendered this format */
		//if ( mono ) {
		//	cached->stored |= CACHED_BITMAP;
		//} else {
			cached->stored |= CACHED_PIXMAP;
		//}
	}

	/* We're done, mark this glyph cached */
	cached->cached = ch;

	return 0;
}


//==================================================================================
// Find glyph
//==================================================================================
FT_Error Find_Glyph(sge_TTFont *font, Uint16 ch, int want)
{
	int retval = 0;

	if( ch < 256 ) {
		font->current = &font->cache[ch];
	} else {
		if ( font->scratch.cached != ch ) {
			Flush_Glyph( &font->scratch );
		}
		font->current = &font->scratch;
	}
	if ( (font->current->stored & want) != want ) {
		retval = Load_Glyph( font, ch, font->current, want );
	}
	return retval;
}


//==================================================================================
// Change the size of font
//==================================================================================
int sge_TTF_SetFontSize(sge_TTFont *font, int ptsize)
{
	FT_Error error;
	FT_Fixed scale;
	FT_Face face;

	/* Set the character size and use 96 DPI */
	error = FT_Set_Char_Size( font->face, 0, ptsize * 64, 96, 96 );
	//error = FT_Set_Pixel_Sizes( font->face, 0, ptsize );
	if( error ) {
		sge_SetError("SGE - Couldn't set font size");
		sge_TTF_CloseFont( font );
		return -1;
	}
	
	Flush_Cache(font);
	face = font->face;
	
	/* Get the scalable font metrics for this font */
	scale = face->size->metrics.y_scale;
	font->ascent  = FT_CEIL(FT_MulFix(face->bbox.yMax, scale));
	font->descent = FT_CEIL(FT_MulFix(face->bbox.yMin, scale));
	font->height  = font->ascent - font->descent + /* baseline */ 1;
	font->lineskip = FT_CEIL(FT_MulFix(face->height, scale));
	font->underline_offset = FT_FLOOR(FT_MulFix(face->underline_position, scale));
	font->underline_height = FT_FLOOR(FT_MulFix(face->underline_thickness, scale));
	if ( font->underline_height < 1 ) {
		font->underline_height = 1;
	}
	
	/* Set the default font style */
	//font->style = SGE_TTF_NORMAL;
	font->glyph_overhang = face->size->metrics.y_ppem / 10;
	/* x offset = cos(((90.0-12)/360)*2*M_PI), or 12 degree angle */
	font->glyph_italics = 0.207f;
	font->glyph_italics *= font->height;

	return 0;
}


//==================================================================================
// Get font geometrics
//==================================================================================
int sge_TTF_FontHeight(sge_TTFont *font)
{
	return(font->height);
}
int sge_TTF_FontAscent(sge_TTFont *font)
{
	return(font->ascent);
}
int sge_TTF_FontDescent(sge_TTFont *font)
{
	return(font->descent);
}
int sge_TTF_FontLineSkip(sge_TTFont *font)
{
	return(font->lineskip);
}
int sge_TTF_GlyphMetrics(sge_TTFont *font, Uint16 ch, int* minx, int* maxx, int* miny, int* maxy, int* advance)
{
	FT_Error error;

	error = Find_Glyph(font, ch, CACHED_METRICS);

	if ( error ) {
		return -1;
	}

	if ( minx ) {
		*minx = font->current->minx;
	}
	if ( maxx ) {
		*maxx = font->current->maxx;
	}
	if ( miny ) {
		*miny = font->current->miny;
	}
	if ( maxy ) {
		*maxy = font->current->maxy;
	}
	if ( advance ) {
		*advance = font->current->advance;
	}
	return 0;
}


//==================================================================================
// Set font style
//==================================================================================
void sge_TTF_SetFontStyle(sge_TTFont *font, Uint8 style)
{
	font->style = style;
	Flush_Cache(font);
}


//==================================================================================
// Get font style
//==================================================================================
Uint8 sge_TTF_GetFontStyle(sge_TTFont *font)
{
	return(font->style);
}
#endif /* _SGE_NOTTF */


//==================================================================================
// Convert the Latin-1 text to UNICODE
//==================================================================================
Uint16 *ASCII_to_UNICODE(Uint16 *unicode, const char *text, int len)
{
	int i;

	for ( i=0; i < len; ++i ) {
		unicode[i] = ((const unsigned char *)text)[i];
	}
	unicode[i] = 0;

	return unicode;
}

Uint16 *sge_Latin1_Uni(const char *text)
{
	Uint16 *unicode_text;
	int i, unicode_len;

	/* Copy the Latin-1 text to a UNICODE text buffer */
	unicode_len = strlen(text);
	unicode_text = (Uint16 *)malloc((unicode_len+1)*(sizeof *unicode_text));
	if ( unicode_text == NULL ) {
		SDL_SetError("SGE - Out of memory");
		return(NULL);
	}
	for ( i=0; i < unicode_len; ++i ) {
		unicode_text[i] = ((const unsigned char *)text)[i];
	}
	unicode_text[i] = 0;

	return(unicode_text);
}

//==================================================================================
// Convert the UTF-8 text to UNICODE
//==================================================================================
Uint16 *UTF8_to_UNICODE(Uint16 *unicode, const char *utf8, int len)
{
	int i, j;
	Uint16 ch;

	for ( i=0, j=0; i < len; ++i, ++j ) {
		ch = ((const unsigned char *)utf8)[i];
		if ( ch >= 0xF0 ) {
			ch  =  (Uint16)(utf8[i]&0x07) << 18;
			ch |=  (Uint16)(utf8[++i]&0x3F) << 12;
			ch |=  (Uint16)(utf8[++i]&0x3F) << 6;
			ch |=  (Uint16)(utf8[++i]&0x3F);
		} else
		if ( ch >= 0xE0 ) {
			ch  =  (Uint16)(utf8[i]&0x3F) << 12;
			ch |=  (Uint16)(utf8[++i]&0x3F) << 6;
			ch |=  (Uint16)(utf8[++i]&0x3F);
		} else
		if ( ch >= 0xC0 ) {
			ch  =  (Uint16)(utf8[i]&0x3F) << 6;
			ch |=  (Uint16)(utf8[++i]&0x3F);
		}
		unicode[j] = ch;
	}
	unicode[j] = 0;

	return unicode;
}

Uint16 *sge_UTF8_Uni(const char *text)
{
	Uint16 *unicode_text;
	int unicode_len;

	/* Copy the UTF-8 text to a UNICODE text buffer */
	unicode_len = strlen(text);
	unicode_text = (Uint16 *)malloc((unicode_len+1)*(sizeof *unicode_text));
	if ( unicode_text == NULL ) {
		SDL_SetError("SGE - Out of memory");
		return(NULL);
	}
	
	return UTF8_to_UNICODE(unicode_text, text, unicode_len);
}

#ifndef _SGE_NOTTF
//==================================================================================
// Get the width of the text with the given font
//==================================================================================
SDL_Rect sge_TTF_TextSizeUNI(sge_TTFont *font, const Uint16 *text)
{
	SDL_Rect ret; ret.x=0; ret.y=0, ret.w=0, ret.h=0;
	const Uint16 *ch;
	int x, z;
	int minx, maxx;
	int miny, maxy;
	glyph *glyph;
	FT_Error error;

	/* Initialize everything to 0 */
	if ( ! _sge_TTF_initialized ) {
		return ret;
	}

	minx = miny = 0;
	maxx = maxy = 0;

	/* Load each character and sum it's bounding box */
	x= 0;
	for ( ch=text; *ch; ++ch ) {
		error = Find_Glyph(font, *ch, CACHED_METRICS);
		if ( error ) {
			return ret;
		}
		glyph = font->current;

		z = x + glyph->minx;
		if ( minx > z ) {
			minx = z;
		}
		if ( font->style & SGE_TTF_BOLD ) {
			x += font->glyph_overhang;
		}
		if ( glyph->advance > glyph->maxx ) {
			z = x + glyph->advance;
		} else {
			z = x + glyph->maxx;
		}
		if ( maxx < z ) {
			maxx = z;
		}
		x += glyph->advance;

		if ( glyph->miny < miny ) {
			miny = glyph->miny;
		}
		if ( glyph->maxy > maxy ) {
			maxy = glyph->maxy;
		}
	}

	/* Fill the bounds rectangle */
	ret.w = (maxx - minx);
	//ret.h = (maxy - miny); /* This is correct, but breaks many applications */
	ret.h = font->height; 

	return ret;
}

SDL_Rect sge_TTF_TextSize(sge_TTFont *font, const char *text, int a_iMaxLength)
{
	SDL_Rect ret; ret.x=ret.y=ret.w=ret.y=0;
	Uint16 *unicode_text;
	int unicode_len;

	/* Copy the Latin-1 text to a UNICODE text buffer */
	unicode_len = strlen(text);
	if ( unicode_len > a_iMaxLength
		&& a_iMaxLength>=0 )
	{
		unicode_len = a_iMaxLength;
	}
	unicode_text = (Uint16 *)malloc((unicode_len+1)*(sizeof *unicode_text));
	if ( unicode_text == NULL ) {
		SDL_SetError("SGE - Out of memory");
		return ret;
	}
	ASCII_to_UNICODE( unicode_text, text, unicode_len );
	
	/* Render the new text */
	ret = sge_TTF_TextSizeUNI(font, unicode_text);

	/* Free the text buffer and return */
	free(unicode_text);
	
	return ret;
}



/**********************************************************************************/
/**                           TTF output functions                               **/
/**********************************************************************************/

//==================================================================================
// TT Render (unicode)
// Returns an 8bit or 32bit(8/8/8/8-alpha) surface with TT text
//==================================================================================
SDL_Surface *sge_TTF_RenderUNICODE(sge_TTFont *font,const Uint16 *text, SDL_Color fg, SDL_Color bg)
{
	int xstart, width;
	int w, h;
	SDL_Surface *textbuf;
	SDL_Palette *palette;
	int index;
	int rdiff, gdiff, bdiff;
	const Uint16 *ch;
	Uint8 *src, *dst;
	Uint32 *dst32;
	Uint32 alpha=0;
	Uint32 pixel=0;
	Uint32 Rmask=0, Gmask=0, Bmask=0, Amask=0;
	int row, col;
	FT_Error error;

	/* Get the dimensions of the text surface */
	SDL_Rect ret=sge_TTF_TextSizeUNI(font, text);
	w=ret.w; h=ret.h;
	if ( !w ) {
		SDL_SetError("SGE - Text has zero width");
		return(NULL);
	}

	/* Create the target surface */
	width = w;	
	if(_sge_TTF_AA!=2) /* Allocate an 8-bit pixmap */
		textbuf = SDL_AllocSurface(SDL_SWSURFACE, w, h, 8, 0, 0, 0, 0);
	else{ /* Allocate an 32-bit alpha pixmap */
		if ( SDL_BYTEORDER == SDL_LIL_ENDIAN ) {
			Rmask = 0x000000FF;
			Gmask = 0x0000FF00;
			Bmask = 0x00FF0000;
			Amask = 0xFF000000;
		} else {
			Rmask = 0xFF000000;
			Gmask = 0x00FF0000;
			Bmask = 0x0000FF00;
			Amask = 0x000000FF;
		}
		textbuf = SDL_AllocSurface(SDL_SWSURFACE, w, h, 32, Rmask, Gmask, Bmask, Amask);	
	}
		
	if ( textbuf == NULL ) {
		SDL_SetError("SGE - Out of memory");
		return(NULL);
	}


	/* Setup our colors */
	switch(_sge_TTF_AA){
	
		case 0:{  /* No fancy antialiasing or alpha component */
			palette = textbuf->format->palette;
	
			palette->colors[0].r = bg.r;
			palette->colors[0].g = bg.g;
			palette->colors[0].b = bg.b;
			palette->colors[1].r = fg.r;
			palette->colors[1].g = fg.g;
			palette->colors[1].b = fg.b;	
		}
		break;
	
		case 1:{  /* Fill the palette with NUM_GRAYS levels of shading from bg to fg */
			palette = textbuf->format->palette;	
	
			rdiff = fg.r - bg.r;
			gdiff = fg.g - bg.g;
			bdiff = fg.b - bg.b;
			for ( index=0; index< NUM_GRAYS; ++index ) {
				palette->colors[index].r = bg.r + (index*rdiff)/(NUM_GRAYS-1);
				palette->colors[index].g = bg.g + (index*gdiff)/(NUM_GRAYS-1);
				palette->colors[index].b = bg.b + (index*bdiff)/(NUM_GRAYS-1);
			}
		}
		break;
	
		case 2:{  /* Alpha component magic */
			sge_ClearSurface(textbuf, SDL_MapRGBA(textbuf->format, bg.r,bg.g,bg.b,SDL_ALPHA_TRANSPARENT));
			//pixel = (fg.r<<16)|(fg.g<<8)|fg.b;
			pixel = (fg.b<<16)|(fg.g<<8)|fg.r;
		}
		break;
	}


	/* Load and render each character */
	xstart = 0;
	for ( ch=text; *ch; ++ch ) {
		error = Find_Glyph(font, *ch, CACHED_METRICS|CACHED_PIXMAP);
		
		if ( ! error ) {
			w = font->current->pixmap.width;
			src = (Uint8 *)font->current->pixmap.buffer;
			for ( row = 0; row < font->current->pixmap.rows; ++row ) {
				dst = (Uint8 *)textbuf->pixels + (row + font->current->yoffset)* textbuf->pitch + xstart + font->current->minx;
				
				switch(_sge_TTF_AA){
				
					case 0:{  /* Normal */	
						src = font->current->pixmap.buffer + row * font->current->pixmap.pitch;
						for ( col=w; col>0; --col ) {
							*dst++ |= (*src++<NUM_GRAYS/2)? 0:1;
						}	
					}
					break;
					case 1:{  /* Antialiasing */
						src = font->current->pixmap.buffer + row * font->current->pixmap.pitch;
						for ( col=w; col>0; --col ) {
							*dst++ |= *src++;
						}	
					}
					break;
					
					case 2:{  /* Alpha */
						dst32 = (Uint32 *)textbuf->pixels + (row + font->current->yoffset)* textbuf->pitch/4 + xstart + font->current->minx;	
						for ( col=w; col>0; --col ) {
							alpha = *src++;
							*dst32++ |= pixel | (alpha << 24);
						}
					}
					break;
				}
			}

			xstart += font->current->advance;
			if ( font->style & SGE_TTF_BOLD ) {
				xstart += font->glyph_overhang;
			}
		}
	}
	
	/* Handle the underline style */
	if ( font->style & SGE_TTF_UNDERLINE ) {
		int row_offset;

		row_offset = font->ascent - font->underline_offset - 1;
		if ( row_offset > textbuf->h ) {
			row_offset = (textbuf->h-1) - font->underline_height;
		}
		
		if(_sge_TTF_AA==0){
			dst = (Uint8 *)textbuf->pixels + row_offset * textbuf->pitch;
			for ( row=font->underline_height; row>0; --row ) {
				memset(dst, 1, textbuf->w );
				dst += textbuf->pitch;
			}
		}else if(_sge_TTF_AA==1){
			dst = (Uint8 *)textbuf->pixels + row_offset * textbuf->pitch;
			for ( row=font->underline_height; row>0; --row ) {
				memset(dst, NUM_GRAYS - 1, textbuf->w );
				dst += textbuf->pitch;
			}
		}else{
			pixel |= Amask;
			dst32 = (Uint32 *)textbuf->pixels+row_offset*textbuf->pitch/4;
			for ( row=font->underline_height; row>0; --row ) {
				for ( col=0; col < textbuf->w; ++col ) {
					dst32[col] = pixel;
				}
				dst32 += textbuf->pitch/4;
			}
		}	
	}
	return(textbuf);
}


//==================================================================================
// Renders the Unicode string to TrueType on surface, with the color fcolor.
// bcolor is the target color for the antialiasing.
// Alpha sets the transparency of the text (255-solid, 0-max).
//==================================================================================
SDL_Rect sge_tt_textout_UNI(SDL_Surface *Surface, sge_TTFont *font, const Uint16 *uni, Sint16 x, Sint16 y, Uint32 fcolor, Uint32 bcolor, int Alpha)
{	
	SDL_Rect ret; ret.x=0; ret.y=0; ret.w=0; ret.h=0;	

	SDL_Color temp;
	SDL_Surface *text;

	text=sge_TTF_RenderUNICODE(font,uni,sge_GetRGB(Surface,fcolor),sge_GetRGB(Surface,bcolor));
	if(text==NULL){return ret;}

 	/* Align the surface text to the baseline */
	Uint16 ascent=font->ascent;

	temp=sge_GetRGB(Surface,bcolor);
	sge_BlitTransparent(text,Surface,0,0,x,y-ascent,text->w,text->h,SDL_MapRGB(text->format,temp.r,temp.g,temp.b),Alpha);
	sge_UpdateRect(Surface,x,y-ascent,text->w,text->h);

	ret.x=x; ret.y=y-ascent; ret.w=text->w; ret.h=text->h;

	SDL_FreeSurface(text);
	return ret;	
}


//==================================================================================
// Renders the Unicode string to TrueType on surface, with the color fcolor.
// bcolor is the target color for the antialiasing.
// Alpha sets the transparency of the text (0-solid, 255-max). (RGB)
//==================================================================================
SDL_Rect sge_tt_textout_UNI(SDL_Surface *Surface, sge_TTFont *font, const Uint16 *uni, Sint16 x, Sint16 y, Uint8 fR, Uint8 fG, Uint8 fB, Uint8 bR, Uint8 bG, Uint8 bB, int Alpha)
{
	SDL_Rect ret; ret.x=0; ret.y=0; ret.w=0; ret.h=0;
	SDL_Surface *text;

	text=sge_TTF_RenderUNICODE(font,uni,sge_FillPaletteEntry(fR,fG,fB),sge_FillPaletteEntry(bR,bG,bB));
	if(text==NULL){return ret;}

	/* Align the surface text to the baseline */
	Uint16 ascent=font->ascent;

	sge_BlitTransparent(text,Surface,0,0,x,y-ascent,text->w,text->h,SDL_MapRGB(text->format,bR,bG,bB),Alpha);

	sge_UpdateRect(Surface,x,y-ascent,text->w,text->h);

	ret.x=x; ret.y=y-ascent; ret.w=text->w; ret.h=text->h;

	SDL_FreeSurface(text);
	return ret;	
}


//==================================================================================
// Renders the Latin-1 string to TrueType on surface, with the color fcolor.
// bcolor is the target color for the antialiasing.
// Alpha sets the transparency of the text (0-solid, 255-max).
//==================================================================================
SDL_Rect sge_tt_textout(SDL_Surface *Surface, sge_TTFont *font, const char *string, Sint16 x, Sint16 y, Uint32 fcolor, Uint32 bcolor, int Alpha)
{
	SDL_Rect ret;
	Uint16 *uni;

	uni=sge_Latin1_Uni(string);
	
	ret=sge_tt_textout_UNI(Surface,font,uni,x,y,fcolor,bcolor,Alpha);
	free(uni);
	
	return ret;	
}

//==================================================================================
// Renders the Latin-1 string to TrueType on surface, with the color fcolor.
// bcolor is the target color for the antialiasing.
// Alpha sets the transparency of the text (0-solid, 255-max). (RGB)
//==================================================================================
SDL_Rect sge_tt_textout(SDL_Surface *Surface, sge_TTFont *font, const char *string, Sint16 x, Sint16 y, Uint8 fR, Uint8 fG, Uint8 fB, Uint8 bR, Uint8 bG, Uint8 bB, int Alpha)
{
	SDL_Rect ret;
	Uint16 *uni;

	uni=sge_Latin1_Uni(string);
	
	ret=sge_tt_textout_UNI(Surface,font,uni,x,y, fR,fG,fB, bR,bG,bB, Alpha);
	free(uni);
	
	return ret;	
}


//==================================================================================
// Renders the UTF-8 string to TrueType on surface, with the color fcolor.
// bcolor is the target color for the antialiasing.
// Alpha sets the transparency of the text (0-solid, 255-max).
//==================================================================================
SDL_Rect sge_tt_textout_UTF8(SDL_Surface *Surface, sge_TTFont *font, const char *string, Sint16 x, Sint16 y, Uint32 fcolor, Uint32 bcolor, int Alpha)
{
	SDL_Rect ret;
	Uint16 *uni;

	uni=sge_UTF8_Uni(string);

	ret=sge_tt_textout_UNI(Surface,font,uni,x,y,fcolor,bcolor,Alpha);
	free(uni);
	
	return ret;	
}

//==================================================================================
// Renders the UTF-8 string to TrueType on surface, with the color fcolor.
// bcolor is the target color for the antialiasing.
// Alpha sets the transparency of the text (0-solid, 255-max). (RGB)
//==================================================================================
SDL_Rect sge_tt_textout_UTF8(SDL_Surface *Surface, sge_TTFont *font, const char *string, Sint16 x, Sint16 y, Uint8 fR, Uint8 fG, Uint8 fB, Uint8 bR, Uint8 bG, Uint8 bB, int Alpha)
{
	SDL_Rect ret;
	Uint16 *uni;

	uni=sge_UTF8_Uni(string);

	ret=sge_tt_textout_UNI(Surface,font,uni,x,y, fR,fG,fB, bR,bG,bB, Alpha);
	free(uni);
	
	return ret;	
}


//==================================================================================
// Renders the formatet Latin-1 string to TrueType on surface, with the color fcolor.
// bcolor is the target color for the antialiasing.
// Alpha sets the transparency of the text (0-solid, 255-max). (RGB ONLY)
// * just like printf(char *format,...) *
//==================================================================================
SDL_Rect sge_tt_textoutf(SDL_Surface *Surface, sge_TTFont *font, Sint16 x, Sint16 y, Uint8 fR, Uint8 fG, Uint8 fB, Uint8 bR, Uint8 bG, Uint8 bB, int Alpha ,const char *format,...)
{
	char buf[256];

	va_list ap;
	
	#ifdef __WIN32__
	va_list ap;
    va_start(ap, format);
	#else
	va_start(ap, format);
	#endif
	
	vsprintf(buf, format, ap);
	va_end(ap);

	return sge_tt_textout(Surface, font, buf, x,y, fR,fG,fB, bR,bG,bB, Alpha);
}




/**********************************************************************************/
/**                          TTF 'input' functions                               **/
/**********************************************************************************/

// First some internel functions for TTF input

//==================================================================================
// Fast update function for TTF input
//
// type=0 - in ret smaller then out ret
// type=1 - in ret bigger then out ret
// type=3 - safe
//==================================================================================
SDL_Rect fast_update(SDL_Surface *Surface,SDL_Surface *buffer,SDL_Rect ret, int type,sge_TTFont *font,Uint16 *string, Sint16 x,Sint16 y, Uint32 fcol, Uint32 bcol,int Alpha)
{
	if(type==0){
  	sge_Update_OFF();	
		sge_FilledRect(Surface, ret.x, ret.y, ret.x+ret.w, ret.y+ret.h, bcol);
		ret=sge_tt_textout_UNI(Surface,font,string, x,y, fcol, bcol, 0);
		sge_Update_ON();
		sge_UpdateRect(Surface, ret.x, ret.y, ret.w, ret.h);
	}
	else if(type==1){
  	SDL_Rect temp;
	
  	sge_Update_OFF();	
		sge_FilledRect(Surface, ret.x, ret.y, ret.x+ret.w, ret.y+ret.h, bcol);
		temp=sge_tt_textout_UNI(Surface,font,string, x,y, fcol, bcol, 0);
		sge_Update_ON();
		sge_UpdateRect(Surface, ret.x, ret.y, ret.w, ret.h);
		ret=temp;
	}
	else{
  	SDL_Rect temp;
	
  	sge_Update_OFF();
		sge_FilledRect(Surface, ret.x, ret.y, ret.x+ret.w, ret.y+ret.h, bcol);	
		temp=sge_tt_textout_UNI(Surface,font,string, x,y, fcol, bcol, 0);
		sge_Update_ON();
		if(ret.w>=temp.w){
			sge_UpdateRect(Surface, ret.x, ret.y, ret.w, ret.h);
		}
		else{
			sge_UpdateRect(Surface, temp.x, temp.y, temp.w, temp.h);	
		}
		ret=temp;
	}
	return ret;
}

//==================================================================================
// Update function for TTF input that preserve background
//
// type=0 - in ret smaller then out ret
// type=1 - in ret bigger then out ret
// type=3 - safe
//==================================================================================
SDL_Rect nice_update(SDL_Surface *Surface,SDL_Surface *buffer,SDL_Rect ret, int type,sge_TTFont *font,Uint16 *string, Sint16 x,Sint16 y, Uint32 fcol, Uint32 bcol, int Alpha)
{
 	if(type==0){
  	sge_Update_OFF();	
		sge_Blit(buffer,Surface, ret.x, ret.y, ret.x, ret.y, ret.w, ret.h);
		ret=sge_tt_textout_UNI(Surface,font,string, x,y, fcol, bcol, Alpha);
		sge_Update_ON();
		sge_UpdateRect(Surface, ret.x, ret.y, ret.w, ret.h);
	}
	else if(type==1){
  	SDL_Rect temp;
	
  	sge_Update_OFF();	
		sge_Blit(buffer,Surface, ret.x, ret.y, ret.x, ret.y, ret.w, ret.h);
		temp=sge_tt_textout_UNI(Surface,font,string, x,y, fcol, bcol, Alpha);
		sge_Update_ON();
		sge_UpdateRect(Surface, ret.x, ret.y, ret.w, ret.h);
		ret=temp;
	}
	else{
  	SDL_Rect temp;
	
  	sge_Update_OFF();	
		sge_Blit(buffer,Surface, ret.x, ret.y, ret.x, ret.y, ret.w, ret.h);
		temp=sge_tt_textout_UNI(Surface,font,string, x,y, fcol, bcol, Alpha);
		sge_Update_ON();
		if(ret.w>=temp.w){
			sge_UpdateRect(Surface, ret.x, ret.y, ret.w, ret.h);
		}
		else{
			sge_UpdateRect(Surface, temp.x, temp.y, temp.w, temp.h);	
		}
		ret=temp;
	}

	return ret;
}
#endif /* _SGE_NOTTF */


//==================================================================================
// Handle keyrepeats
//==================================================================================
int keyrepeat(SDL_Event *event, int wait)
{
	int keydown=1,c=0,ret=0;
	SDL_Event ev;

	do{
		if(SDL_PollEvent(&ev)==1){
 			if(ev.type==SDL_QUIT){ret=-1;keydown=0;}
			if(ev.type==SDL_KEYUP || ev.type==SDL_KEYDOWN){ //Keyrepeat cancelled
				keydown=0;
				if(ev.type==SDL_KEYDOWN){
					SDL_PeepEvents(&ev,1, SDL_ADDEVENT, 0); //Return the newly pressed key to the event queue
				}
			} 		
		}
		SDL_Delay(10);
		c++;
		if(c>wait && keydown==1){ //trigers keyrepeat
			ret=1;
			SDL_PeepEvents(event,1, SDL_ADDEVENT, 0); //Return the old key to the event queue
			keydown=0;	
		}
	}while(keydown==1);						

	return ret;
}


//==================================================================================
// Insert a element
//==================================================================================
void insert_char(Uint16 *string, Uint16 ch, int pos, int max)
{
	if(pos>max || pos<0){return;}
	else if(pos==max){string[pos]=ch;}
	else{
  	for(int i=max; i>=pos; i--){
			string[i+1]=string[i];	
		}
		string[pos]=ch;
	}
}


//==================================================================================
// Delete a element
//==================================================================================
void delete_char(Uint16 *string, int pos, int max)
{
	if(pos>max || pos<0){return;}
	else if(pos==max){string[pos]=0;}
	else{
  	for(int i=pos; i<=max-1; i++){
			string[i]=string[i+1];	
		}
		string[max]=0;
	}
}


#ifndef _SGE_NOTTF
// These functions handle keyboard input and shows the result on screen. The text
// can be edited with [Backspace], [Delete], [Left arrow] and [Right arrow].
// Text input is terminated when [Return] or [Enter] is pressed, or if a quit event
// is recived.
// The sge_tt_input functions puts the result in 'string'.
//
// Flags: SGE_IBG   - Keeps background, else bye bye background
//        SGE_IDEL  - Delete text on exit
//        SGE_INOKR - No keyrepeat
// (use OR | to give more than one)
//
// If you want a 'default' text you can copy it to string before call and set pos to
// the first empty element in string - ex. "Hello" => pos=5. If not - zero.
// len is the max numbers of chars editable - ex. if you set the default text to "100"
// and only want 3 digits out, set len to 3. string should have atleast len+1 elements
// allocated
// This is *not* a fast, optimized function - but it gets the job done...

// Return:
// Zero or above - the lenght of the string
// -1 recieved a quit event (the lenght is lost)
// -2 invalid indata
// -3 out of memory

//==================================================================================
// Text input UNICODE (the core)
//==================================================================================
int sge_tt_input_UNI(SDL_Surface *screen,sge_TTFont *font,Uint16 *string, Uint8 flags, int pos,int len,Sint16 x,Sint16 y, Uint32 fcol, Uint32 bcol, int Alpha)
{
	if(len<pos || pos<0 || len<0){return -2;}

	Uint16 cur=124;//The charactar for the cursor - '|'
	int max;       //The strings size   	

	/* Set update function */
	SDL_Rect (*_update)(SDL_Surface *screen,SDL_Surface *buffer,SDL_Rect ret, int type,sge_TTFont *font,Uint16 *string, Sint16 x,Sint16 y, Uint32 fcol, Uint32 bcol, int Alpha);
	SDL_Surface *buffer;
	
	if(flags&SGE_FLAG1){   /* Keep background? */
		_update=nice_update;
		buffer=SDL_DisplayFormat(screen); /* Fixme: Yum! Memory! */
		if(buffer==NULL){SDL_SetError("SGE - Out of memory");return -3;}
	}
	else{               /* nope */
	 	_update=fast_update;
		buffer=NULL;
		Alpha=0;
	}

	SDL_Rect ret;

	max=pos;
	string[pos+1]=0;

	SDL_EnableUNICODE(1);

	/* Init cursor */
	string[pos]=cur;
	ret=sge_tt_textout_UNI(screen,font,string, x,y, fcol, bcol, Alpha);	

	SDL_Event event;
	int quit=0;
	do{
		/* Check events */
		SDL_WaitEvent(&event);
		if(event.type==SDL_QUIT){quit=-1;}
		else if(event.type==SDL_KEYDOWN && event.key.keysym.sym==SDLK_ESCAPE){quit=1;}
		else if(event.type==SDL_KEYDOWN && (event.key.keysym.sym==SDLK_RETURN || event.key.keysym.sym==SDLK_KP_ENTER)){quit=1;}
		else if(event.type==SDL_KEYDOWN && event.key.keysym.sym==SDLK_BACKSPACE){
    	if(pos>0){
				/* Delete char cursor-1 */
				delete_char(string,pos-1,max); pos--; max--;

				ret=_update(screen,buffer,ret,1,font,string, x,y, fcol, bcol, Alpha);
				
				/* Handle keyrepeat */
				if(!(flags&SGE_FLAG3))
					if(keyrepeat(&event, 20)==-1){quit=-1;}
			}
		}	
		else if(event.type==SDL_KEYDOWN && event.key.keysym.sym==SDLK_RIGHT){
			if(pos!=max && pos!=len){
				/* Move cursor right */
				delete_char(string,pos,max);pos++;
				insert_char(string,cur,pos,max);

				ret=_update(screen,buffer,ret,3,font,string, x,y, fcol, bcol, Alpha);

				/* Handle keyrepeat */
				if(!(flags&SGE_FLAG3))
					if(keyrepeat(&event, 20)==-1){quit=-1;}
			}		
		}	
		else if(event.type==SDL_KEYDOWN && event.key.keysym.sym==SDLK_LEFT){
			if(pos>0){
				/* Move cursor left */
				delete_char(string,pos,max);pos--;
				insert_char(string,cur,pos,max);

				ret=_update(screen,buffer,ret,3,font,string, x,y, fcol, bcol, Alpha);	

				/* Handle keyrepeat */
				if(!(flags&SGE_FLAG3))
					if(keyrepeat(&event, 20)==-1){quit=-1;}
			}	
		}
		else if(event.type==SDL_KEYDOWN && event.key.keysym.sym==SDLK_DELETE){
			/* Delete char cursor+1 */
			if(pos!=max && pos!=len){
				delete_char(string,pos+1,max);max--;

				ret=_update(screen,buffer,ret,1,font,string, x,y, fcol, bcol, Alpha);	
	
				/* Handle keyrepeat */
				if(!(flags&SGE_FLAG3))
					if(keyrepeat(&event, 20)==-1){quit=-1;}
			}	
		}
		else if(event.type==SDL_KEYDOWN && event.key.keysym.unicode!=0){
			/* Insert new char */
			if(max!=len){
				max++;
				insert_char(string, event.key.keysym.unicode, pos,max); pos++;	

				ret=_update(screen,buffer,ret,0,font,string, x,y, fcol, bcol, Alpha);

				/* Handle keyrepeat */
				if(!(flags&SGE_FLAG3))
					if(keyrepeat(&event, 40)==-1){quit=-1;}	
			}				
		}
	}while(quit==0);
	
	/* Remove the cursor from string */
	delete_char(string,pos,max);

	if(flags&SGE_FLAG2){ //Remove the text
		if(flags&SGE_FLAG1){
			sge_Blit(buffer,screen, ret.x, ret.y, ret.x, ret.y, ret.w, ret.h);
			sge_UpdateRect(screen, ret.x, ret.y, ret.w, ret.h);	
		}
		else{
			sge_FilledRect(screen,ret.x, ret.y, ret.x+ret.w, ret.y+ret.h,bcol);
		}
	}
	else{ //Draw text without cursor
		ret=_update(screen,buffer,ret,1,font,string, x,y, fcol, bcol, Alpha);	
	}


	if(flags&SGE_FLAG1){SDL_FreeSurface(buffer);}

	if(quit==-1){return -1;} //Waaa! The user killed me!
	return max;
}




CReadline::CReadline( SDL_Surface *a_poScreen, sge_TTFont *a_poFont,
		char *a_pcString, int a_iPos, int a_iLen,
		int a_x, int a_y, int a_w, Uint32 a_iFCol, Uint32 a_iBCol, int a_iAlpha )
{
	m_iResult = 0;
	m_poScreen = a_poScreen;
	m_poFont = a_poFont;
	m_piString = 0;
	m_iLen = -1;
	w = a_w;
	x = a_x;
	y = a_y;
	
	SDL_EnableUNICODE(1);

	// Create background copy.

	m_oWorkArea.x = x;
	m_oWorkArea.y = y - sge_TTF_FontAscent(m_poFont);
	m_oWorkArea.w = w;
	m_oWorkArea.h = sge_TTF_FontHeight(a_poFont);
	
	m_poBackground = sge_copy_surface( a_poScreen, m_oWorkArea.x, m_oWorkArea.y, m_oWorkArea.w, m_oWorkArea.h );
	
	Restart( a_pcString, a_iPos, a_iLen, a_iFCol, a_iBCol, a_iAlpha );
}


void CReadline::Restart( char *a_pcString, int a_iPos, int a_iLen,
		Uint32 a_iFCol, Uint32 a_iBCol, int a_iAlpha )
{
	if ( NULL == m_piString
		|| m_iLen <= a_iLen )
	{
		if ( m_piString ) delete[] m_piString;
		m_iLen = a_iLen;
		m_piString = new Uint16[a_iLen+2];
	}
	
	m_iResult = 0;

	m_pcLatin1String = a_pcString;
	m_iPos = a_iPos;
	m_iFCol = a_iFCol;
	m_iBCol = a_iBCol;
	m_iAlpha = a_iAlpha;

	// Convert the original string into an unicode string

	int i;
	for(i=0; i<m_iPos; i++)
	{
		m_piString[i]=(unsigned char)m_pcLatin1String[i];
	}
	m_piString[m_iPos]=0;

	// Insert the cursor at the end

	m_iCursor = 124;	// '|' character
	m_iMax=m_iPos;
	m_piString[m_iPos+1]=0;
	m_piString[m_iPos]=m_iCursor;
	Redraw();
	SDL_EnableKeyRepeat( SDL_DEFAULT_REPEAT_DELAY/2, SDL_DEFAULT_REPEAT_INTERVAL/2 );
}


CReadline::~CReadline()
{
	delete[] m_piString;
	SDL_FreeSurface( m_poBackground );
	m_poBackground = NULL;
	
	SDL_EnableKeyRepeat( 0, 0 );
}


/** Returns the current state of the line input.
\retval -1	A quit event was encountered.
\retval -2	Escape was pressed
\retval 0	Input is in progress.
\retval 1	Input has finished.
*/
int CReadline::GetResult()
{
	if ( m_iResult == 0 )
	{
		return m_iResult;
	}

	SDL_EnableKeyRepeat( 0, 0 );
	
	sge_Blit( m_poBackground, m_poScreen, m_oUpdateRect.x, m_oUpdateRect.y,
		m_oUpdateRect.x, m_oUpdateRect.y, m_oUpdateRect.w, m_oUpdateRect.h);
	sge_UpdateRect(m_poScreen, m_oUpdateRect.x, m_oUpdateRect.y, m_oUpdateRect.w, m_oUpdateRect.h);

	if ( m_iResult < 0 )
	{
		return m_iResult;
	}

	delete_char( m_piString, m_iPos, m_iMax );
	
	memset( m_pcLatin1String, 0, sizeof(char)*(m_iPos+1) );
	for( int i=0; i<=m_iMax; i++)
	{
		m_pcLatin1String[i] = (char)m_piString[i];
	}
	
	insert_char( m_piString, m_iCursor, m_iPos, m_iMax );
	
	return 1;
}




/** Internal method for redrawing the string. */
void CReadline::Update( int a_iCode )
{
	//m_oUpdateRect = nice_update( m_poScreen, m_poBackground, m_oUpdateRect, a_iCode, m_poFont, m_piString,
	//	x, y, m_iFCol, m_iBCol, m_iAlpha );

	SDL_Rect oOldClipRect;
	SDL_GetClipRect( m_poScreen, &oOldClipRect );
	SDL_SetClipRect( m_poScreen, &m_oWorkArea );
	
	sge_Blit( m_poBackground, m_poScreen, 0, 0, m_oWorkArea.x, m_oWorkArea.y, m_oWorkArea.w, m_oWorkArea.h );
	sge_tt_textout_UNI( m_poScreen, m_poFont, m_piString, x, y, m_iFCol, m_iBCol, m_iAlpha );
	sge_UpdateRect( m_poScreen, m_oWorkArea.x, m_oWorkArea.y, m_oWorkArea.w, m_oWorkArea.h );
	
	SDL_SetClipRect( m_poScreen, &oOldClipRect );
}


void CReadline::Redraw()
{
	m_oUpdateRect = sge_tt_textout_UNI( m_poScreen, m_poFont, m_piString, x, y, m_iFCol, m_iBCol, m_iAlpha );
	sge_UpdateRect( m_poScreen, m_oWorkArea.x, m_oWorkArea.y, m_oWorkArea.w, m_oWorkArea.h );
}


void CReadline::Clear()
{
	sge_Blit( m_poBackground, m_poScreen, 0, 0, m_oWorkArea.x, m_oWorkArea.y, m_oWorkArea.w, m_oWorkArea.h );
}


/** Runs the event queue until the input is finished.
\see GetResult
*/
int CReadline::Execute()
{
	int iRetval;
	SDL_Event e;
	while ( 1 )
	{
		SDL_WaitEvent( &e );
		HandleKeyEvent( e );
		iRetval = GetResult();
		if ( iRetval )
			break;
	}

	return iRetval;
}



void CReadline::HandleKeyEvent( SDL_Event& a_roEvent )
{
	if(a_roEvent.type==SDL_QUIT)
	{
		m_iResult = -1;
		return;
	}

	if ( a_roEvent.type != SDL_KEYDOWN )
	{
		return;
	}
	
	if( a_roEvent.key.keysym.sym==SDLK_ESCAPE )
	{
		m_iResult = -2;
		return;
	}
	
	if ( a_roEvent.key.keysym.sym==SDLK_RETURN
		|| a_roEvent.key.keysym.sym==SDLK_KP_ENTER )
	{
		m_iResult = 1;
		return;
	}

	if( a_roEvent.key.keysym.sym==SDLK_BACKSPACE )
	{
		if ( m_iPos == 0 )
		{
			return;
		}
		/* Delete char cursor-1 */
		delete_char(m_piString,m_iPos-1,m_iMax);
		m_iPos--;
		m_iMax--;
		Update( 1 );
		return;
	}
	
	if( a_roEvent.key.keysym.sym==SDLK_RIGHT
		&& m_iPos!=m_iMax && m_iPos!=m_iLen )
	{
		/* Move cursor right */
		delete_char(m_piString,m_iPos,m_iMax);
		m_iPos++;
		insert_char(m_piString,m_iCursor,m_iPos,m_iMax);
		Update( 3 );
		return;
	}

	if ( a_roEvent.key.keysym.sym==SDLK_LEFT
		&& m_iPos>0 )
	{
		/* Move cursor left */
		delete_char(m_piString,m_iPos,m_iMax);
		m_iPos--;
		insert_char(m_piString,m_iCursor,m_iPos,m_iMax);
		Update( 3 );
		return;
	}

	if( a_roEvent.key.keysym.sym==SDLK_DELETE )
	{
		if ( m_iPos!=m_iMax && m_iPos!=m_iLen )
		{
			delete_char(m_piString,m_iPos+1,m_iMax);
			m_iMax--;
			Update( 1 );
		}
		return;
	}
	
	if( a_roEvent.key.keysym.unicode!=0
		&& a_roEvent.key.keysym.unicode >=32
		&& a_roEvent.key.keysym.unicode <=255
		&& m_iMax != m_iLen )
	{
		m_iMax++;
		insert_char(m_piString, a_roEvent.key.keysym.unicode, m_iPos, m_iMax);
		m_iPos++;
		Update( 0 );
	}
}


/*
SDL_Rect CReadline::NiceUpdate( SDL_Surface *Surface,SDL_Surface *buffer,
	SDL_Rect ret, int type,sge_TTFont *font,Uint16 *string, Sint16 x,Sint16 y,
	Uint32 fcol, Uint32 bcol, int Alpha )
{
	if(type==0){
		sge_Update_OFF();
		sge_Blit(buffer,Surface, ret.x, ret.y, ret.x, ret.y, ret.w, ret.h);
		ret=sge_tt_textout_UNI(Surface,font,string, x,y, fcol, bcol, Alpha);
		sge_Update_ON();
		sge_UpdateRect(Surface, ret.x, ret.y, ret.w, ret.h);
	}
	else if(type==1){
  	SDL_Rect temp;

  	sge_Update_OFF();
		sge_Blit(buffer,Surface, ret.x, ret.y, ret.x, ret.y, ret.w, ret.h);
		temp=sge_tt_textout_UNI(Surface,font,string, x,y, fcol, bcol, Alpha);
		sge_Update_ON();
		sge_UpdateRect(Surface, ret.x, ret.y, ret.w, ret.h);
		ret=temp;
	}
	else{
  	SDL_Rect temp;

  	sge_Update_OFF();
		sge_Blit(buffer,Surface, ret.x, ret.y, ret.x, ret.y, ret.w, ret.h);
		temp=sge_tt_textout_UNI(Surface,font,string, x,y, fcol, bcol, Alpha);
		sge_Update_ON();
		if(ret.w>=temp.w){
			sge_UpdateRect(Surface, ret.x, ret.y, ret.w, ret.h);
		}
		else{
			sge_UpdateRect(Surface, temp.x, temp.y, temp.w, temp.h);
		}
		ret=temp;
	}

	return ret;
}
*/





//==================================================================================
// Text input UNICODE (RGB)
//==================================================================================
int sge_tt_input_UNI(SDL_Surface *screen,sge_TTFont *font,Uint16 *string,Uint8 flags, int pos,int len,Sint16 x,Sint16 y, Uint8 fR, Uint8 fG, Uint8 fB, Uint8 bR,Uint8 bG,Uint8 bB, int Alpha)
{
	return sge_tt_input_UNI(screen,font,string,flags,pos,len,x,y,SDL_MapRGB(screen->format, fR,fG,fB),SDL_MapRGB(screen->format, bR,bG,bB),Alpha);		
}


//==================================================================================
// Text input Latin1
//
// Will fail miserable if ret<0!
//==================================================================================
int sge_tt_input(SDL_Surface *screen,sge_TTFont *font,char *string,Uint8 flags, int pos,int len,Sint16 x,Sint16 y, Uint32 fcol, Uint32 bcol, int Alpha)
{
	if(len<pos || pos<0 || len<0){return -2;}

	#ifndef __GNUC__
	Uint16 *uni=new Uint16[len+2];  //ANSI C++
	#else
	Uint16 uni[len+2];
	#endif
	
	int ret;

	int i;
	if(pos!=0){
  	for(i=0; i<pos; i++){ //Convert Latin1 => Uni
    	uni[i]=(unsigned char)string[i];
		}		
	}
	uni[pos]=0;

	ret=sge_tt_input_UNI(screen,font,uni,flags,pos,len,x,y,fcol,bcol,Alpha);	
	
	memset(string,0,sizeof(char)*(pos+1));
	if(ret>0){
		for( i=0; i<=ret; i++){ //Convert Uni => Latin1
			string[i] = (char)uni[i];
		}
	}

	#ifndef __GNUC__
	delete[] uni;
	#endif
	
	return ret;
}


//==================================================================================
// Text input Latin1 (RGB)
//==================================================================================
int sge_tt_input(SDL_Surface *screen,sge_TTFont *font,char *string,Uint8 flags, int pos,int len,Sint16 x,Sint16 y, Uint8 fR, Uint8 fG, Uint8 fB, Uint8 bR,Uint8 bG,Uint8 bB, int Alpha)
{
	return sge_tt_input(screen,font,string,flags,pos,len,x,y,SDL_MapRGB(screen->format, fR,fG,fB),SDL_MapRGB(screen->format, bR,bG,bB),Alpha);		
}
#endif /* _SGE_NOTTF */

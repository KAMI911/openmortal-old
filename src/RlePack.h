/***************************************************************************
                          RlePack.h  -  description
                             -------------------
    begin                : Mon Sep 24 2001
    copyright            : (C) 2001 by upi
    email                : upi@apocalypse.rulez.org
 ***************************************************************************/

#ifndef __RLEPACK_H
#define __RLEPACK_H

#include "FighterEnum.h"

struct RlePack_P;
struct SDL_Surface;

/** 
\class CRlePack
\brief CRlePack is an array of images, compressed with runlength encoding.
\ingroup Media

OpenMortal stores the character graphics in CRlePack objects. The reason is 
simple: CRlePacks give an acceptable tradeoff between memory usage and 
blitting speed. Also the CRlePack allows the sprites to be draw horizontally 
flipped, thus saving memory (the mirrored version of the same sprite doesn't 
need to be stored).

The sprites in the RlePack are always paletted (8 bits per pixel). The size 
of the palette is between 1 and 256. The RlePack stores two copies of its 
palette: one is the "base" palette, as it was read from disk, the other is 
the "tinted" palette. The TintEnum contains values that can be passed to 
SetTint(). This is used for two things:

\li In case both players choose the same fighter, player 2's fighter is 
tinted so they won't get confused.
\li Some special effects (e.g. frozen) make the figther tinted as well.

CRlePack loads the sprites from a .DAT file in its constructor. If there is
an error (e.g. file doesn't exist), the number of sprites loaded will be 0.
CRlePack is usually used to store the many frames of a fighter in OpenMortal.
It is also used for the 'cast' in the CMainScreenDemo.

CRlePack doesn't concern itself with concepts such as "player" or "doodad", 
it merely stores the palette and sprites. This part of OpenMortal can be 
reused in any project with little changes.

\sa TintEnum
*/

class RlePack
{
public:
	RlePack( const char* a_pcFilename, int a_iNumColors );
	~RlePack();

	void		Clear();
	int			LoadFile( const char* a_pcFilename, int a_iNumColors );	
	int			Count();
	void		OffsetSprites( int a_iOffset );
	void		SetTint( TintEnum a_enTint );
	void		ApplyPalette();
	
	int			GetWidth( int a_iIndex );
	int			GetHeight( int a_iIndex );
	void		Draw( int a_iIndex, int a_iX, int a_iY, bool a_bFlipped=false );
	SDL_Surface* CreateSurface( int a_iIndex, bool a_bFlipped=false );
	
private:
	RlePack_P*	p;
};

#endif

/***************************************************************************
                          Background.cpp  -  description
                             -------------------
    begin                : Sun Jan 11 2004
    copyright            : (C) 2004 by upi
    email                : upi@feel
 ***************************************************************************/

#include "Background.h"

#include "SDL.h"
#include "sge_surface.h"
#include "gfx.h"
#include "common.h"
#include <string>
#include <fstream>



/* Calculating background distance:

BW: background width
SW: screen width (640)
AW: arena width (1920)
D: distance

(AW - SW) * D = BW - SW

D = (BW - SW) / (AW - SW)
D = (BW - 640) / 1280

*/



Background::Background()
{
	m_bOK = false;
	m_iNumber = 0;
	m_iFirstExtraLayer = 0;
}


Background::~Background()
{
	Clear();
}


void Background::Clear()
{
	for( LayerIterator it=m_aLayers.begin(); it!=m_aLayers.end(); ++it )
	{
		BackgroundLayer& roLayer = *it;
		if ( roLayer.m_poSurface )
		{
			SDL_FreeSurface( roLayer.m_poSurface );
			roLayer.m_poSurface = NULL;
		}
	}

	m_aLayers.clear();
	m_bOK = false;
	m_iNumber = 0;
	m_iFirstExtraLayer = 0;
}


void Background::Load( int a_iBackgroundNumber )
{
	char acFilename[FILENAME_MAX+1];

	// 1. Try loading a description-based background.
	sprintf( acFilename, "%s/gfx/level%d.desc", DATADIR, a_iBackgroundNumber );
	std::ifstream oInput( acFilename );
	if ( !oInput.is_open() )
	{
		// Description-based background not found. Try simple image-based
		// background.
		sprintf( acFilename, "level%d.jpg", a_iBackgroundNumber );
		SDL_Surface* poImage = LoadBackground( acFilename, 64 );
		if ( NULL == poImage )
		{
			// Couldn't load background.
			return;
		}
		
		BackgroundLayer oLayer;
		oLayer.m_poSurface = poImage;
		oLayer.m_iXOffset = 0;
		oLayer.m_iYOffset = 0;
		oLayer.m_dDistance = 1.0;
		m_aLayers.push_back( oLayer );
		
		m_iNumber = a_iBackgroundNumber;
		m_iFirstExtraLayer = m_aLayers.size();
		m_bOK = true;
		return;
	}
	
	// 2. Parse description.

	int iNumLayers;
	oInput >> iNumLayers;

	for ( int i=0; i<iNumLayers; ++i )
	{
		BackgroundLayer oLayer;
		std::string sFilename;
		oInput >> sFilename >> oLayer.m_iXOffset >> oLayer.m_iYOffset >> oLayer.m_dDistance;
		
		oLayer.m_poSurface = LoadBackground( sFilename.c_str(), 64, 0 );
		if ( NULL == oLayer.m_poSurface )
		{
			continue;
		}
		m_aLayers.push_back( oLayer );
	}
	
	m_iFirstExtraLayer = m_aLayers.size();
	m_bOK = m_aLayers.size() > 0;
	m_iNumber = m_bOK ? a_iBackgroundNumber : 0;
}


/** Adds a layer to the background.

The background object will assume ownership of the given structure, including
the surface within.
*/

void Background::AddExtraLayer( const BackgroundLayer& a_roLayer )
{
	m_aLayers.push_back( a_roLayer );
}


void Background::DeleteExtraLayers()
{
	while ( m_aLayers.size() > m_iFirstExtraLayer )
	{
		SDL_FreeSurface( m_aLayers.back().m_poSurface );
		m_aLayers.pop_back();
	}
}


bool Background::IsOK()
{
	return m_bOK;
}


void Background::Draw( int a_iXPosition, int a_iYPosition, int a_iYOffset )
{
	for ( LayerIterator it = m_aLayers.begin(); it != m_aLayers.end(); ++it )
	{
		BackgroundLayer& roLayer = *it;
		sge_Blit( roLayer.m_poSurface, gamescreen,
			0, 0,	// source position
			roLayer.m_iXOffset - (int)( ((double)a_iXPosition) * roLayer.m_dDistance ),
			roLayer.m_iYOffset - (int)( ((double)a_iYPosition) * roLayer.m_dDistance ) + a_iYOffset,
			gamescreen->w*3 + 100, gamescreen->h + 100 );
	}
}



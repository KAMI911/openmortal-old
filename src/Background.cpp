/***************************************************************************
                          Background.cpp  -  description
                             -------------------
    begin                : Sun Jan 11 2004
    copyright            : (C) 2004 by upi
    email                : upi@feel
 ***************************************************************************/

#include "Background.h"
#include "AnimBG.h"

#include "SDL.h"
#include "sge_surface.h"
#include "gfx.h"
#include "common.h"
#include <string>
#include <sstream>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <cctype>



/* Calculating background distance:

BW: background width
SW: screen width (640)
AW: arena width (1920)
D: distance

(AW - SW) * D = BW - SW

D = (BW - SW) / (AW - SW)
D = (BW - 640) / 1280

*/



// ---------------------------------------------------------------------------
// BackgroundLayer animation helpers
// ---------------------------------------------------------------------------

SDL_Surface* BackgroundLayer::CurrentSurface() const
{
	if ( !IsAnimated() )
		return m_poSurface;
	return m_aFrames[ m_iCurrentFrame ]->surface;
}

void BackgroundLayer::Advance()
{
	unsigned int now = SDL_GetTicks();

	// ---- Auto-scroll ----
	if ( m_dScrollX != 0.0 || m_dScrollY != 0.0 )
	{
		if ( m_uLastTickMs > 0 )
		{
			double dt = ( now - m_uLastTickMs ) / 1000.0;
			m_dScrollAccumX += m_dScrollX * dt;
			m_dScrollAccumY += m_dScrollY * dt;
			int ix = (int)m_dScrollAccumX;
			int iy = (int)m_dScrollAccumY;
			m_iScrollOffsetX += ix;
			m_iScrollOffsetY += iy;
			m_dScrollAccumX  -= ix;
			m_dScrollAccumY  -= iy;
		}
	}
	m_uLastTickMs = now;

	// ---- Animation frame advance ----
	if ( !IsAnimated() ) return;
	if ( m_iLoopsLeft == 0 ) return;   // held on last frame
	if ( now < m_uNextFrameMs ) return;

	int last = (int)m_aFrames.size() - 1;
	m_iCurrentFrame += m_iPlayDir;

	if ( m_iCurrentFrame > last )
	{
		if ( m_bPingPong )
		{
			m_iPlayDir      = -1;
			m_iCurrentFrame = last > 1 ? last - 1 : 0;
		}
		else
		{
			if ( m_iLoopsLeft > 0 && --m_iLoopsLeft == 0 )
				m_iCurrentFrame = last;  // hold last frame
			else
				m_iCurrentFrame = 0;    // loop back
		}
	}
	else if ( m_iCurrentFrame < 0 )
	{
		// Ping-pong hit the start — reverse
		m_iPlayDir      = 1;
		m_iCurrentFrame = last > 0 ? 1 : 0;
		if ( m_iLoopsLeft > 0 && --m_iLoopsLeft == 0 )
			m_iCurrentFrame = 0;
	}

	m_uNextFrameMs = now + m_aFrames[ m_iCurrentFrame ]->delay_ms;
}

// ---------------------------------------------------------------------------
// Helper: parse optional key=value attributes on the x/y/distance line
// ---------------------------------------------------------------------------

static void ParseLayerAttributes( const std::string& sLine, BackgroundLayer& oLayer )
{
	std::istringstream ss( sLine );
	std::string token;
	while ( ss >> token )
	{
		if ( token == "pingpong" )
		{
			oLayer.m_bPingPong = true;
		}
		else if ( token.size() > 6 && token.compare(0,6,"alpha=") == 0 )
		{
			int v = atoi( token.c_str() + 6 );
			oLayer.m_iAlpha = v < 0 ? 0 : ( v > 255 ? 255 : v );
		}
		else if ( token.size() > 6 && token.compare(0,6,"loops=") == 0 )
		{
			oLayer.m_iLoopsLeft = atoi( token.c_str() + 6 );
		}
		else if ( token.size() > 7 && token.compare(0,7,"scroll=") == 0 )
		{
			// scroll=dx,dy  (pixels/second, may be negative)
			const char* p = token.c_str() + 7;
			char* ep;
			oLayer.m_dScrollX = strtod( p, &ep );
			if ( *ep == ',' )
				oLayer.m_dScrollY = strtod( ep + 1, NULL );
		}
	}
}

// Helper: apply per-layer alpha to every surface in a layer
static void ApplyAlpha( BackgroundLayer& oLayer )
{
	if ( oLayer.m_iAlpha >= 255 ) return;
	Uint8 a = (Uint8)oLayer.m_iAlpha;
	if ( oLayer.IsAnimated() )
	{
		for ( size_t i = 0; i < oLayer.m_aFrames.size(); ++i )
			SDL_SetAlpha( oLayer.m_aFrames[i]->surface, SDL_SRCALPHA, a );
	}
	else if ( oLayer.m_poSurface )
	{
		SDL_SetAlpha( oLayer.m_poSurface, SDL_SRCALPHA, a );
	}
}

// Helper: load animation frames (GIF or .anim) into a layer
static bool LoadAnimLayer( const std::string& sFilename, BackgroundLayer& oLayer )
{
	char acPath[FILENAME_MAX+1];
	snprintf( acPath, sizeof(acPath), "%s/gfx/%s", DATADIR, sFilename.c_str() );

	// choose loader by extension (lowercase comparison)
	std::string sExt;
	size_t dot = sFilename.rfind('.');
	if ( dot != std::string::npos )
	{
		sExt = sFilename.substr( dot );
		for ( size_t i = 0; i < sExt.size(); ++i )
			sExt[i] = (char)tolower( (unsigned char)sExt[i] );
	}

	std::vector<AnimFrame> loaded;
	if ( sExt == ".gif" )
		loaded = LoadAnimatedGIF( acPath );
	else if ( sExt == ".anim" )
		loaded = LoadFrameSequence( acPath );

	if ( loaded.empty() ) return false;

	oLayer.m_iCurrentFrame = 0;
	oLayer.m_uNextFrameMs  = SDL_GetTicks() + loaded[0].delay_ms;
	for ( size_t f = 0; f < loaded.size(); ++f )
		oLayer.m_aFrames.push_back( new AnimFrame( loaded[f] ) );
	return true;
}

// ---------------------------------------------------------------------------
// Background class
// ---------------------------------------------------------------------------

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
		if ( roLayer.IsAnimated() )
		{
			for ( size_t i = 0; i < roLayer.m_aFrames.size(); ++i )
			{
				SDL_FreeSurface( roLayer.m_aFrames[i]->surface );
				delete roLayer.m_aFrames[i];
			}
			roLayer.m_aFrames.clear();
		}
		else if ( roLayer.m_poSurface )
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

		// Consume the rest of this line for optional attributes
		std::string sAttrs;
		std::getline( oInput, sAttrs );
		ParseLayerAttributes( sAttrs, oLayer );

		// Classify by extension
		std::string sExt;
		size_t dot = sFilename.rfind('.');
		if ( dot != std::string::npos )
		{
			sExt = sFilename.substr( dot );
			for ( size_t j = 0; j < sExt.size(); ++j )
				sExt[j] = (char)tolower( (unsigned char)sExt[j] );
		}

		bool bLoaded = false;
		if ( sExt == ".gif" || sExt == ".anim" )
		{
			bLoaded = LoadAnimLayer( sFilename, oLayer );
		}
		else
		{
			oLayer.m_poSurface = LoadBackground( sFilename.c_str(), 64, 0 );
			bLoaded = ( oLayer.m_poSurface != NULL );
		}

		if ( !bLoaded ) continue;

		ApplyAlpha( oLayer );
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
		roLayer.Advance();
		SDL_Surface* surf = roLayer.CurrentSurface();
		if ( !surf ) continue;

		// Wrap scroll offset within surface dimensions for seamless tiling
		int scrollX = roLayer.m_iScrollOffsetX;
		int scrollY = roLayer.m_iScrollOffsetY;
		if ( surf->w > 0 ) scrollX = ((scrollX % surf->w) + surf->w) % surf->w;
		if ( surf->h > 0 ) scrollY = ((scrollY % surf->h) + surf->h) % surf->h;

		int destX = roLayer.m_iXOffset - scrollX
		          - (int)( ((double)a_iXPosition) * roLayer.m_dDistance );
		int destY = roLayer.m_iYOffset - scrollY
		          - (int)( ((double)a_iYPosition) * roLayer.m_dDistance ) + a_iYOffset;

		sge_Blit( surf, gamescreen,
			0, 0, destX, destY,
			gamescreen->w*3 + 100, gamescreen->h + 100 );

		// If a scrolling layer's right edge doesn't reach the screen edge,
		// tile a second copy to the right for seamless infinite scroll.
		if ( ( roLayer.m_dScrollX != 0.0 || roLayer.m_dScrollY != 0.0 )
		     && surf->w > 0 && destX + surf->w < gamescreen->w )
		{
			sge_Blit( surf, gamescreen,
				0, 0, destX + surf->w, destY,
				gamescreen->w*3 + 100, gamescreen->h + 100 );
		}
	}
}



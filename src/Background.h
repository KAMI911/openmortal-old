/***************************************************************************
                          Background.h  -  description
                             -------------------
    begin                : Sun Jan 11 2004
    copyright            : (C) 2004 by upi
    email                : upi@feel
 ***************************************************************************/

#ifndef __BACKGROUND_H
#define __BACKGROUND_H


#include <vector>
struct SDL_Surface;

struct AnimFrame;  // from AnimBG.h

struct BackgroundLayer
{
	SDL_Surface*   m_poSurface;    ///< Static surface (NULL when animated)
	int            m_iXOffset;
	int            m_iYOffset;
	double         m_dDistance;

	// --- Animation ---
	std::vector<AnimFrame*> m_aFrames;  ///< Non-empty iff animated
	int            m_iCurrentFrame;
	unsigned int   m_uNextFrameMs;   ///< SDL_GetTicks() threshold for next advance

	// Playback control (animated layers only)
	bool           m_bPingPong;      ///< Reverse at end instead of looping
	int            m_iPlayDir;       ///< +1 = forward, -1 = backward (ping-pong)
	int            m_iLoopsLeft;     ///< -1 = infinite; 0 = hold last; N = remaining

	// --- Auto-scroll (pixels per second, both axes) ---
	double         m_dScrollX;
	double         m_dScrollY;
	double         m_dScrollAccumX;  ///< Sub-pixel accumulator
	double         m_dScrollAccumY;
	int            m_iScrollOffsetX; ///< Current integer scroll offset
	int            m_iScrollOffsetY;
	unsigned int   m_uLastTickMs;    ///< Last SDL_GetTicks() seen in Advance()

	// --- Per-layer alpha (0 = transparent, 255 = opaque) ---
	int            m_iAlpha;

	BackgroundLayer()
		: m_poSurface(0)
		, m_iXOffset(0), m_iYOffset(0), m_dDistance(1.0)
		, m_iCurrentFrame(0), m_uNextFrameMs(0)
		, m_bPingPong(false), m_iPlayDir(1), m_iLoopsLeft(-1)
		, m_dScrollX(0.0), m_dScrollY(0.0)
		, m_dScrollAccumX(0.0), m_dScrollAccumY(0.0)
		, m_iScrollOffsetX(0), m_iScrollOffsetY(0), m_uLastTickMs(0)
		, m_iAlpha(255) {}

	bool IsAnimated() const { return !m_aFrames.empty(); }

	/// Returns the surface to draw this tick (static or current anim frame).
	SDL_Surface* CurrentSurface() const;

	/// Advances scroll position and animation frame if enough time has passed.
	void Advance();
};
typedef std::vector<BackgroundLayer> LayerVector;
typedef LayerVector::iterator LayerIterator;

/*
The backgrounds are identified by their number. Single-layer backgrounds do
not have description files. Multi-layer backgrounds have a description file
which has the following format:
\li First line: number of layers (int)
\li For each layer:
    - Line 1: filename relative to gfx/ (.jpg/.png = static, .gif = animated GIF,
              .anim = frame-sequence descriptor)
    - Line 2: x-displacement(int) y-displacement(int) distance(double)
              followed by zero or more optional key=value attributes:
                alpha=N        per-layer opacity 0-255 (default 255)
                scroll=dx,dy   auto-scroll speed in pixels/second (default 0,0)
                pingpong       reverse animation direction at end (default off)
                loops=N        play N times then hold last frame (-1 = infinite)

Extra layers can be added to the background. These are for dead fighters in
team game mode.

Example .desc with all features:
  3
  sky.jpg
  0 0 0.068
  clouds.gif
  0 0 0.2 scroll=-40,0 alpha=200
  overlay_fog.png
  0 0 0.0 alpha=80
*/

class Background
{
public:
	Background();
	~Background();

	void		Clear();
	void		Load( int a_iBackgroundNumber );
	void		AddExtraLayer( const BackgroundLayer& a_roLayer );
	void		DeleteExtraLayers();

	bool		IsOK();
	void		Draw( int a_iXPosition, int a_iYPosition, int a_iYOffset );

protected:
	int			m_iNumber;
	int			m_iFirstExtraLayer;
	bool		m_bOK;
	LayerVector	m_aLayers;
};

#endif // __BACKGROUND_H

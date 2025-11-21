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

struct BackgroundLayer
{
	SDL_Surface*	m_poSurface;
	int				m_iXOffset;
	int				m_iYOffset;
	double			m_dDistance;
};
typedef std::vector<BackgroundLayer> LayerVector;
typedef LayerVector::iterator LayerIterator;

/*
The backgrounds are identified by their number. Single-layer backgrounds do
not have description files. Multi-layer backgrounds have a description file
which has the following format:
\li First line: number of layers(int)
\li For each layer: First line: filename (relative to gfx directory)
\li For each layer: Second line: x-displacement(int) y-displacement(int) distance(double)

Extra layers can be added to the background. These are for dead fighters in
team game mode.
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

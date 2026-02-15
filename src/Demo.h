/***************************************************************************
                         Demo.h  -  description
                             -------------------
    begin                : Wed Aug 16 22:18:47 CEST 2003
    copyright            : (C) 2003 by upi
    email                : upi@apocalypse.rulez.org
 ***************************************************************************/



#ifndef DEMO_H
#define DEMO_H

/**
\defgroup Demo Demos and special screens
*/

class FlyingChars;
struct SDL_Surface;

/**
\class CDemo
\ingroup Demo
\brief CDemo is the base class for every specific demo.

The demos are played by the DoDemos() function in a predefined order.
*/

class Demo
{
public:
	Demo();
	virtual ~Demo();
	
	virtual int Run();
	
protected:
	virtual int Advance( int a_iNumFrames, bool a_bFlip );
	virtual int AdvanceFlyingChars( int a_iNumFrames );
	virtual int AdvanceGame( int a_iNumFrames );
	virtual void OnMenu();
	
	
protected:
	FlyingChars*	m_poFlyingChars;
	bool			m_bAdvanceGame;
	SDL_Surface*	m_poBackground;
};





#endif // DEMO_H

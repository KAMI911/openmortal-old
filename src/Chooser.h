/***************************************************************************
                          Chooser.h  -  description
                             -------------------
    begin                : Tue Jan 27 2004
    copyright            : (C) 2004 by upi
    email                : upi@feel
 ***************************************************************************/


#ifndef CHOOSER_H
#define CHOOSER_H



#include "FighterEnum.h"
#include "common.h"
#include "SDL.h"

/**
CChooser is a control for choosing fighters.
It is used on the fighter selection screens.
\ingroup PlayerSelect
*/

class CChooser
{
public:
	CChooser();
	~CChooser();
	
	void			Start( SDL_Surface* m_poScreen );
	void			Stop();

	void			Resize( int x1, int y1, int x2, int y2 );
	void			Draw();
	void			MarkFighter( FighterEnum a_enFighter, Uint32 a_iColor );
	SDL_Surface*	GetPortrait( FighterEnum a_enFighter );
	void			DrawPortrait( FighterEnum a_enFighter, SDL_Surface* a_poScreen, const SDL_Rect& a_roRect );
	
	FighterEnum		GetCurrentFighter( int a_iPlayer );
	
	void			MoveRectangle( int a_iPlayer, int a_iDirection );
	void			SetRectangle( int a_iPlayer, FighterEnum a_enFighter );
	void			SetRectangleVisible( int a_iPlayer, bool a_bVisible );
	bool			IsRectangleVisible( int a_iPlayer );
	void			DrawRectangles( int a_iStartingWith );
	SDL_Rect		GetFighterRect( FighterEnum a_enFighter );

protected:
	void			Init();
	int				FighterToPosition( FighterEnum a_enFighter );
	FighterEnum		PositionToFighter( int a_iPosition );
	SDL_Rect		GetRect( int a_iPosition );

	void			ClearRectangle( int a_iPlayer );
	void			DrawRectangle( int a_iPlayer );
	void			DrawRectangle( int a_iPosition, Uint32 a_iColor );

protected:
	SDL_Surface*	m_poScreen;
	FighterEnum		m_aenFighters[100];
	SDL_Surface*	m_apoPortraits[100];
	bool			m_abRectangleVisible[MSZ_MAXPLAYERS];
	int				m_aiPlayerPosition[MSZ_MAXPLAYERS];
	Uint32			m_aiColors[MSZ_MAXPLAYERS];
	
	int				x1, y1, x2, y2;
	
	int				m_iNumberOfFighters;
	int				m_iRows;
	int				m_iCols;
	
};

extern CChooser g_oChooser;

#endif // CHOOSER_H

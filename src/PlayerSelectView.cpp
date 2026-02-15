#include "sge_tt_text.h"
#include "sge_bm_text.h"
#include "sge_primitives.h"
#include "RlePack.h"

#include "PlayerSelectView.h"
#include "PlayerSelect.h"

#include "Chooser.h"
#include "TextArea.h"
#include "Backend.h"
#include "State.h"
#include "gfx.h"
#include "common.h"




/**
Displays teams in the player selection screen for CPlayerSelectView.
\ingroup PlayerSelect
*/

class CTeamDisplay
{
public:
	CTeamDisplay( int a_iPlayer, SDL_Surface* a_poScreen, const SDL_Rect& a_roRect );
	~CTeamDisplay();

	void			DrawTitle( Uint32 a_iColor );
	void			DrawQuestionMark( Uint32 a_iColor, int a_iNumber=-1 );
	void			AddFighter( FighterEnum a_enFighter );
	void			DrawFighter( FighterEnum a_enFighter, int a_iNumber );
	SDL_Rect		GetRect( int a_iNumber=-1 );
	int				GetCount() { return m_iCount; }

protected:
	int				m_iPlayer;
	SDL_Surface*	m_poScreen;
	SDL_Rect		m_oRect;
	int				m_iTitleX;

	int				m_iCount;
	int				m_iMax;
};


CTeamDisplay::CTeamDisplay( int a_iPlayer, SDL_Surface* a_poScreen, const SDL_Rect& a_roRect )
{
	m_iPlayer = a_iPlayer;
	m_poScreen = a_poScreen;
	m_oRect = a_roRect;
	m_iCount = 0;
	m_iMax = g_oState.m_iTeamSize;
	m_iTitleX = m_oRect.x;

	const char* pcFormat = Translate("Team %d");
	SDL_Rect oTextSize = sge_TTF_TextSize( impactFont, pcFormat );
	int w = oTextSize.w;
	m_oRect.w -= w;
	m_oRect.x += w;

	DrawTitle( C_WHITE );

	for ( int i=0; i< m_iMax; ++i )
	{
		DrawQuestionMark( C_WHITE, i );
	}
}


CTeamDisplay::~CTeamDisplay() {}


void CTeamDisplay::DrawTitle( Uint32 a_iColor )
{
	char acBuffer[128];
	const char* pcFormat = Translate("Team %d");
	
	sprintf( acBuffer, pcFormat, m_iPlayer+1 );
	DrawTextMSZ( acBuffer, impactFont, m_iTitleX, m_oRect.y+m_oRect.h/2, 
		AlignVCenter|UseShadow, a_iColor, m_poScreen, false ); 
}


void CTeamDisplay::DrawQuestionMark( Uint32 a_iColor, int a_iNumber )
{
	if ( a_iNumber < 0 ) a_iNumber = m_iCount;
	if ( a_iNumber >= m_iMax ) return;

	SDL_Rect oRect = GetRect( a_iNumber );
	sge_Rect( m_poScreen, oRect.x, oRect.y, oRect.x+oRect.w, oRect.y+oRect.h, C_WHITE );
	DrawTextMSZ( "?", impactFont, oRect.x+oRect.w/2, oRect.y+oRect.h/2, AlignCenter, a_iColor, m_poScreen, false );
}


void CTeamDisplay::AddFighter( FighterEnum a_enFighter )
{
	DrawQuestionMark( C_WHITE );
	++m_iCount;
}


void CTeamDisplay::DrawFighter( FighterEnum a_enFighter, int a_iNumber )
{
	SDL_Rect oRect = GetRect( a_iNumber );
	g_oChooser.DrawPortrait( a_enFighter, m_poScreen, oRect );
	sge_Rect( m_poScreen, oRect.x, oRect.y, oRect.x+oRect.w, oRect.y+oRect.h, C_WHITE );
}


SDL_Rect CTeamDisplay::GetRect( int a_iNumber )
{
	if ( a_iNumber < 0 ) a_iNumber = m_iCount;
	if ( m_oRect.h > 75 )
	{
		m_oRect.y += (m_oRect.h - 75) / 2;
		m_oRect.h = 75;
	}

	int iGap = 5;
	int iWidth = m_oRect.w / m_iMax;
	if ( iWidth > m_oRect.h + iGap ) iWidth = m_oRect.h + iGap;

	SDL_Rect oRect;
	oRect.y = m_oRect.y;
	oRect.h = m_oRect.h;
	oRect.x = m_oRect.x + iWidth * a_iNumber;
	oRect.w = iWidth - iGap;

	return oRect;
}



IViewElement::IViewElement( CPlayerSelectView* a_poView, int a_iPriority )
{
	m_poView = a_poView;
	m_iPriority = a_iPriority;

	m_poView->AddViewElement( this, m_iPriority );
}


IViewElement::~IViewElement()
{
}


int IViewElement::GetPriority() const
{
	return m_iPriority;
}


/**
Flying portrait element for CPlayerSelectView.
The portrait of a given fighter will gracefully fly from the chooser
to a team display. The view element will delete itself afterwards.
\ingroup PlayerSelect
*/

class CFlyingPortraitViewElement: public IViewElement
{
public:
	CFlyingPortraitViewElement( CPlayerSelectView* a_poView, CTeamDisplay* a_poTeamDisplay,
		FighterEnum a_enFighter, const SDL_Rect& a_oSrcRect, const SDL_Rect& a_oDstRect ) 
		: IViewElement( a_poView, 0 )
	{
		m_poTeamDisplay = a_poTeamDisplay;
		m_iTeamNumber = m_poTeamDisplay->GetCount();
		m_enFighter	= a_enFighter;
		m_oRect		= a_oSrcRect;
		m_oDstRect	= a_oDstRect;

		m_dX		= m_oRect.x + m_oRect.w / 2;
		m_dY		= m_oRect.y + m_oRect.h / 2;
		m_dSize		= MIN( m_oRect.w, m_oRect.h );

		m_dSpeedX	= 0.0;
		m_dSpeedY	= - m_dY / 15;
		m_dSpeedSize= 0.0;

		m_dTargetX	= m_oDstRect.x + m_oDstRect.w / 2;
		m_dTargetY	= m_oDstRect.y + m_oDstRect.h / 2;
		m_dTargetSize = MIN( m_oDstRect.w, m_oDstRect.h);

		m_iTotalTime = 70 + m_dY / 7.5;
		m_iTime		= m_iTotalTime;

	}

	~CFlyingPortraitViewElement() {}

	void Advance( int a_iNumFrames )
	{
		if ( m_dX + m_dSpeedX * 0.5 * m_iTime > m_dTargetX ) m_dSpeedX -= a_iNumFrames * 0.5; else m_dSpeedX += a_iNumFrames * 0.5;
		if ( m_dY + m_dSpeedY * 0.5 * m_iTime > m_dTargetY ) m_dSpeedY -= a_iNumFrames * 0.5; else m_dSpeedY += a_iNumFrames * 0.5;

		m_dX += m_dSpeedX * a_iNumFrames;
		m_dY += m_dSpeedY * a_iNumFrames;

		double dTargetSize = (m_iTime > m_iTotalTime/2) ? 75.0 : m_dTargetSize;
		int iTargetTime = (m_iTime > m_iTotalTime/2) ? m_iTime : ( m_iTotalTime/2-m_iTime );
		if ( m_dSize + m_dSpeedSize * iTargetTime > dTargetSize ) m_dSpeedSize -= a_iNumFrames * 0.3; else m_dSpeedSize += a_iNumFrames * 0.3;

		m_dSize += m_dSpeedSize * a_iNumFrames;

		m_oRect.x = int( m_dX - m_dSize / 2 );
		m_oRect.y = int( m_dY - m_dSize / 2 );
		m_oRect.w = m_oRect.h = (int)m_dSize;

		m_iTime -= a_iNumFrames;
		if ( m_iTime < 0 )
		{
			m_poTeamDisplay->DrawFighter( m_enFighter, m_iTeamNumber );
			m_poView->RemoveViewElement( this );
			delete( this );
		}
	}

	void Draw()
	{
		g_oChooser.DrawPortrait( m_enFighter, gamescreen, m_oRect );
	}

protected:
	CTeamDisplay* m_poTeamDisplay;
	int			m_iTeamNumber;
	FighterEnum	m_enFighter;
	double		m_dX, m_dY, m_dSize;
	double		m_dSpeedX, m_dSpeedY, m_dSpeedSize;
	double		m_dTargetX, m_dTargetY, m_dTargetSize;
	int			m_iTotalTime;
	int			m_iTime;
	SDL_Rect	m_oDstRect;
	SDL_Rect	m_oRect;
	int			m_iNumber;
};

/** 
This view element implements the "opening courtain" effect at the start 
of the player selection routine. 
It removes itself when the effect is
finished.
\ingroup PlayerSelect
*/

class CCourtainViewElement: public IViewElement
{
public:
	CCourtainViewElement( CPlayerSelectView* a_poView ) : IViewElement( a_poView, 0 )
	{
		m_iCourtain = 0;
		m_iCourtainSpeed = 0;
		m_iCourtainTime = 80;
	}

	~CCourtainViewElement() {}

	void Advance( int a_iNumFrames )
	{
		if ( a_iNumFrames > 5 ) a_iNumFrames = 5;

		if ( m_iCourtainTime <= 0 )
		{
			SDL_SetClipRect( gamescreen, NULL );
			m_poView->RemoveViewElement( this );
			delete( this );
		}

		if ( m_iCourtain + m_iCourtainSpeed * m_iCourtainTime /2 < 320 * 4 )
			m_iCourtainSpeed += a_iNumFrames;
		else
			m_iCourtainSpeed -= a_iNumFrames;

		m_iCourtain += m_iCourtainSpeed * a_iNumFrames;
		m_iCourtainTime -= a_iNumFrames;

		if ( m_iCourtainTime > 0 )
		{
			SDL_Rect oRect;
			oRect.x = 320 - m_iCourtain/4; oRect.y = 0;
			oRect.w = m_iCourtain / 2; oRect.h = gamescreen->h;
			if ( oRect.x < 0 ) oRect.x = 0;
			if ( oRect.w > gamescreen->w ) oRect.w = gamescreen->w;
			SDL_SetClipRect( gamescreen, &oRect );
		}
	}

	void Draw()
	{
	}

protected:
	int		m_iCourtain;
	int		m_iCourtainSpeed;
	int		m_iCourtainTime;

};



/** 
This view element flashes the title and '?' mark of active teams. 
\ingroup PlayerSelect
*/

class CTeamFlashViewElement: public IViewElement
{
public:
	CTeamFlashViewElement( CPlayerSelectView* a_poView, int a_iPriority )
		: IViewElement( a_poView, a_iPriority )
	{
		for ( int i=0; i<MAXPLAYERS; ++i )
		{
			m_bActive[i] = false;
		}
	}

	~CTeamFlashViewElement() {}

	void Advance( int a_iNumFrames )
	{
		m_iCycle = (m_iCycle + a_iNumFrames*10) % 512;
	}

	void Draw()
	{
		int iLum = m_iCycle < 256 ? m_iCycle : 511-m_iCycle;

		for ( int i=0; i<g_oState.m_iNumPlayers; ++i )
		{
			if ( g_oChooser.IsRectangleVisible( i ) ) 
			{
				SDL_Color oColor;
				switch (i)
				{
				case 0:  oColor.r =  85; oColor.g = 255; oColor.b =  85; break;
				case 1:  oColor.r = 255; oColor.g =  85; oColor.b = 255; break;
				case 2:  oColor.r = 255; oColor.g = 255; oColor.b =  85; break;
				default: oColor.r =  85; oColor.g =  85; oColor.b = 255; break;
				}
				Uint32 iColor = SDL_MapRGB( gamescreen->format, oColor.r*iLum/256, oColor.g*iLum/256, oColor.b*iLum/256 );
				m_poView->GetTeamDisplay(i)->DrawTitle( iColor );
				m_poView->GetTeamDisplay(i)->DrawQuestionMark( iColor );
				m_bActive[i] = true;
			}
			else if ( m_bActive[i] )
			{
				m_poView->GetTeamDisplay(i)->DrawTitle( C_WHITE );
				m_bActive[i] = false;
			}
		}
	}

protected:
	int		m_iCycle;
	bool	m_bActive[MAXPLAYERS];
};





/*************************************************************************
						CPLAYERSELECTVIEW CLASS
*************************************************************************/

CPlayerSelectView::CPlayerSelectView( bool a_bNetworkGame, bool a_bTeamMode )
{
	m_bOver = false;
	m_bNetworkGame = a_bNetworkGame;
	m_bTeamMode = a_bTeamMode;
	m_bTeamMultiselect = g_oState.m_bTeamMultiselect;
	m_poReadline = NULL;
	m_poTextArea = NULL;
	m_iTime = 0;

	int i;
	for ( i=0; i<MAXPLAYERS; ++i ) m_apoTeamDisplays[i] = NULL;

	SDL_FillRect( gamescreen, NULL, C_BLACK );
	SDL_Flip( gamescreen );
	m_poBackground = LoadBackground( "FighterStats.jpg", 64 ); //m_bNetworkGame ? "PlayerSelect_chat.png" : "PlayerSelect.png", 111 );
	if ( m_poBackground ) SDL_SetColorKey( m_poBackground, 0, 0 );

	new CCourtainViewElement(this);

	if ( m_bTeamMode )
	{
		new CTeamFlashViewElement(this, 0);
		SDL_Rect oRect;
		oRect.x = 10; oRect.w = 620;
		oRect.y = 380; oRect.h = 40;
		m_apoTeamDisplays[0] = new CTeamDisplay( 0, m_poBackground, oRect );
		oRect.y = 430; oRect.h = 40;
		m_apoTeamDisplays[1] = new CTeamDisplay( 1, m_poBackground, oRect );
		if ( g_oState.m_iNumPlayers >= 3 )
		{
			oRect.y = 330;
			m_apoTeamDisplays[2] = new CTeamDisplay( 2, m_poBackground, oRect );
		}

		if ( g_oState.m_iNumPlayers >= 4 )
		{
			oRect.y = 280;
			m_apoTeamDisplays[3] = new CTeamDisplay( 3, m_poBackground, oRect );
		}

		if ( m_bTeamMultiselect )
		{
			m_iChooserTop = 25;
			m_iChooserHeight = 80 * 4;
			m_iChooserLeft = 158;
			m_iChooserWidth = 64*5;
		}
		else
		{
			DrawGradientText( "Choose A Fighter Dammit", titleFont, 10, m_poBackground );
			m_iChooserTop = 75;
			m_iChooserHeight = 80 * 4 - 50;
			m_iChooserLeft = 180;
			m_iChooserWidth = 430;
		}
		m_iFighterYOffset = -100;
		m_iFighterNameYOffset = -95;
	}
	else if ( m_bNetworkGame )
	{
		m_iChooserLeft = 158;
		m_iChooserTop = 26;
		m_iChooserWidth = 64 * 5;
		m_iChooserHeight = 64 * 4;
		m_iFighterYOffset = -130;
		m_iFighterNameYOffset = -170;
	}
	else
	{
		m_iChooserLeft = 158;
		m_iChooserTop = 74;
		m_iChooserWidth = 80*4;
		m_iChooserHeight = 80*5;
		m_iFighterYOffset = 0;
		m_iFighterNameYOffset = 0;
	}

	g_oChooser.Start( m_poBackground );
	g_oChooser.Resize( m_iChooserLeft, m_iChooserTop, m_iChooserLeft+m_iChooserWidth, m_iChooserTop+m_iChooserHeight );
	g_oChooser.Draw();
	
	if ( !m_bNetworkGame && !m_bTeamMode )
	{
		DrawGradientText( "Choose A Fighter Dammit", titleFont, 10, m_poBackground );
	}

	if ( m_bNetworkGame )
	{
		char acMsg[256];
		sprintf( acMsg, "Press Enter to chat, Page Up/Page Down to scroll..." );

		if ( m_bNetworkGame )
		{
			m_poReadline = new CReadline( m_bNetworkGame ? m_poBackground : NULL, chatFont,
				acMsg, strlen(acMsg), 256, 15, 465, 610, C_LIGHTCYAN, C_BLACK, 255 );
			m_poTextArea = new CTextArea( m_poBackground, chatFont, 15, 313, 610, 32*4 );
		}
	}

}


CPlayerSelectView::~CPlayerSelectView()
{
	delete m_apoTeamDisplays[0]; m_apoTeamDisplays[0] = NULL;
	delete m_apoTeamDisplays[1]; m_apoTeamDisplays[1] = NULL;

	TViewElements::iterator it;
	for ( it=m_apoElements.begin(); it!= m_apoElements.end(); ++it )
	{
		delete *it;
	}
	m_apoElements.clear();

	delete m_poTextArea;
	delete m_poReadline;
	SDL_FreeSurface( m_poBackground );
}


void CPlayerSelectView::Advance( int a_iNumFrames )
{
	m_iTime += a_iNumFrames;

	for ( int i=0; i<a_iNumFrames; ++i )
	{
		g_oBackend.AdvancePerl();

		TViewElements::iterator it;
	}

	TViewElements::iterator it;
	TViewElements::iterator itNext;
	for ( it = m_apoElements.begin(); it != m_apoElements.end(); )
	{
		itNext = it;
		++itNext;
		(*it)->Advance( a_iNumFrames );
		it = itNext;
	}
}


void CPlayerSelectView::Draw()
{
	TViewElements::iterator it;

	g_oBackend.ReadFromPerl();
	m_bOver = g_oBackend.m_iGameOver;

	g_oChooser.DrawRectangles(m_iTime);
	SDL_BlitSurface( m_poBackground, NULL, gamescreen, NULL );
	
	for ( it = m_apoElements.begin(); it != m_apoElements.end(); ++it )
	{
		(*it)->Draw();
	}

	for ( int i=0; i<g_oState.m_iNumPlayers; ++i )
	{
		if ( m_bTeamMode && 1==i && !m_bTeamMultiselect ) continue;

		const PlayerInfo& roPlayerInfo = g_oPlayerSelect.GetPlayerInfo( i );
		int iPlayerNameWidth = g_oPlayerSelect.GetFighterNameWidth( i );

		if ( g_oBackend.m_aoPlayers[i].m_iFrame )
		{
			roPlayerInfo.m_poPack->Draw(
				ABS(g_oBackend.m_aoPlayers[i].m_iFrame)-1,
				g_oBackend.m_aoPlayers[i].m_iX, g_oBackend.m_aoPlayers[i].m_iY + m_iFighterYOffset,
				g_oBackend.m_aoPlayers[i].m_iFrame < 0 );
		}
		int x = ( m_iChooserLeft - iPlayerNameWidth ) / 2;
		if ( x<10 ) x = 10;
		if ( i ) x = gamescreen->w - x - iPlayerNameWidth;
		
		sge_BF_textout( gamescreen, fastFont, g_oPlayerSelect.GetFighterName(i),
			x, gamescreen->h - 40 + m_iFighterNameYOffset );
	}

	SDL_Flip( gamescreen );
}


CReadline* CPlayerSelectView::GetReadline()
{
	return m_poReadline;
}


CTextArea* CPlayerSelectView::GetTextArea()
{
	return m_poTextArea;
}


void CPlayerSelectView::AddViewElement( IViewElement* a_poElement, int a_iPriority )
{

	m_apoElements.push_back( a_poElement );
}


void CPlayerSelectView::RemoveViewElement( IViewElement* a_poElement )
{
	m_apoElements.remove( a_poElement );
}


void CPlayerSelectView::AddFighterToTeam( int a_iPlayer, FighterEnum a_enFighter )
{
	new CFlyingPortraitViewElement( this, m_apoTeamDisplays[a_iPlayer], a_enFighter, 
		g_oChooser.GetFighterRect( a_enFighter ), 
		m_apoTeamDisplays[a_iPlayer]->GetRect() );
	m_apoTeamDisplays[a_iPlayer]->AddFighter( a_enFighter );
}


bool CPlayerSelectView::IsOver()
{
	return m_bOver;
}


CTeamDisplay* CPlayerSelectView::GetTeamDisplay( int a_iPlayer )
{
	return m_apoTeamDisplays[a_iPlayer];
}

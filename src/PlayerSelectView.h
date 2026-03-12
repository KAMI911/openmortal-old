#ifndef PLAYERSELECTVIEW_H
#define PLAYERSELECTVIEW_H

#include "FighterEnum.h"
#include "State.h"
#include <list>

struct SDL_Surface;
class CTextArea;
class CReadline;
class CPlayerSelectView;
class CTeamDisplay;


/**
A visual element that runs independently within CPlayerSelectView
\ingroup PlayerSelect
*/

class IViewElement
{
public:
	IViewElement( CPlayerSelectView* a_poView, int a_iPriority );
	virtual ~IViewElement();

	int				GetPriority() const;
	virtual void	Advance( int a_iNumFrames ) = 0;
	virtual void	Draw() = 0;

protected:
	CPlayerSelectView*		m_poView;
	int						m_iPriority;
};



/**
View part model-view-controller architecture of the player selection.
\ingroup PlayerSelect
*/

class CPlayerSelectView
{
public:
	CPlayerSelectView( bool a_bNetworkGame, bool a_bTeamMode );
	~CPlayerSelectView();
	
	void			Advance( int a_iNumFrames );
	void			Draw();
	CTextArea*		GetTextArea();
	CReadline*		GetReadline();

	void			AddViewElement( IViewElement* a_poElement, int a_iPriority );
	void			RemoveViewElement( IViewElement* a_poElement );
	void			AddFighterToTeam( int a_iPlayer, FighterEnum a_enFighter );
	bool			IsOver();

	CTeamDisplay*	GetTeamDisplay( int a_iPlayer );

protected:
	bool			m_bTeamMode;
	bool			m_bTeamMultiselect;
	bool			m_bNetworkGame;
	SDL_Surface*	m_poBackground;
	CTextArea*		m_poTextArea;
	CReadline*		m_poReadline;
	bool			m_bOver;

	int				m_iChooserTop;
	int				m_iChooserLeft;
	int				m_iChooserHeight;
	int				m_iChooserWidth;
	int				m_iFighterYOffset;
	int				m_iFighterNameYOffset;
	
	int				m_iTime;
	typedef std::list<IViewElement*> TViewElements;
	TViewElements	m_apoElements;
	CTeamDisplay*	m_apoTeamDisplays[MSZ_MAXPLAYERS];
};

#endif // PLAYERSELECTVIEW_H

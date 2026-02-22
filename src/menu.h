/***************************************************************************
                          menu.h  -  description
                             -------------------
    begin                : Sun Aug 3 2003
    copyright            : (C) 2003 by upi
    email                : upi@apocalypse.rulez.org
 ***************************************************************************/


#ifndef MENU_H
#define MENU_H


#include <string>
#include <vector>

class MenuItem;
class EnumMenuItem;
class TextMenuItem;


enum
{
/* Master menu structure:
MAIN MENU
*/
MENU_UNKNOWN,
	MENU_SURRENDER,
	MENU_SINGLE_PLAYER,
		MENU_EASY,
		MENU_MEDIUM,
		MENU_HARD,
	MENU_MULTI_PLAYER,
		MENU_MULTI_PLAYER_START,
		MENU_NUM_PLAYERS,		// 2-4
		MENU_TEAM_MODE,			// 1 vs 1, good vs evil, evil vs good, custom
		MENU_TEAM_SIZE,			// 2-10
		MENU_TEAM_MULTISELECT,	// yes / no
	MENU_NETWORK_GAME,
		MENU_SERVER,
		MENU_HOSTNAME,
		MENU_NICK,
		MENU_CONNECT,
		MENU_MORTALNET,
		MENU_CANCEL,
	MENU_OPTIONS,
		MENU_GAME_SPEED,
		MENU_GAME_TIME,			//	( :30 - 5:00 )
		MENU_TOTAL_HIT_POINTS,	// ( 25 - 1000 )
		MENU_SOUND,
			MENU_CHANNELS,		// MONO / STEREO

			MENU_MIXING_RATE,	// 11kHz / 22kHz / 44.1 kHz
			MENU_BITS,			// 8 bit / 16 bit
			MENU_MUSIC_VOLUME,	// (0% - 100%)
			MENU_SOUND_VOLUME,	// (0% - 100%)
			MENU_SOUND_OK,
		MENU_FULLSCREEN,
		MENU_KEYS_RIGHT,
		MENU_KEYS_LEFT,
		MENU_OPTIONS_OK,
	MENU_LANGUAGE,
	MENU_INFO,
	MENU_QUIT,					// (confirm)
};



/**
\brief Base class for menu systems in OpenMortal.
\ingroup GameLogic

Menus are displayed over the current screen and allow for the editing
of the game's state, SState. If you think in terms of model, view and
controller, then SState is the model and CMenu is both the view and
the controller.

Menus have menu items. The items in turn can edit various state
variables or invoke submenus. The menu ends then the user exits the
toplevel menu or invokes a command which unrolls the entire menu
stack.
*/

class Menu
{
public:
	Menu( const char* a_pcTitle );
	virtual ~Menu();

	virtual MenuItem*		AddMenuItem( const char* a_pcUtf8Text, SDLKey a_tShortcut = SDLK_UNKNOWN, int a_iCode = 0 );
	virtual EnumMenuItem*	AddEnumMenuItem( const char* a_pcUtf8Text, int a_iInitialValue, 
								const char** a_ppcNames, const int* a_piValues, int a_iCode = 0 );
	virtual TextMenuItem*	AddTextMenuItem( const char* a_pcTitle, const char* a_pcValue, int a_iCode = 0 );
	virtual MenuItem*		AddMenuItem( MenuItem* a_poItem );
	virtual void			AddOkCancel( int a_iOkCode = 0 );
	virtual MenuItem*		GetMenuItem( int a_iCode ) const;

	virtual void			ItemActivated( int a_iItemCode, MenuItem* a_poMenuItem );
	virtual void			ItemChanged( int a_iItemCode, int a_iValue, MenuItem* a_poMenuItem );
	virtual int				Run();

	virtual void			Draw();
	virtual void			Clear();
	virtual void			EnterName( const char* a_pcTitle, std::string& a_rsTarget, TextMenuItem* a_poMenuItem, int a_iMaxlen );

protected:

	virtual void			FocusNext();
	virtual void			FocusPrev();
	virtual void			InvokeSubmenu( Menu* a_poSubmenu );

	typedef std::vector<MenuItem*> ItemList;
	typedef ItemList::iterator ItemIterator;

	
	std::string				m_sTitle;
	ItemList				m_oItems;
	int						m_iCurrentItem;
	int						m_iReturnCode;
	bool					m_bDone;
};


/**
\ingroup GameLogic

Basic menu item. Menu items have a code which they pass to their parent
menu when they are activated. Menu items can be enabled or disabled.
*/
class MenuItem
{
public:
	MenuItem( Menu* a_poMenu, const char* a_pcUtf8Text, int a_iCode = -1 );
	virtual ~MenuItem();

	virtual void Draw();
	virtual void Clear();
	virtual void Activate();
	virtual void Increment() {};
	virtual void Decrement() {};
	
	virtual void SetText( const char* a_pcUtf8Text, bool a_bCenter );
	virtual void SetPosition( const SDL_Rect& a_roPosition );
	virtual void SetActive( bool a_bActive );
	virtual void SetEnabled( bool a_bEnabled );
	
	virtual bool GetEnabled() const { return m_bEnabled; }
	virtual int  GetCode() const { return m_iCode; }
	
protected:
	Menu*			m_poMenu;

	// appearance
	std::string		m_sUtf8Text;
	SDL_Rect		m_oPosition;
	bool			m_bCenter;
	Uint32			m_iHighColor;
	Uint32			m_iLowColor;
	Uint32			m_iInactiveColor;
	Uint32			m_iBackgroundColor;

	// data content
	int				m_iCode;
	bool			m_bActive;
	bool			m_bEnabled;
};


/**
\ingroup GameLogic

Enumerated menu items have an integer value, and a set of values and texts
which they can display for the user. The user can switch between these
values by incrementing and decrementing the value with the left and right
arrow keys.
*/

class EnumMenuItem: public MenuItem
{
public:
	EnumMenuItem(  Menu* a_poMenu, int a_iInitialValue, const char* a_pcUtf8Text, int a_iCode = -1 );
	virtual ~EnumMenuItem();
	
	int GetCurrentValue();
	const char* GetCurrentText();
	virtual void Draw();
	virtual void Increment();
	virtual void Decrement();
	
	virtual void SetEnumValues( const char ** a_ppcNames, const int * a_piValues );
	virtual void SetMaxValue( int a_iMaxValue );
	
protected:
	int				m_iValue;
	int				m_iMax;
	std::string		m_sUtf8Title;
	const char**	m_ppcNames;
	const int*		m_piValues;
};


/**
\ingroup GameLogic

Text menu items are like regular menu items, but they have a text value
which can be set. This value is displayed next to the regular name of
the menu item.
*/

class TextMenuItem: public MenuItem
{
public:
	TextMenuItem( Menu* a_poMenu, const char* a_pcInitialValue, const char* a_pcUtf8Title, int a_iCode );
	virtual ~TextMenuItem();

	virtual void Draw();
	virtual void SetValue( const char* a_pcValue );
	
protected:
	std::string		m_sTitle;
	std::string		m_sValue;
};



/**
\ingroup GameLogic

The Network displays and modifies the network connection parameters.
*/

class CNetworkMenu: public Menu
{
public:
	CNetworkMenu();
	virtual ~CNetworkMenu();
	
	void Connect();
	
	void ItemActivated( int a_iItemCode, MenuItem* a_poMenuItem );
	void ItemChanged( int a_iItemCode, int a_iValue, MenuItem* a_poMenuItem );

protected:
	bool			m_bOK;
	bool			m_bServer;
	std::string		m_sHostname;
	std::string		m_sNick;

	TextMenuItem*	m_poServerMenuItem;
	TextMenuItem*	m_poNickMenuItem;
};




void DoMenu();
void DoMenu( Menu& a_roMenu );

#endif

/***************************************************************************
                          menu.cpp  -  description
                             -------------------
    begin                : Sun Aug 3 2003
    copyright            : (C) 2003 by upi
    email                : upi@apocalypse.rulez.org
 ***************************************************************************/

#include "SDL.h"
#include "SDL_video.h"
#include "sge_primitives.h"

#include "menu.h"
#include "gfx.h"
#include "State.h"
#include "common.h"
#include "Audio.h"
#include "Backend.h"
#include "sge_tt_text.h"
#include "sge_surface.h"
#include "MortalNetwork.h"
#include "Joystick.h"

#include <stdarg.h>




const char* g_ppcGameTime[] = { "0:30", "0:45", "1:00", "1:15", "1:30", "1:45", "2:00", "3:00", "5:00", NULL };
const int g_piGameTime[] = { 30, 45, 60, 75, 90, 105, 120, 180, 300 };

const char* g_ppcHitPoints[] = { "BABY", "VERY LOW", "LOW", "NORMAL", "HIGH", "VERY HIGH", "NEAR IMMORTAL", NULL };
const int g_piHitPoints[] = { 1, 10, 50, 100, 150, 200, 500 };
const char* g_ppcGameSpeed[] = { "SNAIL RACE", "SLOW", "NORMAL", "TURBO", "KUNG-FU MOVIE", NULL };
const int g_piGameSpeed[] = { 16, 14, 12, 10, 8 };
const char* g_ppcChannels[] = { "MONO", "STEREO", NULL };
const int g_piChannels[] = { 1, 2 };
const char* g_ppcMixingRate[] = { "LOW", "MEDIUM", "HIGH", NULL };
const int g_piMixingRate[] = { 1, 2, 3 };
const char* g_ppcMixingBits[] = { "8 bit", "16 bit", NULL };
const int g_piMixingBits[] = { 1, 2 };


const char* g_ppcVolume[] = { "OFF", "10%", "20%", "30%", "40%", "50%", "60%", "70%", "80%", "90%", "100%", NULL };
const int g_piVolume[] = { 0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100 };

const char* g_ppcLanguage[] = { "English", "Spanish", "Francais", "Magyar", "Portugues", NULL };
const int g_piLanguage[] =    {    0,    1,    2,    3,    4 };
const char* g_ppcLanguageCodes[] = { "en", "es", "fr", "hu", "pt" };

const char* g_ppcServer[] = { "Connect to game", "Create game", NULL };
int g_piServer[] = { 0, 1 };

const char* g_ppcTeamMode[] = { "1 VS 1", "Good VS Evil", "Custom teams", NULL };
int g_piTeamMode[] = { SState::Team_ONE_VS_ONE, SState::Team_GOOD_VS_EVIL, SState::Team_CUSTOM };
const char* g_ppcTeamSize[] = { "2", "3", "4", "5", "6", "7", "8", "9", "10", NULL };
int g_piTeamSize[] = { 2, 3, 4, 5, 6, 7, 8, 9, 10 };
const char* g_ppcNumPlayers[] = { "2", "3", "4", NULL };
int g_piNumPlayers[] = { 2, 3, 4 };

const char* g_ppcYesNo[] = { "Yes", "No", NULL };
int g_piYesNo[] = { 1, 0 };

SDL_Surface* poBackground = NULL;



void InputKeys( int a_iPlayerNumber )
{
	SDL_BlitSurface( poBackground, NULL, gamescreen, NULL );
	DrawGradientText( "Input keys", titleFont, 10, gamescreen );
	SDL_Flip( gamescreen );
	
	static const char* apcKeyNames[9] = { "up", "down", "left", "right", "block", 
		"low punch", "high punch", "low kick", "high kick" };
	char acBuffer[1024];
	char acSide[128];
	char acFormat[128];
	int iY = 75;
	int iYIncrement = 35;
	SDLKey enKey;

	const char* pcJoyName = g_oJoystick.GetJoystickName( a_iPlayerNumber );
	if ( NULL != pcJoyName )
	{
		DrawTextMSZ( pcJoyName, inkFont, gamescreen->w/2, iY, AlignHCenter|UseShadow, C_LIGHTCYAN, gamescreen );
		iY += iYIncrement + 10;
	}
	
	DrawTextMSZ( "Press Escape to abort", inkFont, gamescreen->w/2, gamescreen->h-10-iYIncrement, 
		AlignHCenter|UseShadow, C_LIGHTGRAY, gamescreen );

	strcpy( acSide, Translate(a_iPlayerNumber ? "Left" : "Right") );
	strcpy( acFormat, Translate("%s player-'%s'?") );
	
	for ( int i=0; i<9; ++i )
	{
		// 1. PRINT THE FONT AND THE CURRENT KEYSYM
		sprintf( acBuffer, acFormat, acSide, Translate(apcKeyNames[i]) );
		int w = DrawTextMSZ( acBuffer, inkFont, 10, iY, UseShadow, C_WHITE, gamescreen );
		enKey = (SDLKey) g_oState.m_aiPlayerKeys[a_iPlayerNumber][i];
		g_oBackend.PerlEvalF( "GetKeysym(%d);", enKey );
		DrawTextMSZ( g_oBackend.GetPerlString("keysym"), inkFont, w+30, iY, UseShadow, C_LIGHTCYAN, gamescreen );
		
		// 2. INPUT THE NEW KEY
		
		enKey = GetKey( false );
		
		if ( SDLK_ESCAPE == enKey )
		{
			SDL_BlitSurface( poBackground, NULL, gamescreen, NULL );
			SDL_Flip( gamescreen );

			return;
		}

		// 3. PRINT THE NEW KEY
		
		g_oState.m_aiPlayerKeys[a_iPlayerNumber][i] = enKey;
		
		g_oBackend.PerlEvalF( "GetKeysym(%d);", enKey );
		sge_Blit( poBackground, gamescreen, w+10, iY, w+10, iY, gamescreen->w, 50 );
		DrawTextMSZ( g_oBackend.GetPerlString("keysym"), inkFont, w+30, iY, UseShadow, C_WHITE, gamescreen );
		sge_UpdateRect( gamescreen, w+10, iY, gamescreen->w, 50 );
		iY += iYIncrement;
	}
	
	sge_Blit( poBackground, gamescreen, 0, 470-iYIncrement, 0, 470-iYIncrement, gamescreen->w, gamescreen->h );
	sge_UpdateRect( gamescreen, 0, 470-iYIncrement, gamescreen->w, gamescreen->h );
	DrawTextMSZ( "Thanks!", inkFont, gamescreen->w/2, iY + 20, UseShadow | AlignCenter, C_WHITE, gamescreen );
	GetKey( true );
	SDL_BlitSurface( poBackground, NULL, gamescreen, NULL );
	SDL_Flip( gamescreen );
}




/***************************************************************************
							NETWORK MENU DEFINITION
 ***************************************************************************/

int g_iMessageY;
static char g_acMessageBuffer[1024];


void MortalNetworkResetMessages( bool a_bClear )
{
	if ( a_bClear )
	{
		SDL_FillRect( gamescreen, NULL, C_BLACK );
		SDL_Flip( gamescreen );
		g_iMessageY = 185;
	}
	else
	{
		g_iMessageY = 185;
	}
}

void MortalNetworkMessage( const char* format, ... )
{
	char acBuffer[1024];
	va_list ap;
	va_start( ap, format );
	vsprintf( acBuffer, format, ap );
	va_end( ap );
	DrawTextMSZ( acBuffer, impactFont, 20, g_iMessageY, 0, C_LIGHTGRAY, gamescreen );
	g_iMessageY += 25;
}


bool MortalNetworkCheckKey()
{
	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
			case SDL_QUIT:
				g_oState.m_bQuitFlag = true;
				return true;
			
			case SDL_KEYDOWN:
			case SDL_JOYBUTTONDOWN:
				return true;
				
		}	// switch statement
	}	// Polling events
	
	return false;
}



bool Connect( const char* a_pcHostname )
{
	MortalNetworkResetMessages( false );
	bool bOK = g_poNetwork->Start( a_pcHostname );

	if ( bOK )
	{
		// Store these settings as they are pretty good.
		g_oState.SetServer( a_pcHostname );
		g_oState.m_enGameMode = SState::IN_NETWORK;
	}
	else
	{
		// Print error message
		const char* acError = g_poNetwork->GetLastError();
		DrawTextMSZ( "Couldn't connect", inkFont, gamescreen->w/2, g_iMessageY, AlignHCenter|UseShadow, C_LIGHTRED, gamescreen );
		DrawTextMSZ( acError, impactFont, gamescreen->w/2, g_iMessageY + 40, AlignHCenter|UseShadow, C_LIGHTRED, gamescreen, false );
	}

	// Wait for a key, unless Quit was issued.

	if ( !g_oState.m_bQuitFlag )
	{
		if ( bOK )
		{
			// Wait for 1 sec, or keystroke.
			for ( int i=0; i<10; ++i )
			{
				if ( MortalNetworkCheckKey() ) break;
				SDL_Delay( 100 );
			}
		}
		else
		{
			GetKey( true );
		}
	}

	if ( g_oState.m_bQuitFlag )
	{
		bOK = false;
	}

	return bOK;
}



const char* FindString( const char* a_ppcNames[], const int a_piValues[], int a_iValue )
{
	for ( int i=0; NULL != a_ppcNames[i]; ++i )
	{
		if ( a_iValue == a_piValues[i] )
		{
			return a_ppcNames[i];
		}
	}

	return "(unknown)";
}



const char* GetGameTimeString( int a_iValue )
{
	strcpy( g_acMessageBuffer, Translate("GAME TIME: ") );
	strcat( g_acMessageBuffer, Translate(FindString( g_ppcGameTime, g_piGameTime, a_iValue)) );
	return g_acMessageBuffer;
}

const char* GetGameSpeedString( int a_iValue )
{
	strcpy( g_acMessageBuffer, Translate("GAME SPEED: ") );
	strcat( g_acMessageBuffer, Translate(FindString( g_ppcGameSpeed, g_piGameSpeed, a_iValue)) );
	return g_acMessageBuffer;
}

const char* GetHitPointsString( int a_iValue )
{
	strcpy( g_acMessageBuffer, Translate("STAMINA: ") );
	strcat( g_acMessageBuffer, Translate(FindString( g_ppcHitPoints, g_piHitPoints, a_iValue) ) );
	return g_acMessageBuffer;
}






CNetworkMenu::CNetworkMenu(): Menu( "Network Play Setup" )
{
	m_bOK = false;
	m_bServer = g_oState.m_bServer;
	m_sHostname = g_oState.m_acLatestServer;
	m_sNick = g_oState.m_acNick;
	
	AddMenuItem( "Find an opponent on MortalNet", SDLK_UNKNOWN, MENU_MORTALNET );
	AddMenuItem( "START NETWORK GAME!", SDLK_UNKNOWN, MENU_CONNECT );
	m_poNickMenuItem = AddTextMenuItem( "Nickname: ", m_sNick.c_str(), MENU_NICK );
	AddEnumMenuItem( "Network mode: ", m_bServer ? 1 : 0, g_ppcServer, g_piServer, MENU_SERVER );
	m_poServerMenuItem = AddTextMenuItem( "Connect to: ", m_sHostname.c_str(), MENU_HOSTNAME );
	m_poServerMenuItem->SetEnabled(!m_bServer);
	
	MenuItem* poItem = AddMenuItem( "Cancel", SDLK_UNKNOWN, MENU_CANCEL );
	SDL_Rect oRect;
	oRect.x = gamescreen->w - 150; oRect.w = 150;
	oRect.y = gamescreen->h - 50; oRect.h = 30;
	poItem->SetPosition( oRect );
}

CNetworkMenu::~CNetworkMenu() {}

void CNetworkMenu::Connect()
{
	Clear();
	SDL_Flip( gamescreen );

	m_bOK = ::Connect( m_bServer ? NULL : m_sHostname.c_str() );
	
	if ( g_oState.m_bQuitFlag || m_bOK )
	{
		m_bDone = true;
		m_iReturnCode = 100;
	}
	Clear();
	Draw();
}

void Menu::EnterName( const char* a_pcTitle, std::string& a_rsTarget, TextMenuItem* a_poMenuItem, int a_iMaxlen )
{
	Clear();
	Draw();

	if ( a_iMaxlen > 255 ) a_iMaxlen = 255;
	char acBuffer[256];
	strncpy( acBuffer, a_rsTarget.c_str(), 255 );
	acBuffer[255] = 0;

	int y = m_oItems.size() * 40 + 100;
	int x = DrawTextMSZ( a_pcTitle, impactFont, 20, y, 0, C_WHITE, gamescreen );

	int iRetval;
	{
		CReadline oReadline( gamescreen, impactFont, acBuffer, strlen(acBuffer), a_iMaxlen,
			20+x, y + sge_TTF_FontAscent(impactFont), gamescreen->w-40, C_LIGHTCYAN, C_BLACK, 255 );
		iRetval = oReadline.Execute();
	}

	if ( iRetval == -1 )
	{
		m_bDone = true;
		m_iReturnCode = 100;
		g_oState.m_bQuitFlag = true;
	}
	if ( iRetval > 0 )
	{
		a_rsTarget = acBuffer;
		a_poMenuItem->SetValue( acBuffer );
	}
	Clear();
	Draw();
}

void CNetworkMenu::ItemActivated( int a_iItemCode, MenuItem* a_poMenuItem )
{
	switch ( a_iItemCode )
	{
	case MENU_SERVER:
	{
		EnumMenuItem* poItem = (EnumMenuItem*) a_poMenuItem;
		if ( m_bServer )
		{
			poItem->Decrement();
		}
		else
		{
			poItem->Increment();
		}
		break;
	}

	case MENU_CONNECT:
		strcpy( g_oState.m_acNick, m_sNick.c_str() );
		Connect();
		break;

	case MENU_CANCEL:
		m_bOK = false;
		m_bDone = true;
		m_iReturnCode = -1;
		break;

	case MENU_HOSTNAME:
		EnterName( "Server name: ", m_sHostname, m_poServerMenuItem,128 );
		break;

	case MENU_NICK:
		EnterName( "Nickname: ", m_sNick, m_poNickMenuItem, 12 );
		strcpy( g_oState.m_acNick, m_sNick.c_str() );
		break;

	case MENU_MORTALNET:
		g_oState.m_enGameMode = SState::IN_CHAT;
		m_bOK = false;
		m_bDone = true;
		m_iReturnCode = 100;
		break;
	}
}

void CNetworkMenu::ItemChanged( int a_iItemCode, int a_iValue, MenuItem* a_poMenuItem )
{
	switch ( a_iItemCode )
	{
	case MENU_SERVER:
		m_bServer = a_iValue;
		m_poServerMenuItem->SetEnabled(!m_bServer);
		break;
	}
}






/***************************************************************************
							MENUITEM DEFINITION
 ***************************************************************************/
 
 

MenuItem::MenuItem( Menu* a_poMenu, const char* a_pcUtf8Text, int a_iCode )
: m_sUtf8Text( a_pcUtf8Text )
{
	m_poMenu = a_poMenu;
	m_iCode = a_iCode;

	m_oPosition.x = m_oPosition.y = 100;
	m_oPosition.w = m_oPosition.h = 100;
	m_bCenter = true;
	m_iHighColor = C_WHITE;
	m_iLowColor = C_LIGHTGRAY;
	m_iInactiveColor = C_DARKGRAY;
	m_iBackgroundColor = C_BLACK;
	m_bActive = false;
	m_bEnabled = true;

}

	
MenuItem::~MenuItem()
{
}

void MenuItem::Draw()
{
	if ( NULL != poBackground )
	{
		SDL_BlitSurface( poBackground, &m_oPosition, gamescreen, &m_oPosition );
	}
	else
	{
		SDL_FillRect( gamescreen, &m_oPosition, 0 );
	}
	

	int iX = m_oPosition.x;
	int iY = m_oPosition.y;

	if ( m_bCenter )
	{
		iX += m_oPosition.w / 2;
	}
	
	DrawTextMSZ( m_sUtf8Text.c_str(), inkFont, iX, iY,
		UseTilde | UseShadow | (m_bCenter ? AlignHCenter : 0),
		m_bEnabled ? (m_bActive ? m_iHighColor : m_iLowColor) : m_iInactiveColor,
		gamescreen );
	
	SDL_UpdateRect( gamescreen, m_oPosition.x, m_oPosition.y, m_oPosition.w, m_oPosition.h );	
}


void MenuItem::Clear()
{
//	debug( "Clear: %d:%d %dx%d\n", m_oPosition.x, m_oPosition.y, m_oPosition.w, m_oPosition.h );
	if (poBackground )
	{
		SDL_Rect oDest = m_oPosition;
		SDL_BlitSurface( poBackground, &m_oPosition, gamescreen, &oDest );
	}
	else
	{
		SDL_FillRect( gamescreen, &m_oPosition, C_WHITE );

	}
	
	SDL_UpdateRect( gamescreen, m_oPosition.x, m_oPosition.y, m_oPosition.w, m_oPosition.h );	
}


void MenuItem::Activate()
{
	if ( m_poMenu )
	{
		m_poMenu->ItemActivated( m_iCode, this );
	}
}

	
	
void MenuItem::SetText( const char* a_pcUtf8Text, bool a_bCenter )
{
	m_sUtf8Text = a_pcUtf8Text;
	m_bCenter = a_bCenter;
	Draw();
}

void MenuItem::SetPosition( const SDL_Rect& a_roPosition )
{
	m_oPosition = a_roPosition;
}

void MenuItem::SetActive( bool a_bActive )
{
	if ( m_bActive == a_bActive )
	{
		return;
	}
	m_bActive = a_bActive;
	Draw();
}
	
void MenuItem::SetEnabled( bool a_bEnabled )
{
	if ( m_bEnabled == a_bEnabled )
	{
		return;
	}
	m_bEnabled = a_bEnabled;
	Draw();
}




/***************************************************************************
							ENUMMENUITEM DEFINITION
 ***************************************************************************/
 
 

EnumMenuItem::EnumMenuItem( Menu* a_poMenu, int a_iInitialValue, const char* a_pcUtf8Text, int a_iCode )
: MenuItem( a_poMenu, a_pcUtf8Text, a_iCode )
{
	m_sUtf8Title = a_pcUtf8Text;
	m_iMax = -1;
	m_iValue = a_iInitialValue;
}

EnumMenuItem::~EnumMenuItem()
{
}


int EnumMenuItem::GetCurrentValue()
{
	return m_iValue <= m_iMax ? m_piValues[m_iValue] : 0;
}


const char* EnumMenuItem::GetCurrentText()
{
	return m_iValue <= m_iMax ? m_ppcNames[m_iValue] : "";
}


void EnumMenuItem::Draw()
{
	m_sUtf8Text = Translate( m_sUtf8Title.c_str() );
	if ( m_iValue <= m_iMax )
	{
		m_sUtf8Text += Translate(m_ppcNames[m_iValue]);
	}
	if ( m_iValue > 0 )
	{
		m_sUtf8Text = "< " + m_sUtf8Text;
	}
	if ( m_iValue < m_iMax )
	{
		m_sUtf8Text += " >";
	}
	
	MenuItem::Draw();
}



void EnumMenuItem::Increment()
{
	if ( m_iValue < m_iMax )
	{
		++m_iValue;
		Draw();
		m_poMenu->ItemChanged( m_iCode, m_piValues[m_iValue], this );
		Audio->PlaySample( "MENU_ITEM_VALUE_CHANGES" );
	}
}



void EnumMenuItem::Decrement()
{
	if ( m_iValue > 0 )
	{
		--m_iValue;
		Draw();
		m_poMenu->ItemChanged( m_iCode, m_piValues[m_iValue], this );
		Audio->PlaySample( "MENU_ITEM_VALUE_CHANGES" );
	}
}

	


void EnumMenuItem::SetEnumValues( const char ** a_ppcNames, const int * a_piValues )
{
	m_ppcNames = a_ppcNames;
	m_piValues = a_piValues;
	
	int i;
	bool bFoundValue = false;
	

	for ( i=0; NULL != a_ppcNames[i]; ++i )
	{
		if ( !bFoundValue &&
			m_iValue == a_piValues[i] )
		{
			bFoundValue = true;
			m_iValue = i;
		}
	}
	
	if ( !bFoundValue )
	{
		m_iValue = 0;
	}
	
	m_iMax = i-1;
}



void EnumMenuItem::SetMaxValue( int a_iMaxValue )
{
	for ( int i=0; NULL != m_ppcNames[i]; ++i )
	{
		if ( m_piValues[i] == a_iMaxValue )
		{
			m_iMax = i;
			break;
		}
	}

	debug( "EnumMenuItem::SetMaxValue: value %d not found\n", a_iMaxValue );
}






/***************************************************************************
							TextMenuItem DEFINITION
 ***************************************************************************/



TextMenuItem::TextMenuItem( Menu* a_poMenu, const char* a_pcInitialValue, const char* a_pcUtf8Title, int a_iCode )
	: MenuItem( a_poMenu, a_pcUtf8Title, a_iCode )

{
	m_sTitle = a_pcUtf8Title;
	m_sValue = a_pcInitialValue;
}


TextMenuItem::~TextMenuItem()
{
}


void TextMenuItem::Draw()
{
	m_sUtf8Text = Translate( m_sTitle.c_str() );
	m_sUtf8Text += m_sValue;
	
	MenuItem::Draw();
}


void TextMenuItem::SetValue( const char* a_pcValue )
{
	m_sValue = a_pcValue;
	Draw();
}









/***************************************************************************

							MENU DEFINITION
 ***************************************************************************/
 


Menu::Menu( const char* a_pcTitle )
: m_sTitle( a_pcTitle )
{
	m_iCurrentItem = 0;
	m_iReturnCode = -1;
	m_bDone = false;
}


Menu::~Menu()
{
	ItemIterator it;

	for ( it = m_oItems.begin(); it != m_oItems.end(); ++it )
	{
		delete *it;
	}
}


MenuItem* Menu::AddMenuItem( const char* a_pcUtf8Text, SDLKey a_tShortcut, int a_iCode )
{
	MenuItem* poItem = new MenuItem( this, a_pcUtf8Text, a_iCode );
	return AddMenuItem( poItem );
}



EnumMenuItem* Menu::AddEnumMenuItem( const char* a_pcUtf8Text, int a_iInitialValue, 
	const char** a_ppcNames, const int* a_piValues, int a_iCode )
{
	EnumMenuItem* poItem = new EnumMenuItem( this, a_iInitialValue, a_pcUtf8Text, a_iCode );
	poItem->SetEnumValues( a_ppcNames, a_piValues );
	
	AddMenuItem( poItem );
	return poItem;
}


TextMenuItem* Menu::AddTextMenuItem( const char* a_pcTitle, const char* a_pcValue, int a_iCode )
{
	TextMenuItem* poItem = new TextMenuItem( this, a_pcValue, a_pcTitle, a_iCode );
	AddMenuItem( poItem );
	return poItem;
}




MenuItem* Menu::AddMenuItem( MenuItem* a_poItem )
{
	m_oItems.push_back( a_poItem );
	
	SDL_Rect oRect;
	oRect.x = 0; oRect.w = gamescreen->w;
	oRect.y = m_oItems.size() * 40 + 100;
	oRect.h = sge_TTF_FontHeight( inkFont );

	a_poItem->SetPosition( oRect );
	
	return a_poItem;
}


void Menu::AddOkCancel( int a_iOkCode )
{
	SDL_Rect oRect;
	oRect.x = 0; oRect.w = 150;
	oRect.y = gamescreen->h - 50; oRect.h = sge_TTF_FontHeight( inkFont );

	MenuItem* poItem = AddMenuItem( "~OK", SDLK_o, a_iOkCode );
	poItem->SetPosition( oRect );
	
//	poItem = AddMenuItem( "Cancel", SDLK_UNKNOWN, 0 );
	oRect.x = gamescreen->w - 150;
	poItem->SetPosition( oRect );
}



MenuItem* Menu::GetMenuItem( int a_iCode ) const
{
	ItemList::const_iterator it;
	for ( it = m_oItems.begin(); it != m_oItems.end(); ++it )
	{
		if ( *it && (*it)->GetCode() == a_iCode )
		{
			return *it;
		}
	}
	debug( "Couldn't find menu item %d\n", a_iCode );
	return NULL;
}



void Menu::InvokeSubmenu( Menu* a_poMenu )
{
	Audio->PlaySample( "MENU_ITEM_INVOKED" );
	Clear();

	m_iReturnCode = a_poMenu->Run();
	if ( g_oState.m_bQuitFlag )
	{
		m_iReturnCode = 100;
		m_bDone = true;
	}
	
	if ( m_iReturnCode < 0 )
	{
		Audio->PlaySample( "MENU_SUBMENU_END" );
		Draw();
	}
	else
	{
		m_iReturnCode --;
		m_bDone = true;
	}
}


void Menu::ItemActivated( int a_iItemCode, MenuItem* a_poMenuItem )
{
	debug( "Menu::ItemActivated( %d )\n", a_iItemCode );
	
	switch ( a_iItemCode )
	{
		case MENU_QUIT:
			m_bDone = true;

			m_iReturnCode = 100;
			g_oState.m_bQuitFlag = true;
			break;
		
		case MENU_SURRENDER:
			m_bDone = true;
			m_iReturnCode = 100;
			if ( SState::IN_NETWORK == g_oState.m_enGameMode )
			{
				g_poNetwork->Stop();
			}
			g_oState.m_enGameMode = SState::IN_DEMO;
			break;
		
		case MENU_NETWORK_GAME:
		{
			Menu* poMenu = new CNetworkMenu();
			InvokeSubmenu( poMenu );
			delete poMenu;
			break;
		}

		case MENU_MULTI_PLAYER:
		{
			Menu* poMenu = new Menu( "Multi Player" );
			poMenu->AddMenuItem( "START GAME", SDLK_UNKNOWN, MENU_MULTI_PLAYER_START );
//			poMenu->AddEnumMenuItem( "Number of Players: ", g_oState.m_iNumPlayers, g_ppcNumPlayers, g_piNumPlayers, MENU_NUM_PLAYERS );
			poMenu->AddEnumMenuItem( "Team mode: ", g_oState.m_enTeamMode, g_ppcTeamMode, g_piTeamMode, MENU_TEAM_MODE );
			poMenu->AddEnumMenuItem( "Team size: ", g_oState.m_iTeamSize, g_ppcTeamSize, g_piTeamSize, MENU_TEAM_SIZE )
				->SetEnabled( SState::Team_CUSTOM == g_oState.m_enTeamMode );
			poMenu->AddEnumMenuItem( "Cloning allowed: ", g_oState.m_bTeamMultiselect, g_ppcYesNo, g_piYesNo, MENU_TEAM_MULTISELECT )
				->SetEnabled( SState::Team_CUSTOM == g_oState.m_enTeamMode );
			InvokeSubmenu( poMenu );
			delete poMenu;
			break;
		}
			
		case MENU_MULTI_PLAYER_START:
			m_bDone = true;
			m_iReturnCode = 100;
			g_oState.m_enGameMode = SState::IN_MULTI;
			break;
		
		case MENU_FULLSCREEN:
			Audio->PlaySample( "MENU_ITEM_INVOKED" );
			g_oState.ToggleFullscreen();
			if ( NULL != poBackground )
			{
				SDL_BlitSurface( poBackground, NULL, gamescreen, NULL );
			}
			else
			{
				SDL_FillRect( gamescreen, NULL, 0 );
			}
			a_poMenuItem->SetText( g_oState.m_bFullscreen ? "~FULLSCREEN ON" : "~FULLSCREEN OFF", true );
			Draw();
			break;
		
		case MENU_OPTIONS_OK:
			m_bDone = true;
			m_iReturnCode = -1;
			break;
		
		case MENU_OPTIONS:
		{
			Menu* poMenu = new Menu( "Options" );
			if ( g_oState.m_enGameMode != SState::IN_NETWORK || g_poNetwork->IsMaster() )
			{
				poMenu->AddEnumMenuItem( "GAME SPEED: ", g_oState.m_iGameSpeed, g_ppcGameSpeed, g_piGameSpeed, MENU_GAME_SPEED );
				poMenu->AddEnumMenuItem( "GAME TIME: ", g_oState.m_iGameTime, g_ppcGameTime, g_piGameTime, MENU_GAME_TIME );
				poMenu->AddEnumMenuItem( "STAMINA: ", g_oState.m_iHitPoints, g_ppcHitPoints, g_piHitPoints, MENU_TOTAL_HIT_POINTS );
			}
			poMenu->AddMenuItem( "~SOUND", SDLK_s, MENU_SOUND );
			poMenu->AddMenuItem( g_oState.m_bFullscreen ? "~FULLSCREEN ON" : "~FULLSCREEN OFF", SDLK_f, MENU_FULLSCREEN );
			poMenu->AddMenuItem( "~RIGHT PLAYER KEYS", SDLK_r, MENU_KEYS_RIGHT );
			poMenu->AddMenuItem( "~LEFT PLAYER KEYS", SDLK_l, MENU_KEYS_LEFT );
			poMenu->AddOkCancel( MENU_OPTIONS_OK );
			
			InvokeSubmenu( poMenu );
			delete poMenu;
			break;
		}
		
		case MENU_SOUND:
		{
			Menu* poMenu = new Menu( "Sound" );
			poMenu->AddEnumMenuItem( "CHANNELS: ", 1, g_ppcChannels, g_piChannels, MENU_CHANNELS )->SetEnabled(false);
			poMenu->AddEnumMenuItem( "SOUND QUALITY: ", 2, g_ppcMixingRate, g_piMixingRate, MENU_MIXING_RATE )->SetEnabled(false);
			poMenu->AddEnumMenuItem( "SOUND FIDELITY: ", 2, g_ppcMixingBits, g_piMixingBits, MENU_BITS )->SetEnabled(false);
			poMenu->AddEnumMenuItem( "MUSIC VOLUME: ", g_oState.m_iMusicVolume, g_ppcVolume, g_piVolume, MENU_MUSIC_VOLUME );
			poMenu->AddEnumMenuItem( "EFFECTS VOLUME: ", g_oState.m_iSoundVolume, g_ppcVolume, g_piVolume, MENU_SOUND_VOLUME );
			poMenu->AddOkCancel( MENU_SOUND_OK );
			
			InvokeSubmenu( poMenu );
			delete poMenu;
			break;
		}
		
		case MENU_SOUND_OK:
			m_bDone = true;
			m_iReturnCode = -1;
			break;
		
		case MENU_KEYS_LEFT:
			InputKeys(1);
			Draw();
			break;
		
		case MENU_KEYS_RIGHT:
			InputKeys(0);
			Draw();
			break;
		
		default:
			break;

	}
}



void Menu::ItemChanged( int a_iItemCode, int a_iValue, MenuItem* a_poMenuItem )
{
	debug( "Menu::ItemChanged( %d, %d )\n", a_iItemCode, a_iValue );
	
	switch ( a_iItemCode )
	{
		case MENU_NUM_PLAYERS:
			g_oState.m_iNumPlayers = a_iValue;
			break;

		case MENU_TEAM_MODE:
		{
			g_oState.m_enTeamMode = (SState::TTeamModeEnum) a_iValue;
			MenuItem* poItem;
			poItem = GetMenuItem( MENU_TEAM_SIZE );
			if ( poItem ) poItem->SetEnabled( SState::Team_CUSTOM==g_oState.m_enTeamMode );
			poItem = GetMenuItem( MENU_TEAM_MULTISELECT );
			if ( poItem ) poItem->SetEnabled( SState::Team_CUSTOM==g_oState.m_enTeamMode );
			break;
		}

		case MENU_TEAM_SIZE:
			g_oState.m_iTeamSize = a_iValue;
			break;

		case MENU_TEAM_MULTISELECT:
			g_oState.m_bTeamMultiselect = a_iValue;
			break;

		case MENU_MUSIC_VOLUME:
			g_oState.m_iMusicVolume = a_iValue;
			Audio->SetMusicVolume( a_iValue );
			break;
			
		case MENU_SOUND_VOLUME:
			g_oState.m_iSoundVolume = a_iValue;
			break;
			
		case MENU_GAME_TIME:
			g_oState.m_iGameTime = a_iValue;
			break;
			
		case MENU_GAME_SPEED:
			g_oState.m_iGameSpeed = a_iValue;
			break;
			
		case MENU_TOTAL_HIT_POINTS:
			g_oState.m_iHitPoints = a_iValue;
			break;
		
		case MENU_LANGUAGE:
			g_oState.SetLanguage( g_ppcLanguageCodes[ a_iValue ] );
			Clear();
			Draw();
			break;
	} // end of switch a_iItemCode
}


/** Run executes the menus, maybe invoking submenus as well. The
menus modify the global game state.

Returns 0, or the number of parent menus that should be cleared. */

int Menu::Run()
{
	if ( m_oItems[m_iCurrentItem]->GetEnabled() )
	{
		m_oItems[m_iCurrentItem]->SetActive(true);
	}
	else
	{
		FocusNext();
	}

	Draw();

	while ( !m_bDone )
	{
		if ( g_oState.m_bQuitFlag  )
		{
			m_bDone = true;
			m_iReturnCode = -1;
			break;
		}
		
		SDLKey enKey = GetKey( true );
		
		if ( g_oState.m_bQuitFlag ||
			SDLK_ESCAPE == enKey )
		{
			m_bDone = true;
			m_iReturnCode = -1;
			break;
		}
		
		switch ( enKey )
		{
			case SDLK_UP:
			{
				FocusPrev();
				break;
			} // end of SDLK_UP
			

			case SDLK_DOWN:
			{
				FocusNext();
				break;
			} // end of SDLK_DOWN
			
			case SDLK_LEFT:
			{
				MenuItem* poItem = m_oItems[m_iCurrentItem];
				poItem->Decrement();
				break;
			}
			
			case SDLK_RIGHT:
			{
				MenuItem* poItem = m_oItems[m_iCurrentItem];
				poItem->Increment();
				break;
			}

			
			case SDLK_RETURN:
			{
				MenuItem* poItem = m_oItems[m_iCurrentItem];
				if ( poItem->GetEnabled() )
				{
					poItem->Activate();
				}
			}
			
			default:
				break;
		} // end of switch
	}
	
	Clear();
	
	return m_iReturnCode;
}



void Menu::Draw()
{
	DrawGradientText( m_sTitle.c_str(), titleFont, 20, gamescreen );

	for ( ItemIterator it=m_oItems.begin(); it!=m_oItems.end(); ++it )
	{
		(*it)->Draw();
	}

	SDL_Flip( gamescreen );

}



void Menu::FocusNext()
{

	MenuItem* poItem = NULL;
	int iNextItem;
	
	for ( iNextItem = m_iCurrentItem+1; iNextItem < (int) m_oItems.size(); ++iNextItem )
	{
 		poItem = m_oItems[iNextItem];
		if ( poItem->GetEnabled() )
		{
			break;
		}
		poItem = NULL;
	}
	
	if ( NULL != poItem )
	{
		Audio->PlaySample("MENU_ITEM_SELECTION");
		m_oItems[m_iCurrentItem]->SetActive(false);
		m_oItems[iNextItem]->SetActive(true);
		m_iCurrentItem = iNextItem;
	}
}



void Menu::FocusPrev()
{
	MenuItem* poItem = NULL;

	int iPrevItem;
	
	for ( iPrevItem = m_iCurrentItem-1; iPrevItem >= 0; --iPrevItem )
	{
		poItem = m_oItems[iPrevItem];
		if ( poItem->GetEnabled() )
		{
			break;
		}
		poItem = NULL;
	}
	
	if ( NULL != poItem )
	{
		Audio->PlaySample("MENU_ITEM_SELECTION");
		m_oItems[m_iCurrentItem]->SetActive(false);
		m_oItems[iPrevItem]->SetActive(true);
		m_iCurrentItem = iPrevItem;
	}
}



void Menu::Clear()
{
	if (poBackground)
	{
		SDL_BlitSurface( poBackground, 0, gamescreen, 0 );
	}
	else
	{
		SDL_FillRect( gamescreen, NULL, 0 );
	}
}







void MakeMenuBackground()
{
	if ( poBackground != NULL )
	{
		return;
	}
	
	poBackground = SDL_ConvertSurface( gamescreen, gamescreen->format, SDL_SWSURFACE );
	if ( NULL == poBackground )
	{
		debug( "DoMenu: Couldn't allocate background.\n" );
		return;
	}

	if ( gamescreen->format->BitsPerPixel <= 8 )
	{
		int i;
		SDL_Rect oRect;
		oRect.x = 0; oRect.w = poBackground->w; oRect.h = 1;

		for ( i=0; i<poBackground->h; i += 2 )
		{
			oRect.y = i;
			SDL_FillRect( poBackground, &oRect, C_BLACK );
		}

		oRect.w = 1; oRect.y = 0; oRect.h = poBackground->h;


		for ( i=0; i<poBackground->w; i+=2 )
		{
			oRect.x = i;
			SDL_FillRect(poBackground, &oRect, C_BLACK );
		}
	}
	else
	{
		sge_FilledRectAlpha( poBackground, 0, 0, poBackground->w, poBackground->h, C_BLACK, 128 );
		sge_FilledRectAlpha( poBackground, 0, 0, poBackground->w, poBackground->h, C_BLUE, 64 );
	}

	SDL_BlitSurface( poBackground, 0, gamescreen, 0 );
	SDL_Flip( gamescreen );
}



void DoMenu( Menu& a_roMenu )
{
	Audio->PlaySample( "MENU_START" );

	MakeMenuBackground();

	a_roMenu.Clear();
	a_roMenu.Draw();
	a_roMenu.Run();

	if ( !g_oState.m_bQuitFlag )
	{
		Audio->PlaySample("MENU_END");
	}

	if ( NULL != poBackground )
	{
		SDL_FreeSurface( poBackground );
		poBackground = NULL;
	}
}



void DoMenu()
{
	MakeMenuBackground();
	
	Menu oMenu( "Main Menu" );
	
	if ( SState::IN_DEMO == g_oState.m_enGameMode )
	{
		oMenu.AddMenuItem( "~SINGLE PLAYER GAME", SDLK_s, MENU_SINGLE_PLAYER )->SetEnabled(false);
		oMenu.AddMenuItem( "~NETWORK GAME", SDLK_n, MENU_NETWORK_GAME );
		oMenu.AddMenuItem( "~MULTI PLAYER GAME", SDLK_m, MENU_MULTI_PLAYER );
	}
	else
	{
		oMenu.AddMenuItem( "~SURRENDER GAME", SDLK_s, MENU_SURRENDER );
	}
	oMenu.AddEnumMenuItem( "~LANGUAGE: ", g_oState.m_iLanguageCode, g_ppcLanguage, g_piLanguage, MENU_LANGUAGE );

	oMenu.AddMenuItem( "~OPTIONS", SDLK_o, MENU_OPTIONS );
	oMenu.AddMenuItem( "~INFO", SDLK_i, MENU_INFO )->SetEnabled(false);
	oMenu.AddMenuItem( "QUIT", SDLK_UNKNOWN, MENU_QUIT );
	
	DoMenu( oMenu );
}



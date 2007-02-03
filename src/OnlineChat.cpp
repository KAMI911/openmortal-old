/***************************************************************************
                          OnlineChat.cpp  -  description
                             -------------------
    begin                : Fri Jan 30 2004
    copyright            : (C) 2004 by upi
    email                : upi@feel
 ***************************************************************************/

#include "OnlineChat.h"
#include "TextArea.h"
#include "State.h"
#include "menu.h"

#include "Audio.h"
#include "sge_tt_text.h"
#include "gfx.h"
#include "common.h"
#include "config.h"
#include "Event.h"

//#include "SDL_video.h"


#define MORTALNETSERVER		"apocalypse.game-host.org"
#define MORTALNETWORKPORT	0x3A23

// Layout

#define READLINE_Y			440
#define NICKLIST_X			500

void MortalNetworkResetMessages( bool a_bClear );
void MortalNetworkMessage( const char* format, ... );
bool MortalNetworkCheckKey();




/*************************************************************************
                       CHAT / CHALLENGE MENUS
*************************************************************************/


enum
{
	MENU_DISCONNECT = 100,
	MENU_CHALLENGE,
	MENU_CHANGENICK,
	MENU_OK,

	MENU_ACCEPTCHALLENGE,
	MENU_REJECTHALLENGE,
};


/**
\ingroup Network
Displays the 'you have been challenged' screen.
*/

class CChallengeMenu: public CMenu
{
public:
	CChallengeMenu( std::string a_sChallenger )
	: CMenu( "You have been challenged!" ),
	m_sChallenger( a_sChallenger )
	{
		m_bAccepted = false;
		AddTextMenuItem( "ACCEPT CHALLENGE FROM ", m_sChallenger.c_str(), MENU_ACCEPTCHALLENGE );
		AddMenuItem( "NO, I'M REALLY SCARED", SDLK_UNKNOWN, MENU_REJECTHALLENGE );
	}
	
	~CChallengeMenu() {}

	bool GetAccepted()
	{
		return m_bAccepted;
	}

	void ItemActivated( int a_iItemCode, CMenuItem* a_poMenuItem )
	{
		switch ( a_iItemCode )
		{
		case MENU_ACCEPTCHALLENGE:
			m_bAccepted = true;
			// intentional fall through
		case MENU_REJECTHALLENGE:
			m_iReturnCode = 100;
			m_bDone = true;
			break;
		}
	}

protected:
	std::string			m_sChallenger;
	bool				m_bAccepted;
};




/**
\ingroup Network

The Chat menu is displayed instead of the regular CMenu when the user is 
connected to MortalNet. The Chat menu has a different set of operations.
*/


class CChatMenu: public CMenu
{
public:
	CChatMenu( const TNickMap& a_roNicks )
	: CMenu( "MortalNet Menu" ),
	m_roNicks( a_roNicks )
	{
		int i=0;
		TNickMap::const_iterator it;
		for ( it=m_roNicks.begin();
			it != m_roNicks.end() && i < 1023;
			++it, ++i )
		{
			if ( it->first == g_oState.m_acNick )
			{
				--i;
				continue;
			}
			m_apcNicks[i] = (it->first).c_str();
			m_aiNicks[i] = i;
		}
		m_apcNicks[i] = NULL;
		m_sNick = g_oState.m_acNick;
		
		AddMenuItem( "DISCONNECT", SDLK_UNKNOWN, MENU_DISCONNECT );
		if ( i > 0 )
		{
			AddEnumMenuItem( "CHALLENGE USER: ", 0, m_apcNicks, m_aiNicks, MENU_CHALLENGE );
		}
		m_poNickMenuItem = AddTextMenuItem( "Nickname: ", g_oState.m_acNick, MENU_CHANGENICK );
		AddMenuItem( "~OPTIONS", SDLK_o, MENU_OPTIONS );
		AddMenuItem( "QUIT", SDLK_UNKNOWN, MENU_QUIT );
		AddOkCancel( MENU_OK );
	}

	virtual ~CChatMenu() {}

	std::string GetNick()
	{
		return m_sNick;
	}

	std::string GetChallengedNick()
	{
		return m_sChallengedNick;
	}

	void ItemActivated( int a_iItemCode, CMenuItem* a_poMenuItem )
	{
		switch ( a_iItemCode )
		{
		case MENU_DISCONNECT:
			g_oState.m_enGameMode = SState::IN_DEMO;
			m_bDone = true;
			m_iReturnCode = -1;
			break;

		case MENU_CHANGENICK:
		{
			EnterName( "Nickname: ", m_sNick, m_poNickMenuItem, 12 );
			break;
		}

		case MENU_CHALLENGE:
		{
			m_sChallengedNick = ((CEnumMenuItem*)a_poMenuItem)->GetCurrentText();
			
			m_bDone = true;
			m_iReturnCode = 100;
			break;
		}

		case MENU_OK:
		{
			m_bDone = true;
			m_iReturnCode = -1;
			break;
		}
		
		default:
			CMenu::ItemActivated( a_iItemCode, a_poMenuItem );
		
		} // end of switch statement
		
	}

protected:
	std::string		m_sChallengedNick;
	std::string		m_sNick;
	
	const TNickMap&	m_roNicks;
	const char*		m_apcNicks[1024];
	int				m_aiNicks[1024];
	CTextMenuItem*	m_poNickMenuItem;
};







/*************************************************************************
                      CHAT CONNECT/DISCONNECT
*************************************************************************/


COnlineChat::COnlineChat()
{
	m_poScreen = gamescreen;
	m_poBackground = NULL;
	m_poSocket = NULL;
	m_poSocketSet = NULL;
	m_iIncomingBufferSize = 0;
	m_poReadline = NULL;
	m_poTextArea = NULL;
}


COnlineChat::~COnlineChat()
{
	Stop();
	
	if ( m_poBackground )
	{
		SDL_FreeSurface( m_poBackground );
		m_poBackground = NULL;
	}
}



#define CHECKCONNECTION if ( m_poSocket == NULL ) return;


bool COnlineChat::Start()
{
	if ( m_poSocket != NULL )
	{
		return true;		// Already connected.
	}
	
#define RETURNNOERROR {								\
		debug( "%s\n", m_sLastError.c_str() );		\
		return false; }
#define RETURNWITHERROR {							\
		m_sLastError = SDLNet_GetError();			\
		debug( "%s\n", m_sLastError.c_str() );		\
		return false; }
#define RETURNWITHADDITIONALERROR {					\
		m_sLastError += SDLNet_GetError();			\
		debug( "%s\n", m_sLastError.c_str() );		\
		return false; }

	m_poBackground = LoadBackground( "FighterStats.jpg", 64 );
	if ( NULL == m_poBackground )
	{
		return false;	// Should carp
	}

	SDL_BlitSurface( m_poBackground, NULL, m_poScreen, NULL );
	SDL_Flip( m_poScreen );

	MortalNetworkResetMessages( false );
	MortalNetworkMessage( Translate("Resolving hostname (%s)..."), MORTALNETSERVER );
	
	IPaddress oAddress;
	int iResult = SDLNet_ResolveHost( &oAddress, MORTALNETSERVER, MORTALNETWORKPORT );
	if ( iResult )
	{
		m_sLastError = Translate( "Couldn't resolve host." );
		RETURNNOERROR;
	}
	debug( "IP Address of server is 0x%x\n", oAddress.host );

	Uint32 ipaddr=SDL_SwapBE32(oAddress.host);
	MortalNetworkMessage("Connecting to %d.%d.%d.%d port %d",
		ipaddr>>24, (ipaddr>>16)&0xff, (ipaddr>>8)&0xff, ipaddr&0xff, oAddress.port);
	
	MortalNetworkMessage ( Translate("Waiting for connection... (press any key to abort)") );
	
	while (1)
	{
		m_poSocket = SDLNet_TCP_Open( &oAddress );
		if ( m_poSocket ) break;
		if ( MortalNetworkCheckKey() ) break;
		SDL_Delay( 100 );
	}
	if ( NULL == m_poSocket )
	{
		RETURNWITHERROR;
	}
	
	// CONNECTION ESTABLISHED. SEND INTRO PACKETS
	
	MortalNetworkMessage( Translate("Connection established.") );
	
	m_poSocketSet = SDLNet_AllocSocketSet( 1 );
	SDLNet_TCP_AddSocket( m_poSocketSet, m_poSocket );	// Check for errors?

	m_iIncomingBufferSize = 0;
	m_bMyNickIsOk = false;
	m_asNicks.clear();
	
	SendRawData( 'N', g_oState.m_acNick );
	
	return ( m_poSocket != NULL );
}



void COnlineChat::Stop()
{
	if ( m_poSocketSet )
	{
		SDLNet_FreeSocketSet( m_poSocketSet );
		m_poSocketSet = NULL;
	}
	if ( m_poSocket )
	{
		SDLNet_TCP_Close( m_poSocket );
		m_poSocket = NULL;
	}
	m_bMyNickIsOk = false;
	m_asNicks.clear();
	
	if ( g_oState.m_enGameMode == SState::IN_CHAT )
	{
		g_oState.m_enGameMode = SState::IN_DEMO;
	}
}







/*************************************************************************
                     SENDING / RECEIVING CHAT DATA
*************************************************************************/



void COnlineChat::SendRawData( char a_cID, const std::string& a_rsData )
{
	CHECKCONNECTION;
	
	SDLNet_TCP_Send( m_poSocket, &a_cID, 1 );
	SDLNet_TCP_Send( m_poSocket, (void*) a_rsData.c_str(), a_rsData.length() );

	char cNL = '\n';
	SDLNet_TCP_Send( m_poSocket, &cNL, 1 );
}


void COnlineChat::Update()
{
	CHECKCONNECTION;
	
	// 1. CHECK FOR STUFF TO READ
	
	int iRetval = SDLNet_CheckSockets( m_poSocketSet, 0 );
	if ( iRetval <= 0 )
	{
		return;
	}
	
	// 2. APPEND AT MOST 1024 bytes TO THE END OF THE INCOMING BUFFER

	// CHECK FOR BUFFER OVERFLOW HERE.
	if ( m_iIncomingBufferSize >= 1024*3 )
	{
		m_acIncomingBuffer[m_iIncomingBufferSize] = '\n';
		m_acIncomingBuffer[m_iIncomingBufferSize+1] = 'x';
		m_iIncomingBufferSize += 1;
	}
	else
	{
		iRetval = SDLNet_TCP_Recv( m_poSocket, m_acIncomingBuffer + m_iIncomingBufferSize, 1024 );
		if ( iRetval <= 0 )
		{
			m_sLastError = SDLNet_GetError();
			Stop();
			return;
		}
		m_iIncomingBufferSize += iRetval;
	}
	
	// 3. CONSUME THE INCOMING BUFFER.
	// We always make sure the incoming buffer starts with a package header.

	int iOffset = 0;
	m_acIncomingBuffer[ m_iIncomingBufferSize ] = 0;
	bool bRedraw = false;
	
	while ( iOffset < m_iIncomingBufferSize )
	{
		// 3.1. Find the end of the line.
		
		char* pcLineEnd = strchr( m_acIncomingBuffer + iOffset, '\n' );
		if ( NULL == pcLineEnd )
		{
			// The buffer doesn't have the end of the line (yet)
			debug( "Buffer doesn't have the NL (%d buffer, %d offset)\n", m_iIncomingBufferSize, iOffset );
			break;
		}
		
		*pcLineEnd = 0;

		debug( "Receiving message: %d ('%c') type, %d length, %d offset, %d in buffer.\n",
			m_acIncomingBuffer[iOffset], m_acIncomingBuffer[iOffset],
			pcLineEnd - m_acIncomingBuffer - iOffset, iOffset, m_iIncomingBufferSize );
		
		// 3.2. Receive the data.
		
		switch ( m_acIncomingBuffer[iOffset] )
		{
			case 'M':
			case 'S':
				ReceiveMsg( m_acIncomingBuffer[iOffset], m_acIncomingBuffer + iOffset + 1 ); bRedraw = true; break;
			case 'J':
			case 'L':
			case 'N':
			case 'Y':
			case 'W':
			case 'C':
				ReceiveUser( m_acIncomingBuffer[iOffset], m_acIncomingBuffer + iOffset + 1 ); bRedraw = true; break;
		}
		
		CHECKCONNECTION;
		
		iOffset = pcLineEnd - m_acIncomingBuffer + 1;
	}
	
	// 4. MOVE LEFTOVER DATA TO THE BEGINNING OF THE INCOMING BUFFER
	// The leftover data starts at iOffset, and is (m_iIncomingBufferSize-iOffset) long.
	
	memmove( m_acIncomingBuffer, m_acIncomingBuffer + iOffset, m_iIncomingBufferSize-iOffset );
	m_iIncomingBufferSize -= iOffset;

	if ( bRedraw && m_poTextArea )
	{
		Audio->PlaySample( "NETWORK_MESSAGE" );
		m_poTextArea->Redraw();
	}
}



/*************************************************************************
						INCOMING MESSAGES METHODS
*************************************************************************/


void COnlineChat::ReceiveMsg( char a_cID, char* a_pcData )
{
	debug( "Received message %c, content is '%s'\n", a_cID, a_pcData );
	if ( m_poTextArea )
	{
		m_poTextArea->AddString( a_pcData, a_cID == 'M' ? C_YELLOW : C_WHITE );
	}
}


void COnlineChat::ReceiveUser( char a_cID, char* a_pcData )
{
	debug( "Received user %c, content is '%s'\n", a_cID, a_pcData );
	// Split up the data.
	int iNumWords = 1;
	char* pcFirstWord = a_pcData;
	char* pcSecondWord = strchr( a_pcData, ' ' );
	if ( pcSecondWord )
	{
		iNumWords = 2;
		*pcSecondWord =  0;
		++pcSecondWord;
	}

	int iColor = C_WHITE;
	char acMsg[1024];
	acMsg[0] = 0;
	
	switch ( a_cID )
	{
	case 'J':
		sprintf( acMsg, "*** %s has joined MortalChat from %s.", pcFirstWord, pcSecondWord );
		if ( strcmp( pcFirstWord, g_oState.m_acNick ) != 0 )
		{
			m_asNicks[ pcFirstWord ] = pcSecondWord;
		}
		break;
	case 'L':
	{
		sprintf( acMsg, "*** %s has left MortalChat.", pcFirstWord );
		iColor = C_LIGHTRED;
		m_asNicks.erase( pcFirstWord );
		debug( "# of Nicks: %d\n", m_asNicks.size() );
		break;
	}
	case 'N':
	{
		sprintf( acMsg, "%s is now known as %s", pcFirstWord, pcSecondWord );
		iColor = C_LIGHTGRAY;
		std::string sHost = m_asNicks[pcFirstWord];
		m_asNicks.erase( pcFirstWord );
		m_asNicks[ pcSecondWord ] = sHost;
		break;
	}
	case 'Y':
		sprintf( acMsg, "You are now known as %s", pcFirstWord );
		iColor = C_LIGHTCYAN;

		m_bMyNickIsOk = true;
		m_asNicks.erase( g_oState.m_acNick );
		m_asNicks[ pcFirstWord ] = "";
		
		strncpy( g_oState.m_acNick, pcFirstWord, 127 );
		g_oState.m_acNick[127] = 0;
		break;
	case 'W':
		sprintf( acMsg, "%s is hailing from %s", pcFirstWord, pcSecondWord );
		iColor = C_LIGHTGRAY;
		m_asNicks[ pcFirstWord ] = pcSecondWord;
		break;
	case 'C':
		CChallengeMenu oMenu( pcFirstWord );
		DoMenu( oMenu );
		if ( oMenu.GetAccepted() )
		{
			MortalNetworkResetMessages( true );
			Connect( m_asNicks[pcFirstWord].c_str() );
		}
		Redraw();
		break;
	}

	if ( m_poTextArea && m_bMyNickIsOk )
	{
		m_poTextArea->AddString( acMsg, iColor );
		DrawNickList();
	}
}




/*************************************************************************
                         DRAWING THE SCREEN
*************************************************************************/


void COnlineChat::Redraw()
{
	SDL_BlitSurface( m_poBackground, NULL, m_poScreen, NULL );
	m_poReadline->Redraw();
	m_poTextArea->Redraw();
	DrawNickList();
	SDL_Flip( m_poScreen );
}



void COnlineChat::DrawNickList()
{
	SDL_Rect oNickListRect;
	oNickListRect.x = NICKLIST_X;
	oNickListRect.y = 10;
	oNickListRect.w = gamescreen->w - oNickListRect.x - 10;
	oNickListRect.h = READLINE_Y - oNickListRect.y - 10;

	SDL_Rect oOldClipRect;
	SDL_GetClipRect( m_poScreen, &oOldClipRect );
	SDL_SetClipRect( m_poScreen, &oNickListRect );
	
	SDL_BlitSurface( m_poBackground, &oNickListRect, m_poScreen, &oNickListRect );
	int y = oNickListRect.y + sge_TTF_FontAscent( chatFont );
	int yEnd = oNickListRect.y + oNickListRect.h - sge_TTF_FontDescent( chatFont );
	
	
	TNickMap::const_iterator it;
	for ( it = m_asNicks.begin();
		it != m_asNicks.end() && y <= yEnd;
		++it , y += sge_TTF_FontHeight( chatFont ) )
	{
		int iColor = (it->first) == g_oState.m_acNick ? C_LIGHTCYAN : C_WHITE;
		sge_tt_textout( m_poScreen, chatFont, (it->first).c_str(), oNickListRect.x, y, iColor, C_BLACK, 255 );
	}

	SDL_UpdateRect( m_poScreen, oNickListRect.x, oNickListRect.y, oNickListRect.w, oNickListRect.h );
	SDL_SetClipRect( m_poScreen, NULL );
}


extern int g_iMessageY;

void COnlineChat::DoOnlineChat()
{
	if ( !Start() )
	{
		if ( !g_oState.m_bQuitFlag )
		{
			DrawTextMSZ( "Couldn't connect", inkFont, 320, g_iMessageY, AlignHCenter|UseShadow, C_LIGHTRED, gamescreen );
			DrawTextMSZ( m_sLastError.c_str(), impactFont, 320, g_iMessageY + 40, AlignHCenter|UseShadow, C_LIGHTRED, gamescreen, false );
			GetKey( true );
		}
		return;
	}

	// CREATE USER INTERFACE ELEMENTS

	char		acMsg[256];
	SDL_Event	event;

	SDL_BlitSurface( m_poBackground, NULL, m_poScreen, NULL );
	SDL_Flip( m_poScreen );

	m_poTextArea = new CTextArea( m_poScreen, chatFont, 10, 10, NICKLIST_X-20, READLINE_Y-20 );
	m_poTextArea->TintBackground( C_DARKGRAY, 128 );
	m_poReadline = new CReadline( m_poScreen, chatFont,
		acMsg, 0, 255,
		10, READLINE_Y + sge_TTF_FontAscent(chatFont), 620, C_LIGHTCYAN, C_BLACK, 255 );

	while (1)
	{
		if ( g_oState.m_enGameMode != SState::IN_CHAT ) break;
		if ( NULL == m_poSocket ) break;
		Update();
		if ( NULL == m_poSocket ) break;
		SDL_Delay( 10 );

		if ( g_oState.m_bQuitFlag ) break;

		while (SDL_PollEvent(&event))
		{
			if ( SDL_KEYDOWN != event.type )
			{
				SMortalEvent oEvent;
				TranslateEvent( &event, &oEvent );
				switch (oEvent.m_enType)
				{
					case Me_MENU:
						Menu();
						break;
					
					case Me_QUIT:
						g_oState.m_bQuitFlag = true;
						break;
					
					default:
						break;
				}
				continue;
			}
			
			// HANDLE SCROLLING THE TEXT AREA
			
			if ( event.type == SDL_KEYDOWN )
			{
				SDLKey enKey = event.key.keysym.sym;
				if ( enKey == SDLK_PAGEUP || enKey == SDLK_KP9 )
				{
					m_poTextArea->ScrollUp();
					continue;
				}
				if ( enKey == SDLK_PAGEDOWN || enKey == SDLK_KP3 )
				{
					m_poTextArea->ScrollDown();
					continue;
				}
				if ( enKey == SDLK_ESCAPE )
				{
					Menu();
					continue;
				}
			}

			m_poReadline->HandleKeyEvent( event );
			int iResult = m_poReadline->GetResult();
			if ( iResult > 0 )
			{
				if ( strlen( acMsg ) )
				{
					SendRawData( 'M', acMsg );
					std::string sMsg = std::string("<") + g_oState.m_acNick + "> " + acMsg;
					m_poTextArea->AddString( sMsg.c_str(), C_LIGHTCYAN );
					m_poTextArea->Redraw();
				}
				m_poReadline->Clear();
				acMsg[0] = 0;
				m_poReadline->Restart( acMsg, strlen(acMsg), 256, C_LIGHTCYAN, C_BLACK, 255 );
			}
		}
	}

	if ( NULL == m_poSocket
		&& !g_oState.m_bQuitFlag )
	{
		DrawTextMSZ( "Connection closed.", inkFont, 320, 210, AlignHCenter | UseShadow, C_WHITE, m_poScreen );
		DrawTextMSZ( m_sLastError.c_str(), impactFont, 320, 250, AlignHCenter | UseShadow, C_WHITE, m_poScreen );
		SDL_Delay( 1000 );
		GetKey( true );
	}

	delete m_poReadline;
	m_poReadline = NULL;
	delete m_poTextArea;
	m_poTextArea = NULL;

	Stop();
}


void COnlineChat::Menu()
{
	CChatMenu oMenu( m_asNicks );
	DoMenu( oMenu );
	if ( !g_oState.m_bQuitFlag )
	{
		if ( oMenu.GetNick() != g_oState.m_acNick )
		{
			SendRawData( 'N', oMenu.GetNick() );
		}
		if ( oMenu.GetChallengedNick().length() )
		{
			SendRawData( 'C', oMenu.GetChallengedNick() );
			MortalNetworkResetMessages( true );
			Connect( NULL );
		}

		Redraw();
	}
}



/** Static global entry point for chatting. */
void DoOnlineChat()
{
	g_oState.m_enGameMode = SState::IN_CHAT;
	COnlineChat oChat;
	oChat.DoOnlineChat();
}



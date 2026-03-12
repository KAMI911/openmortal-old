//
// C++ Implementation: COnlineChatBEImpl
//
// Description: 
//
//
// Author: upi <upi@feel>, (C) 2004
//
// Copyright: See COPYING file that comes with this distribution
//
//

#include "OnlineChatBEImpl.h"
#include "common.h"


#define MSZ_CHAT_SERVER		"apocalypse.game-host.org"
#define MSZ_CHAT_PORT	0x3A23

IOnlineChatBE* g_poChatBE = NULL;

class Guard {
public:
	Guard( SDL_mutex* a_poMutex ) : m_poMutex( a_poMutex ) { SDL_mutexP( m_poMutex ); }
	~Guard() { SDL_mutexV( m_poMutex ); }
protected:
	SDL_mutex* m_poMutex;
};



///////////////////////////////////////////////////////////////////////////
// Constructor - destructor
///////////////////////////////////////////////////////////////////////////




COnlineChatBEImpl::COnlineChatBEImpl()
{
	m_enConnectionState = m_enNotifiedState = CS_Disconnected;
	m_enClientMode = CM_Chatroom;
	
	m_poSocket = NULL;
	m_poSocketSet = NULL;
	m_iIncomingBufferSize = 0;
	
	
	UserInfo* poMyInfo = new UserInfo;
	poMyInfo->m_enClientMode = m_enClientMode;
	m_apoUsers.push_front( poMyInfo );			// The first user is always "me"
	
	m_poLock = SDL_CreateMutex();
}


COnlineChatBEImpl::~COnlineChatBEImpl()
{
	SDL_DestroyMutex( m_poLock );
	m_poLock = NULL;
}


///////////////////////////////////////////////////////////////////////////
//      Internal methods that run in the communication thread
///////////////////////////////////////////////////////////////////////////


/** internalConnect establishes the connection to the server. It is 
assumed that the client is NOT connected before this method is called.
Successful execution sets the m_poSocket and other connection related
attributes, and the m_enConnectionState to CS_Connected
*/

void COnlineChatBEImpl::internalConnect()
{
	m_enConnectionState = CS_Connecting;
	
	// 1. RESOLVE MSZ_CHAT_SERVER
	
	notifyConnectionState( std::string(Translate("Resolving host: ")) + MSZ_CHAT_SERVER );
	
	IPaddress oAddress;
	int iResult = SDLNet_ResolveHost( &oAddress, MSZ_CHAT_SERVER, MSZ_CHAT_PORT );
	if ( iResult )
	{
		notifyConnectionState( Translate("Couldn't resolve host.") );
		return;
	}
	debug( "IP Address of server is 0x%x\n", oAddress.host );

	Uint32 ipaddr=SDL_SwapBE32(oAddress.host);
	
	char acMessage[128];
	sprintf( acMessage, "%s %d.%d.%d.%d port %d", Translate("Connecting to"),
		ipaddr>>24, (ipaddr>>16)&0xff, (ipaddr>>8)&0xff, ipaddr&0xff, oAddress.port);
	notifyConnectionState( acMessage );
	notifyConnectionState( Translate("Waiting for connection...") );
	
	// 2. ESTABLISH CONNECTION
	
	while (1)
	{
		if ( m_enConnectionState != CS_Connecting )
		{	// Operation was canceled by disconnect()
			return;
		}
		
		m_poSocket = SDLNet_TCP_Open( &oAddress );
		if ( m_poSocket ) break;
		SDL_Delay( 100 );
	}
	
	// CONNECTION ESTABLISHED. SEND INTRO PACKETS
	
	notifyConnectionState( Translate("Connection established.") );
	
	// Guarded section follows
	{
		Guard oGuard( m_poLock );
		
		if ( m_enConnectionState != CS_Connecting )
		{	// Operation was canceled
			SDLNet_TCP_Close( m_poSocket );
			m_poSocket = NULL;
			return;
		}
		
		m_poSocketSet = SDLNet_AllocSocketSet( 1 );
		SDLNet_TCP_AddSocket( m_poSocketSet, m_poSocket );	// Check for errors?
		
		m_iIncomingBufferSize = 0;
		m_apoUsers.resize(1);
		m_enConnectionState = CS_Connected;
	}
	
	sendNick();
	
	return;
}



void COnlineChatBEImpl::internalDisconnect()
{
	Guard oGuard( m_poLock );
	m_enConnectionState = CS_Disconnecting;
	
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
	
	m_apoUsers.resize(1);
	
	m_enConnectionState = CS_Disconnected;
	
	notifyConnectionState( Translate( "Disconnected from MortalNet." ) );
}


/** Reads one message from the server and processes it. */
void COnlineChatBEImpl::internalProcessMessage()
{
	if ( m_enConnectionState != CS_Connected )
	{
		return;
	}
	
	// 1. CHECK FOR STUFF TO READ
	
	int iRetval = SDLNet_CheckSockets( m_poSocketSet, 0 );
	if ( iRetval <= 0 )
	{
		SDL_Delay( 100 );
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
			notifyConnectionState( std::string(Translate("Connection error: ")) + SDLNet_GetError() );
			internalDisconnect();
			return;
		}
		m_iIncomingBufferSize += iRetval;
	}
	
	
}



void COnlineChatBEImpl::threadFunction()
{
	while (1)
	{
		// Wait until connection is initiated by connect()
		while ( m_enConnectionState == CS_Disconnected ) {
			SDL_Delay( 100 );
		}
		
		// Start to connect
		internalConnect();
		
		while ( m_enConnectionState == CS_Connected ) {
			internalProcessMessage();
		}
		
		if ( m_enConnectionState == CS_Disconnecting )
		{
			internalDisconnect();
		}
	}
}


void COnlineChatBEImpl::sendNick()
{
	sendRawData( 'N', m_sMyNick.c_str() );
}


void COnlineChatBEImpl::sendRawData( char a_cPrefix, const char* a_pcMessage )
{
	if ( m_enConnectionState == CS_Disconnected
		|| m_poSocket == NULL )
	{
		debug( "COnlineChatBEImpl::sendRawMessage: Cannot send %c %s: disconnected\n",
			a_cPrefix, a_pcMessage );
		return;
	}
	
	// TODO This could be more efficient, send with one SDLNet_TCP_Send
	SDLNet_TCP_Send( m_poSocket, &a_cPrefix, 1 );
	SDLNet_TCP_Send( m_poSocket, (void*) a_pcMessage, strlen(a_pcMessage) );

	char cNL = '\n';
	SDLNet_TCP_Send( m_poSocket, &cNL, 1 );
}




///////////////////////////////////////////////////////////////////////////
//    Synchronous control methods
///////////////////////////////////////////////////////////////////////////

void COnlineChatBEImpl::connect( std::string a_sNick )
{
	if ( m_enConnectionState != CS_Disconnected )
	{
		notifyConnectionState( m_enConnectionState, m_enConnectionState, "Cannot connect: already connected." );
		return;
	}
	
	Guard oGuard(m_poLock);
	
	m_sMyNick = a_sNick;
	m_enConnectionState = CS_Connecting;
}


void COnlineChatBEImpl::disconnect()
{
	if ( m_enConnectionState == CS_Disconnected )
	{
		notifyConnectionState( m_enConnectionState, m_enConnectionState, "Cannot disconnect: already disconnected." );
		return;
	}
	
	Guard oGuard(m_poLock);
	
	m_enConnectionState = CS_Disconnecting;
}


IOnlineChatBE::ConnectionStateEnum COnlineChatBEImpl::getConnectionState() const
{
	return m_enConnectionState;
}

 
 // Getting/setting my connection parameters
 
std::string COnlineChatBEImpl::getMyNick() const
{
	return m_sMyNick;
}


void COnlineChatBEImpl::setMyNick( std::string a_sNick )
{
	if ( m_enConnectionState == CS_Connected )
	{
	}
	else
	{
		m_sMyNick = a_sNick;
	}
}


IOnlineChatBE::ClientModeEnum COnlineChatBEImpl::getMyClientMode() const
{
	return m_enClientMode;
}


void COnlineChatBEImpl::setMyClientMode( IOnlineChatBE::ClientModeEnum a_enNewMode )
{
	if ( m_enClientMode == a_enNewMode )
	{
		return;
	}
	
	m_enClientMode = a_enNewMode;
	if ( m_enConnectionState == CS_Connected )
	{
		// TODO send this to the server
	}
}


const IOnlineChatBE::UserInfo& COnlineChatBEImpl::getMyUserInfo() const
{
	UserInfo* poInfo = m_apoUsers.front();
	
	poInfo->m_sNick = m_sMyNick;
	poInfo->m_enClientMode = m_enClientMode;
	
	return *poInfo;
}



 
 // User control
 
int COnlineChatBEImpl::getUserCount() const
{
	return m_apoUsers.size();
}


const IOnlineChatBE::UserInfo& COnlineChatBEImpl::getUserInfo( int a_iUserNumber ) const
{
	if ( a_iUserNumber <= 0
		|| a_iUserNumber >= getUserCount() )
	{
		return getMyUserInfo();
	}
	else
	{
		UserInfoList::const_iterator it = m_apoUsers.begin();
		for ( int i=0; i<a_iUserNumber; ++i )
		{
			++it;
		}
		return *(*it);
	}
}


IOnlineChatBE::ClientModeEnum COnlineChatBEImpl::getClientMode() const
{
	return m_enClientMode;
}


void COnlineChatBEImpl::setClientMode( ClientModeEnum a_enNewMode )
{
	if ( m_enClientMode == a_enNewMode )
	{
		return;
	}
	
	m_enClientMode = a_enNewMode;
	
	if ( m_enConnectionState == CS_Connected )
	{
		// TODO Send this to the server
	}
}



 
 // Event sinks
 

void COnlineChatBEImpl::addEventSink( IOnlineEventSink* a_poSink )
{
	m_apoSinks.push_back( a_poSink );
}


void COnlineChatBEImpl::removeEventSink( IOnlineEventSink* a_poSink )
{
	m_apoSinks.remove( a_poSink );
}


void COnlineChatBEImpl::removeAllEventSinks()
{
	m_apoSinks.clear();
}


void COnlineChatBEImpl::notifyEvent( const IOnlineChatBE::SChatEvent& a_roEvent )
{
	EventSinkList::const_iterator it;
	for ( it = m_apoSinks.begin(); it != m_apoSinks.end(); ++it )
	{
		(*it)->receiveEvent( a_roEvent );
	}
}


void COnlineChatBEImpl::notifyConnectionState( IOnlineChatBE::ConnectionStateEnum a_enOldState, 
	IOnlineChatBE::ConnectionStateEnum a_enNewState, const std::string& a_rsMessage )
{
	EventSinkList::const_iterator it;
	for ( it = m_apoSinks.begin(); it != m_apoSinks.end(); ++it )
	{
		(*it)->connectionStateChange( a_enOldState, a_enNewState, a_rsMessage );
	}
	
	m_enNotifiedState = a_enNewState;
}


void COnlineChatBEImpl::notifyConnectionState( const std::string& a_rsMessage )
{
	notifyConnectionState( m_enNotifiedState, m_enConnectionState, a_rsMessage );
}






IOnlineChatBE::UserInfo::UserInfo()
{
	m_enClientMode = CM_Chatroom;
}

IOnlineChatBE::UserInfo::UserInfo( const UserInfo& a_roRhs )
:	m_sNick( a_roRhs.m_sNick ),
	m_enClientMode( a_roRhs.m_enClientMode ),
	m_sHostName( a_roRhs.m_sHostName ),
	m_sHostAddress( a_roRhs.m_sHostAddress )
{
}

IOnlineChatBE::UserInfo& IOnlineChatBE::UserInfo::operator=( const UserInfo& a_roRhs )
{
	m_sNick = a_roRhs.m_sNick;
	m_enClientMode = a_roRhs.m_enClientMode;
	m_sHostName = a_roRhs.m_sHostName;
	m_sHostAddress = a_roRhs.m_sHostAddress;
	return *this;
}


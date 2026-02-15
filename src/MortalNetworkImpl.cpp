/***************************************************************************
                          MortalNetworkImpl.cpp  -  description
                             -------------------
    begin                : Sun Jan 25 2004
    copyright            : (C) 2004 by upi
    email                : upi@feel
 ***************************************************************************/

#include "MortalNetworkImpl.h"
#include "State.h"
#include "PlayerSelect.h"
#include "common.h"
#include "config.h"


#define MORTALNETWORKPORT 0x3A22
#define MAXSTRINGLENGTH 900


// Some graphics routines, defined in menu.cpp

void MortalNetworkResetMessages( bool a_bClear );
void MortalNetworkMessage( const char* format, ... );
bool MortalNetworkCheckKey();
const char* GetGameTimeString( int a_iValue );
const char* GetGameSpeedString( int a_iValue );
const char* GetHitPointsString( int a_iValue );




CMortalNetwork* g_poNetwork = NULL;




void CMortalNetwork::Create()	// static
{
	if ( NULL == g_poNetwork )
	{
		g_poNetwork = new CMortalNetworkImpl;
	}
}



CMortalNetworkImpl::CMortalNetworkImpl()
{
	m_enState = NS_DISCONNECTED;
	m_bServer = false;
	m_bMaster = false;
	m_poSocket = NULL;

	m_enRemoteFighter = UNKNOWN;
	m_bRemoteReady = false;
	m_bRoundOver = false;
	m_iWhoWon = -1;
	m_bGameOver = false;
	m_iGameTick = 0;
	m_iHurryupCode = 0;
	m_iIncomingBufferSize = 0;
	
	if(SDLNet_Init()==-1)
	{
		m_bNetworkAvailable = false;
		m_sLastError = SDLNet_GetError();
		debug ( "Error opening SDLNet: %s\n", m_sLastError.c_str() );
	}

	m_bNetworkAvailable = true;
}


CMortalNetworkImpl::~CMortalNetworkImpl()
{
	Stop();
}


bool CMortalNetworkImpl::Start( const char* a_pcServerName )
{
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

	if ( !m_bNetworkAvailable )
	{
		return false;
	}

	debug( "CMortalNetworkImpl::Start( %s )\n", a_pcServerName ? a_pcServerName : "NULL" );
	IPaddress oAddress;

	if ( a_pcServerName )
	{
		MortalNetworkMessage( Translate("Resolving hostname (%s)..."), a_pcServerName );
	}

	int iResult = SDLNet_ResolveHost( &oAddress, (char*) a_pcServerName, MORTALNETWORKPORT );
	if ( iResult )
	{
		m_sLastError = Translate( "Couldn't resolve host." );
		RETURNNOERROR;
	}
	debug( "IP Address of server is 0x%x\n", oAddress.host );
	
	if ( a_pcServerName )
	{
		Uint32 ipaddr=SDL_SwapBE32(oAddress.host);
		MortalNetworkMessage("Connecting to %d.%d.%d.%d port %d",
			ipaddr>>24, (ipaddr>>16)&0xff, (ipaddr>>8)&0xff, ipaddr&0xff, MORTALNETWORKPORT );
	}
	
	if ( !a_pcServerName )
	{
		// SERVER-MODE CONNECTION
		
		m_poSocket = SDLNet_TCP_Open( &oAddress );
		if ( NULL == m_poSocket ) RETURNWITHERROR;
		
		// Wait for connection ...
		MortalNetworkMessage ( Translate("Waiting for connection... (press any key to abort)") );
		MortalNetworkMessage( Translate("You must have port 14882 open for this to work.") );
		TCPsocket poClient;
		while ( 1 )
		{
			poClient = SDLNet_TCP_Accept( m_poSocket );
			if ( poClient ) break;
			
			if (MortalNetworkCheckKey()) break;;
			SDL_Delay( 100 );
		}
		
		SDLNet_TCP_Close( m_poSocket );
		
		if ( NULL == poClient )
		{
			m_sLastError = "No connection.";
			return false;
		}
		
		IPaddress* poRemoteAddress = SDLNet_TCP_GetPeerAddress(poClient);

		if ( !poRemoteAddress )
		{
			RETURNWITHERROR;
		}
		Uint32 ipaddr=SDL_SwapBE32(poRemoteAddress->host);
		MortalNetworkMessage("Accepted connection from %d.%d.%d.%d port %d",
			ipaddr>>24, (ipaddr>>16)&0xff, (ipaddr>>8)&0xff, ipaddr&0xff, MORTALNETWORKPORT);

		// Set the client socket as our socket, and drop the server socket.

		m_poSocket = poClient;
	}
	else
	{
		// CLIENT-MODE CONNECTION
		
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
	}

	// CONNECTION ESTABLISHED. SEND INTRO PACKETS

	MortalNetworkMessage( Translate("Connection established.") );
	
	struct SIntroPackage
	{
		char	cID;
		char	acVersion[10];
	} oIntroPackage, oRemotePackage;

	oIntroPackage.cID = 'I';
	strncpy( oIntroPackage.acVersion, VERSION, 10 );
	oIntroPackage.acVersion[9] = 0;

	debug( "Sending intro package... " );

	int iRetval = SDLNet_TCP_Send( m_poSocket, &oIntroPackage, sizeof( oIntroPackage ) );

	if ( iRetval < (int) sizeof( oIntroPackage ) )
	{
		RETURNWITHERROR;
	}

	iRetval = SDLNet_TCP_Recv( m_poSocket, &oRemotePackage, sizeof( oRemotePackage ) );
	if ( iRetval <= 0 )
	{
		RETURNWITHERROR;
	}
	if ( iRetval < (int) sizeof( oRemotePackage )
		|| oRemotePackage.cID != 'I'
		|| strncmp( oRemotePackage.acVersion, VERSION, 9 ) )
	{
		m_sLastError = Translate( "The remote side has a different version of OpenMortal running." );
		RETURNNOERROR;
	}
	
	MortalNetworkMessage( Translate("Life is good.") );
	
	m_enState = NS_CHARACTER_SELECTION;
	m_bServer = NULL == a_pcServerName;
	m_bMaster = m_bServer;
	m_sLastError = "";
	m_asMsgs.clear();
	m_enRemoteFighter = UNKNOWN;
	m_bRemoteReady = false;
	m_sLatestGameData = "";
	m_aiKeystrokes.clear();
	m_aiKeystrokes.clear();
	m_abKeystrokes.clear();
	m_iGameTick = 0;
	m_iHurryupCode = 0;
	m_iIncomingBufferSize = 0;
	m_aiAvailableRemoteFighters.clear();
	m_oGameParams.iGameTime = m_oGameParams.iGameSpeed = m_oGameParams.iHitPoints = m_oGameParams.iBackgroundNumber = 0;
	m_sRemoteUserName = "HIM";
	
	m_poSocketSet = SDLNet_AllocSocketSet( 1 );
	SDLNet_TCP_AddSocket( m_poSocketSet, m_poSocket );	// Check for errors?

	SendRawData( 'U', g_oState.m_acNick, strlen(g_oState.m_acNick)+1 );
	
	return true;
}


void CMortalNetworkImpl::Stop()
{
	if ( NS_DISCONNECTED == m_enState )
	{
		return;
	}

	g_oState.m_enGameMode = SState::IN_DEMO;
	m_enState = NS_DISCONNECTED;
	SDLNet_FreeSocketSet( m_poSocketSet );
	SDLNet_TCP_Close( m_poSocket );
}


bool CMortalNetworkImpl::IsConnectionAlive()
{
	return ( NS_DISCONNECTED != m_enState );
}



#define DISCONNECTONCOMMUNICATIONERROR { 									\
		m_sLastError = Translate("Communication error. Disconnecting.");	\
		Stop();																\
		return; }
#define DISCONNECTWITH(A) { 												\
		m_sLastError = Translate("Communication error. Disconnecting.");	\
		Stop();																\
		return(A); }

#define CHECKCONNECTION if ( NS_DISCONNECTED == m_enState ) return;


void CMortalNetworkImpl::Update()
{
	CHECKCONNECTION;

	// 1. CHECK FOR STUFF TO READ
	
	int iRetval = SDLNet_CheckSockets( m_poSocketSet, 0 );
	if ( iRetval <= 0 )
	{
		return;
	}

	// 2. APPEND AT MOST 1024 bytes TO THE END OF THE INCOMING BUFFER
	
	iRetval = SDLNet_TCP_Recv( m_poSocket, m_acIncomingBuffer + m_iIncomingBufferSize, 1024 );
	if ( iRetval <= 0 )
	{
		m_sLastError = SDLNet_GetError();
		Stop();
		return;
	}
	m_iIncomingBufferSize += iRetval;

	// 3. CONSUME THE INCOMING BUFFER.
	// We always make sure the incoming buffer starts with a package header.

	int iOffset = 0;

	while ( iOffset < m_iIncomingBufferSize )
	{
		// 3.1. Check if we have enough data to receive the package.
		
		if ( m_iIncomingBufferSize - iOffset < 4 )
		{
			// Not enough space left for a full header.
			debug( "Not enough space left for a full header (%d).\n", m_iIncomingBufferSize-iOffset );
			break;
		}
		
		unsigned int iLengthOfPackage = SDL_SwapBE16(*((Uint16*)(m_acIncomingBuffer + iOffset + 1)));
		if ( iLengthOfPackage > 1000 )
		{
			debug( "Maximum package size exceeded.\n" );
			DISCONNECTONCOMMUNICATIONERROR;
		}
//		debug( "Receiving stuff.. %c type, %d package length, offset %d in buffer, %d bytes in buffer\n",
//			m_acIncomingBuffer[iOffset], iLengthOfPackage, iOffset, m_iIncomingBufferSize );
		
		if ( iOffset + 4 + (int)iLengthOfPackage > m_iIncomingBufferSize )
		{
			// Not enough space left for the actual package.
			debug( "Not enough space left for the actual package.\n" );
			break;
		}

		// 3.2. Receive the data.
		
		switch ( m_acIncomingBuffer[iOffset] )
		{
			case 'M': ReceiveMsg( m_acIncomingBuffer+iOffset+4, iLengthOfPackage ); break;
			case 'F': ReceiveFighter( m_acIncomingBuffer+iOffset+4, iLengthOfPackage ); break;
			case 'R': ReceiveReady( m_acIncomingBuffer+iOffset+4, iLengthOfPackage ); break;
			case 'S': m_bSynchQueryResponse=true; break;
			case 'G': ReceiveGameData( m_acIncomingBuffer+iOffset+4, iLengthOfPackage ); break;
			case 'K': ReceiveKeystroke( m_acIncomingBuffer+iOffset+4, iLengthOfPackage ); break;
			case 'O': ReceiveRoundOver( m_acIncomingBuffer+iOffset+4, iLengthOfPackage ); break;
			case 'T': ReceiveGameTick( m_acIncomingBuffer+iOffset+4, iLengthOfPackage ); break;
			case 'H': ReceiveHurryup( m_acIncomingBuffer+iOffset+4, iLengthOfPackage ); break;
			case 'A': ReceiveRemoteFighterAvailable( m_acIncomingBuffer+iOffset+4, iLengthOfPackage ); break;
			case 'Q': ReceiveRemoteFighterQuery( m_acIncomingBuffer+iOffset+4, iLengthOfPackage ); break;
			case 'P': ReceiveGameParams( m_acIncomingBuffer+iOffset+4, iLengthOfPackage ); break;
			case 'U': ReceiveRemoteUserName( m_acIncomingBuffer+iOffset+4, iLengthOfPackage ); break;
			default:
			{
				debug( "Bad ID: %c (%d)\n", m_acIncomingBuffer[iOffset], m_acIncomingBuffer[iOffset] );
				DISCONNECTONCOMMUNICATIONERROR;
			}
		}

		if ( !IsConnectionAlive() )
		{
			return;
		}

		iOffset += iLengthOfPackage + 4;
	}

	// 4. MOVE LEFTOVER DATA TO THE BEGINNING OF THE INCOMING BUFFER
	// The leftover data starts at iOffset, and is (m_iIncomingBufferSize-iOffset) long.
	
	memmove( m_acIncomingBuffer, m_acIncomingBuffer + iOffset, m_iIncomingBufferSize-iOffset );
	m_iIncomingBufferSize -= iOffset;
}


const char* CMortalNetworkImpl::GetLastError()
{
	return m_sLastError.c_str();
}


bool CMortalNetworkImpl::IsMaster()
{
	return m_bMaster;
}



void CMortalNetworkImpl::ReceiveRemoteUserName( void* a_pData, int a_iLength )
{
	if ( a_iLength < 1 || a_iLength > MAXSTRINGLENGTH ) DISCONNECTONCOMMUNICATIONERROR;

	char* acData = (char*) a_pData;
	acData[ a_iLength-1 ] = 0;	// Last char should be 0, just making sure..

	m_sRemoteUserName = acData;
}

const char* CMortalNetworkImpl::GetRemoteUsername()
{
	return m_sRemoteUserName.c_str();
}



/*************************************************************************
							MSG RELATED METHODS
*************************************************************************/



/** All sent data must go through this method. It ensures the well-formed
header for the data.
The header itself looks like this:
ID			char
Length		Uint16
Reserved	char
This is followed by as many bytes as the Length is.
*/
void CMortalNetworkImpl::SendRawData( char a_cID, const void* a_pData, int a_iLength )
{
	CHECKCONNECTION;

	int iPacketLength = a_iLength + 4;
	
	char *pcBuffer = new char[iPacketLength];
	pcBuffer[0] = a_cID;
	*((Uint16*)(pcBuffer+1)) = SDL_SwapBE16( a_iLength );
	pcBuffer[3] = 0;

	if ( a_iLength > 0 )
	{
		memcpy( pcBuffer+4, a_pData, a_iLength );
	}
	
	int iRetval = SDLNet_TCP_Send( m_poSocket, pcBuffer, iPacketLength );
	if ( iRetval != iPacketLength ) DISCONNECTONCOMMUNICATIONERROR;

	delete [] pcBuffer;
}







void CMortalNetworkImpl::SendMsg( const char* a_pcMsg )
{
	CHECKCONNECTION;
	
	int iMsgLen = strlen( a_pcMsg ) + 1;
	if ( iMsgLen > MAXSTRINGLENGTH )
	{
		// Will not be 0 terminated if exceeds length!
		iMsgLen = MAXSTRINGLENGTH;
	}
	SendRawData( 'M', a_pcMsg, iMsgLen );
}

void CMortalNetworkImpl::ReceiveMsg( void* a_pData, int a_iLength )
{
	if ( a_iLength < 1 || a_iLength > MAXSTRINGLENGTH ) DISCONNECTONCOMMUNICATIONERROR;
	
	char* pcData = (char*) a_pData;
	pcData[ a_iLength-1 ] = 0;	// Last char should be 0, just making sure..

	std::string sMsg = "<" + m_sRemoteUserName + "> " + pcData;
	
	m_asMsgs.push_back( sMsg );
}


bool CMortalNetworkImpl::IsMsgAvailable()
{
	return m_asMsgs.size() > 0;
}

const char* CMortalNetworkImpl::GetMsg()
{
	static std::string sLastMsg;
	if ( IsMsgAvailable() )
	{
		sLastMsg = m_asMsgs.front();
		m_asMsgs.pop_front();
		return sLastMsg.c_str();
	}
	return NULL;
}




/*************************************************************************
				CHARACTER SELECTION RELATED METHODS
*************************************************************************/


/** Unfortunately not all STL are created equal... Some do not have the
find algorithm. So, here is our feeble implementation... */
template<class InputIterator, class EqualityComparable>
InputIterator MszFind(InputIterator first, InputIterator last, const EqualityComparable& value )
{
	while ( first != last
		&& *first != value )
	{
		++first;
	}
	return first;
}



bool CMortalNetworkImpl::IsRemoteFighterAvailable( FighterEnum a_enFighter )
{
	if ( 0 == a_enFighter )
	{
		return false;
	}
	
	// Check if we already have it cached.
	if ( MszFind( m_aiAvailableRemoteFighters.begin(), m_aiAvailableRemoteFighters.end(), a_enFighter ) != m_aiAvailableRemoteFighters.end() )
	{
		return true;
	}
	if ( MszFind( m_aiAvailableRemoteFighters.begin(), m_aiAvailableRemoteFighters.end(), -a_enFighter ) != m_aiAvailableRemoteFighters.end() )
	{
		return false;
	}
	
	Uint32 iFighter = SDL_SwapBE32( a_enFighter );
	SendRawData( 'Q', &iFighter, sizeof(Uint32) );

	for ( int i=0; i<100; ++i )
	{
		SDL_Delay(10);
		Update();
		if ( MszFind( m_aiAvailableRemoteFighters.begin(), m_aiAvailableRemoteFighters.end(), a_enFighter ) != m_aiAvailableRemoteFighters.end() )
		{
			return true;
		}
		if ( MszFind( m_aiAvailableRemoteFighters.begin(), m_aiAvailableRemoteFighters.end(), -a_enFighter ) != m_aiAvailableRemoteFighters.end() )
		{
			return false;
		}
	}

	m_aiAvailableRemoteFighters.push_front( -a_enFighter );
	return false;
}

void CMortalNetworkImpl::ReceiveRemoteFighterQuery( void* a_pData, int a_iLength )
{
	if ( a_iLength != sizeof(Uint32) ) DISCONNECTONCOMMUNICATIONERROR;
	FighterEnum iFighter = (FighterEnum) SDL_SwapBE32( *((Uint32*)a_pData) );

	bool bAvailable = g_oPlayerSelect.IsLocalFighterAvailable( iFighter );
	Uint32 iResponse = bAvailable ? iFighter : iFighter + 100000;
	debug( "ReceiveRemoteFighterQuery: %d -> %d\n", iFighter, iResponse );
	iResponse = SDL_SwapBE32( iResponse );

	SendRawData( 'A', &iResponse, sizeof(Uint32) );
}

void CMortalNetworkImpl::ReceiveRemoteFighterAvailable( void* a_pData, int a_iLength )
{
	if ( a_iLength != sizeof(Uint32) ) DISCONNECTONCOMMUNICATIONERROR;
	Uint32 iFighter = SDL_SwapBE32( *((Uint32*)a_pData) );
	debug( "ReceiveRemoteFighterAvailable: %d\n", iFighter );
	if ( iFighter >= 100000 )
	{
		m_aiAvailableRemoteFighters.push_front( -(iFighter-100000) );
	}
	else
	{
		m_aiAvailableRemoteFighters.push_front( iFighter );
	}
}






void CMortalNetworkImpl::SendFighter( FighterEnum a_enFighter )
{
	CHECKCONNECTION;

	Uint32 iFighter = SDL_SwapBE32( a_enFighter );
	SendRawData( 'F', &iFighter, sizeof (iFighter) );
}

void CMortalNetworkImpl::ReceiveFighter( void* a_pcData, int a_iLength )
{
	Uint32 iFighter;
	if ( a_iLength != sizeof(iFighter) ) DISCONNECTONCOMMUNICATIONERROR;
	iFighter = *((Uint32*)a_pcData);
	
	m_enRemoteFighter = (FighterEnum) SDL_SwapBE32( iFighter );
	debug( "ReceiveFighter: %d\n", m_enRemoteFighter );
}

FighterEnum CMortalNetworkImpl::GetRemoteFighter()
{
	return m_enRemoteFighter;
}




void CMortalNetworkImpl::SendReady()
{
	CHECKCONNECTION;

	SendRawData( 'R', NULL, 0 );
}

void CMortalNetworkImpl::ReceiveReady( void* a_pData, int a_iLength )
{
	if ( a_iLength != 0 ) DISCONNECTONCOMMUNICATIONERROR;
	m_bRemoteReady = true;
}

bool CMortalNetworkImpl::IsRemoteSideReady()
{
	return m_bRemoteReady;
}



void CMortalNetworkImpl::SendGameParams( int a_iGameSpeed, int a_iGameTime, int a_iHitPoints, int a_iBackgroundNumber )
{
	CHECKCONNECTION;
	if ( (int)m_oGameParams.iGameSpeed == a_iGameSpeed
		&& (int)m_oGameParams.iGameTime == a_iGameTime
		&& (int)m_oGameParams.iHitPoints == a_iHitPoints
		&& (int)m_oGameParams.iBackgroundNumber == a_iBackgroundNumber )
	{
		// Nothing to update.
		return;
	}
	
	m_oGameParams.iGameSpeed = a_iGameSpeed;
	m_oGameParams.iGameTime = a_iGameTime;
	m_oGameParams.iHitPoints = a_iHitPoints;
	m_oGameParams.iBackgroundNumber = a_iBackgroundNumber;

	SGameParams oPackage;
	oPackage.iGameSpeed = SDL_SwapBE32( a_iGameSpeed );
	oPackage.iGameTime = SDL_SwapBE32( a_iGameTime );
	oPackage.iHitPoints = SDL_SwapBE32( a_iHitPoints );
	oPackage.iBackgroundNumber = SDL_SwapBE32( a_iBackgroundNumber );
	SendRawData( 'P', &oPackage, sizeof(SGameParams) );
}

void CMortalNetworkImpl::ReceiveGameParams( void* a_pData, int a_iLength )
{
	if ( a_iLength != sizeof(SGameParams) ) DISCONNECTONCOMMUNICATIONERROR;

	SGameParams* poPackage = (SGameParams*) a_pData;
	if ( m_oGameParams.iGameSpeed != SDL_SwapBE32( poPackage->iGameSpeed ) )
	{
		m_oGameParams.iGameSpeed = SDL_SwapBE32( poPackage->iGameSpeed );
		m_asMsgs.push_back( std::string("*** ") + GetGameSpeedString( m_oGameParams.iGameSpeed ) );
	}
	if ( m_oGameParams.iGameTime != SDL_SwapBE32( poPackage->iGameTime ) )
	{
		m_oGameParams.iGameTime = SDL_SwapBE32( poPackage->iGameTime );
		m_asMsgs.push_back( std::string("*** ") + GetGameTimeString( m_oGameParams.iGameTime) );
	}
	if ( m_oGameParams.iHitPoints != SDL_SwapBE32( poPackage->iHitPoints ) )
	{
		m_oGameParams.iHitPoints = SDL_SwapBE32( poPackage->iHitPoints );
		m_asMsgs.push_back( std::string("*** ") + GetHitPointsString( m_oGameParams.iHitPoints ) );
	}

	m_oGameParams.iBackgroundNumber = SDL_SwapBE32( poPackage->iBackgroundNumber );
}

CMortalNetworkImpl::SGameParams CMortalNetworkImpl::GetGameParams()
{
	return m_oGameParams;
}



/*************************************************************************
						GAME RELATED METHODS
*************************************************************************/





bool CMortalNetworkImpl::SynchStartRound()
{
	debug( "SynchStartRound STARTED.\n" );
	
	m_bSynchQueryResponse = false;
	
	// run until both sides manage to get a SYNCH

	int i=0;

	while ( !m_bSynchQueryResponse )
	{
		SendRawData('S', NULL, 0);
		if ( !IsConnectionAlive() ) break;
		Update();
		SDL_Delay(100);
		if ( !IsConnectionAlive() ) break;
		++i;

		if ( i == 10 )
		{
			debug( "Synch is slow...\n" );
			MortalNetworkResetMessages( true );
			MortalNetworkMessage( "Synching with remote side..." );
			MortalNetworkMessage( "Press any key to disconnect." );
		}
		if ( i > 10
			&& MortalNetworkCheckKey() )
		{
			Stop();
			return false;
		}
	}

	if ( IsConnectionAlive() )
	{
		m_enState = NS_IN_GAME;
		m_bRoundOver = false;
		m_bGameOver = false;
		m_bRemoteReady = false;
		m_bSynchQueryResponse = false;
		m_iHurryupCode = -1;
	}

	return IsConnectionAlive();

	debug( "SynchStartRound FINISHED.\n" );
}










void CMortalNetworkImpl::SendGameData( const char* a_pcGameData )
{
	CHECKCONNECTION;
	
	int iMsgLen = strlen( a_pcGameData ) + 1;
	if ( iMsgLen > MAXSTRINGLENGTH )
	{
		// Will not be 0 terminated if exceeds length!
		iMsgLen = MAXSTRINGLENGTH;
	}
	SendRawData( 'G', a_pcGameData, iMsgLen );
}

void CMortalNetworkImpl::ReceiveGameData( void* a_pData, int a_iLength )
{
	if ( a_iLength < 1 || a_iLength > MAXSTRINGLENGTH ) DISCONNECTONCOMMUNICATIONERROR;

	char* pcData = (char*) a_pData;
	pcData[ a_iLength-1 ] = 0;	// Last char should be 0, just making sure..

	if ( pcData[0] )
	{
		m_sLatestGameData = pcData;
	}
}

const char* CMortalNetworkImpl::GetLatestGameData()
{
	return m_sLatestGameData.c_str();
}




struct SKeystrokePackage
{
	Uint32	iTime;
	char	cKey;
	bool	bPressed;
};

void CMortalNetworkImpl::SendKeystroke( int a_iTime, int a_iKey, bool a_bPressed )
{
	CHECKCONNECTION;
	
	SKeystrokePackage oPackage;
	oPackage.iTime = SDL_SwapBE32( a_iTime );
	oPackage.cKey = a_iKey;
	oPackage.bPressed = a_bPressed;
	
	SendRawData( 'K', &oPackage, sizeof( oPackage) );
}

void CMortalNetworkImpl::ReceiveKeystroke( void* a_pData, int a_iLength )
{
	if ( a_iLength != (int)sizeof(SKeystrokePackage) ) DISCONNECTONCOMMUNICATIONERROR;
	SKeystrokePackage* poPackage = (SKeystrokePackage*) a_pData;

	m_aiKeyTimes.push_back( SDL_SwapBE32(poPackage->iTime) );
	m_aiKeystrokes.push_back( poPackage->cKey );
	m_abKeystrokes.push_back( poPackage->bPressed );
}

bool CMortalNetworkImpl::GetKeystroke( int& a_riOutTime, int& a_riOutKey, bool& a_rbOutPressed )
{
	if ( m_aiKeystrokes.size() == 0 )
	{
		return false;
	}
	a_riOutTime = m_aiKeyTimes.front();
	a_riOutKey = m_aiKeystrokes.front();
	a_rbOutPressed = m_abKeystrokes.front();
	m_aiKeyTimes.pop_front();
	m_aiKeystrokes.pop_front();
	m_abKeystrokes.pop_front();
	
	// debug( "GetKeystroke: %d, %d\n", a_riOutKey, a_rbOutPressed );
	return true;
}



struct SGameTickPackage
{
	Uint32 iGameTick;
};

void CMortalNetworkImpl::SendGameTick( int a_iGameTick )
{
	CHECKCONNECTION;

	if ( a_iGameTick < 0 ) a_iGameTick = 0;
	
	SGameTickPackage oPackage;
	oPackage.iGameTick  = SDL_SwapBE32( a_iGameTick );
	
	SendRawData( 'T', &oPackage, sizeof(SGameTickPackage) );
}

void CMortalNetworkImpl::ReceiveGameTick( void* a_pData, int a_iLength )
{
	if ( a_iLength != sizeof(SGameTickPackage) ) DISCONNECTONCOMMUNICATIONERROR;
	SGameTickPackage* poPackage = (SGameTickPackage*) a_pData;

	m_iGameTick  = SDL_SwapBE32( poPackage->iGameTick );
}

int CMortalNetworkImpl::GetGameTick()
{
	return m_iGameTick;
}





void CMortalNetworkImpl::SendHurryup( int a_iHurryUpCode )
{
	CHECKCONNECTION;
	int iPackage = SDL_SwapBE32( a_iHurryUpCode );
	SendRawData( 'H', &iPackage, sizeof(int) );
}

void CMortalNetworkImpl::ReceiveHurryup( void* a_pData, int a_iLength )
{
	if ( a_iLength != sizeof(int) ) DISCONNECTONCOMMUNICATIONERROR;
	m_iHurryupCode = SDL_SwapBE32( *((int*)a_pData) );
}

int CMortalNetworkImpl::GetHurryup()
{
	int iRetval = m_iHurryupCode;
	m_iHurryupCode = -1;
	return iRetval;
}




struct SRoundOrder
{
	int iWhoWon;
	bool bGameOver;
};

void CMortalNetworkImpl::SendRoundOver( int a_iWhoWon, bool a_bGameOver )
{
	CHECKCONNECTION;
	SRoundOrder oPackage;

	oPackage.iWhoWon = a_iWhoWon;
	oPackage.bGameOver = a_bGameOver;
	SendRawData( 'O', &oPackage, sizeof(SRoundOrder) );

	if ( a_bGameOver )
	{
		m_enState = NS_CHARACTER_SELECTION;
	}

	debug ( "SendRoundOver: %d, %d\n", a_iWhoWon, a_bGameOver );
}

void CMortalNetworkImpl::ReceiveRoundOver( void* a_pData, int a_iLength )
{
	if ( a_iLength != sizeof(SRoundOrder) ) DISCONNECTONCOMMUNICATIONERROR;
	SRoundOrder* poPackage = (SRoundOrder*) a_pData;
	m_iWhoWon = poPackage->iWhoWon;
	m_bGameOver = poPackage->bGameOver;
	m_bRoundOver = true;
	
	debug ( "ReceiveRoundOver: %d, %d\n", m_iWhoWon, m_bGameOver );
}

bool CMortalNetworkImpl::IsRoundOver()
{
	return m_bRoundOver;
}

bool CMortalNetworkImpl::IsGameOver()
{
	return m_bGameOver;
}

int CMortalNetworkImpl::GetWhoWon()
{
	return m_iWhoWon;
}



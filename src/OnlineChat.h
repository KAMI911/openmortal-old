/***************************************************************************
                          OnlineChat.h  -  description
                             -------------------
    begin                : Thu Jan 29 2004
    copyright            : (C) 2004 by upi
    email                : upi@feel
 ***************************************************************************/

#ifndef ONLINECHAT_H
#define ONLINECHAT_H

/** 
\class COnlineChat
\ingroup Network
The "mortal.net" chat protocoll should be simple enough...

[LOGGING IN]

Server: <greeting msg>
Client: N <version> <nickname>
Server: OK <actual nickname>


[CHATTING]

Server: J <user> <IP>		- User joins.
Server: L <user> 			- User leaves.
Server: N <user> <newnick>	- Nick change.
Server: W <user> <IP>		- Whois response
Server: Y <num>				- You are now known as...
Server: C <user>			- Someone has challenged you!

Server: M <user> <text>		- Public message.
Server: S <text>			- Server message.

Client: M <text>			- Send message.
Client: W <user>			- WHOIS query
Client: L					- Quit
Client: C <user>			- Challenge a user
*/


#include "SDL_net.h"
#include <string>
#include <map>


class CReadline;
class CTextArea;
struct SDL_Surface;

typedef std::map<std::string,std::string> TNickMap;


class COnlineChat
{
public:
	COnlineChat();
	~COnlineChat();

	bool					Start();
	void					Stop();
	void					DoOnlineChat();

protected:
	void					Redraw();
	void					Update();				// Read network traffic. Might get disconnected...
	void					SendRawData( char a_cID, const std::string& a_rsData );

	void					ReceiveMsg( char a_cID, char* a_pcData );
	void					ReceiveUser( char a_cID, char* a_pcData );

	void					DrawNickList();
	void					Menu();


protected:
	TCPsocket				m_poSocket;					///< The TCP/IP network socket.
	SDLNet_SocketSet		m_poSocketSet;				///< SDLNet construct for watching the socket.
	
	char					m_acIncomingBuffer[4096];	///< Received data goes here.
	int						m_iIncomingBufferSize;		///< How much of the buffer is filled?
	
	std::string				m_sLastError;				///< The last error message from SDLNet

	SDL_Surface*			m_poScreen;
	SDL_Surface*			m_poBackground;
	CReadline*				m_poReadline;
	CTextArea*				m_poTextArea;
	
	bool					m_bMyNickIsOk;
	TNickMap				m_asNicks;
};

#endif // ONLINECHAT_H

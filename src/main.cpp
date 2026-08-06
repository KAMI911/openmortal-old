/***************************************************************************
                          main.cpp  -  description
                             -------------------
    begin                : Wed Aug 22 10:18:47 CEST 2001
    copyright            : (C) 2001 by upi
    email                : upi@apocalypse.rulez.org
 ***************************************************************************/


/**
\mainpage

The document you read now describes the design of OpenMortal. This page 
serves as a starting point. The documentation is generated with doxygen
(http://doxygen.org).

OpenMortal consists of two main parts: the \b frontend and the \b backend.

\li The frontend is a C++ program, responsible for multimedia 
(sounds, music, graphics) and general interaction with the players 
(menus, keyboard input), and the demo and intro screens.

\li The backend is written in Perl, and is incorporated into the C++ 
program with Perl embedding.


 
\section s1 1. Modules

The classes of OpenMortal are organized into the following groups (see Modules above).

\li \ref Media - OpenMortal uses \b SDL (http://libsdl.org) for hardware 
access such as screen drawing, music, sound effect and keyboard input. For 
information and documentation of SDL, SDL_image, SDL_ttf and SDL_mixer, 
please look at the SDL homepage.
\li \ref PlayerSelect
\li \ref Network
\li \ref Demo
\li \ref GameLogic



\section s2 2. Main Functions
These global functions implement important parts of the program. They serve
as entry points into functional parts.

\li DoMenu() - Displays and runs the menu over the current screen
\li GameOver() - Displays the "Final Judgeent" screen
\li DoDemos() - Runs the demos in an endless loop until a game is started or the program ends.
\li DoGame() - Runs the game.
\li DoOnlineChat() - Connects to and runs the MortalNet.
\li CPlayerSelect::DoPlayerSelect() - Runs the player selection screen.


\section s3 3. Definitions

Here are the definitions of terms used in this documentation.

<dl>

<dt>\b Player <dd>
	Player refers to one of the two persons playing the game. A player chooses a fighter. The two players are referred to as "Player 1" and "Player 2", even though the C++ and perl code count arrays from 0.
<dt>\b Fighter <dd>
	Fighter is one of the many available characters. Usually there are only two fighters loaded at any time. Fighters are static: their properties never change. Maybe Fighter should be renamed to Character?
<dt>\b Game <dd>
	One game is the part of the program in which the players actually compete. The game consists of a number of rounds. The player selection, and gameover screen are not part of this definition.
<dt>\b Round <dd>
	One round starts with both players at full health, and ends either with a timeout or with a ko.
<dt>\b Doodad <dd>
	A graphical element on the game screen that is not the background or the characters. E.g. the "3x combo" text or a thrown projectile are doodads.
<dt>\b Tint <dd>
	A tint is a methodical change in a palette. There are many ways to tint (e.g. grayscaling, darkening, green chroma, etc). Usually when two players choose the same fighter, the fighter of player 2 is tinted.
<dt>\b Scene <dd>
	The description of a frozen moment in the course of a game. The Backend is responsible for calculating each consecutive scene. The number of scenes calculated per second is constant (except for the "hurry up mode", or if the computer is too slow).
<dt>\b FPS <dd>
	Frames Per Second. The FPS indicator on the screen during a game indicates the number of scenes drawn, not the number of scenes calculated by the backend.
</dl>


\section s4 4. C++ Coding conventions

Historically two different coding conventions were mixed in OpenMortal. 
Hopefully most of the old conventions are eliminated by now.
Here I will describe the new conventions:

\li <B> Class names: </B> CMixedCaps.
\li <B> Struct names: </B> SMixedCaps.
\li <B> Typedef names: </B> CSomeTypedef (There's some traces of the old TSomeTypedef 
    left, these should be eliminated.
\li <B> Enum names: </B> SomeThingEnum.
\li <B> Enum values: </B> Prefix_ENUM_VALUE
\li <B> Method names: </B> MixedCaps.
\li <B> Variable names: </B> &lt;prefix&gt;VariableName.
\li <B> Instance property names: </B> m_&lt;prefix&gt;VariableName,
\li <B> Class property names </B>(a.k.a. static class variables): mg_&lt;prefix&gt;VariableName.
\li <B> Method argument names: </B> a_&lt;prefix&gt;VariableName. 
If a reference or pointer argument is "output-only: a_&lt;prefix&gt;OutVariableName.
\li <B> Global variable names: </B> g_&lt;prefix&gt;VariableName.

The prefixes used are:

\li <B> Array of something: </B> a&lt;something&gt;
\li <B> Pointer to something: </B> p&lt;something&gt;
\li <B> Reference of something: </B> r&lt;something&gt;
\li <B> Basic types: </B> Integer: i; char: c; double: d; enum: en; object (class or struct): o; std::string: s;

Example:
\code
CSomeExampleClass: public CSomeBaseClass
{
public:
	CSomeExampleClass();
	void SomeMethod( int& a_riOutSomething );
protected:
	int m_iSomething;
	char* m_pcSomethingElse;
	static enum SomeThingEnum
	{
		Ste_VALUE,
	} mg_enWhatever;
};
\endcode
*/


#include "config.h"

#include "PlayerSelect.h"

#include "SDL_video.h"
#include "sge_tt_text.h"
#include "sge_bm_text.h"
#include "sge_surface.h"
#include "SDL.h"
#include "SDL_image.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <string>

#include "common.h"
#include "gfx.h"
#include "Audio.h"
#include "RlePack.h"
#include "Backend.h"
#include "State.h"
#include "FighterStats.h"
#include "MortalNetwork.h"


#if defined(_WIN32) || defined(WIN32) || defined(_WINDOWS)
#undef DATADIR                // GRRR.. windows keyword...
#include <windows.h>
#define DATADIR "../data"

#endif

_sge_TTFont* inkFont;
_sge_TTFont* impactFont;
_sge_TTFont* titleFont;
_sge_TTFont* chatFont;
sge_bmpFont* fastFont;
sge_bmpFont* creditsFont;
sge_bmpFont* storyFont;
bool bDebug = false;

Uint32 C_BLACK, C_BLUE, C_GREEN, C_CYAN, C_RED, C_MAGENTA, C_ORANGE, C_LIGHTGRAY,
	C_DARKGRAY, C_LIGHTBLUE, C_LIGHTGREEN, C_LIGHTCYAN, C_LIGHTRED, C_LIGHTMAGENTA, C_YELLOW, C_WHITE;

SDL_Color Colors[] =
{
   { 0,  0,  0,  0 },   { 0,  0,  42, 0 },   { 0,  42, 0,  0 },   { 0,  42, 42, 0 },
   { 42, 0,  0,  0 },   { 42, 0,  42, 0 },   { 63, 42, 0,  0 },   { 42, 42, 42, 0 },
   { 21, 21, 21, 0 },   { 21, 21, 63, 0 },   { 21, 63, 21, 0 },   { 21, 63, 63, 0 },
   { 63, 21, 21, 0 },   { 63, 21, 63, 0 },   { 63, 63, 21, 0 },   { 63, 63, 63, 0 }
};


void Complain( const char* a_pcError )
{
#if defined(_WIN32) || defined(WIN32) || defined(_WINDOWS)
	::MessageBoxA( 0, a_pcError, "OpenMortal", MB_ICONEXCLAMATION );
#else
	fprintf( stderr, "%s", a_pcError );
#endif
}


_sge_TTFont* LoadTTF( const char* a_pcFilename, int a_iSize )
{
	std::string sPath = std::string(DATADIR) + "/fonts/" + a_pcFilename;
	_sge_TTFont* poFont = sge_TTF_OpenFont( sPath.c_str(), a_iSize );
	
	if ( NULL == poFont )
	{
		Complain( ("Couldn't load font: " + sPath).c_str() );
	}
	
	return poFont;
}


sge_bmpFont* LoadBMPFont( const char* a_pcFilename )
{
	std::string sPath = std::string(DATADIR) + "/fonts/" + a_pcFilename;
	sge_bmpFont* poFont = sge_BF_OpenFont( sPath.c_str(), SGE_BFSFONT | SGE_BFTRANSP );
	if ( NULL == poFont )
	{
		Complain( ("Couldn't load font: " + sPath).c_str() );
	}
	return poFont;
}


int init()
{
	if (SDL_Init(SDL_INIT_VIDEO /*| SDL_INIT_AUDIO*/) < 0)
	{
		fprintf(stderr, "Failed to initialize SDL: %s\n", SDL_GetError());
		return -1;
	}
	atexit(SDL_Quit);
	
	SetVideoMode( false, g_oState.m_bFullscreen );
	if (gamescreen == NULL)
	{
		fprintf(stderr, "failed to set video mode: %s\n", SDL_GetError());
		return -1;
	}
	
	SDL_WM_SetCaption( "OpenMortal", "OpenMortal" );
	std::string sPath = std::string(DATADIR) + "/gfx/icon.png";
	SDL_WM_SetIcon(IMG_Load(sPath.c_str()), NULL);
	SDL_ShowCursor( SDL_DISABLE );

	int i;
	for ( i=0; i<16; ++i ) 
	{ 
		Colors[i].r *=4; Colors[i].g *=4; Colors[i].b *=4;
	}
	if ( gamescreen->format->BitsPerPixel > 8 )
	{
		i = 0;
		C_BLACK			= SDL_MapRGB( gamescreen->format, Colors[i].r, Colors[i].g, Colors[i].b ); ++i;
		C_BLUE			= SDL_MapRGB( gamescreen->format, Colors[i].r, Colors[i].g, Colors[i].b ); ++i;
		C_GREEN			= SDL_MapRGB( gamescreen->format, Colors[i].r, Colors[i].g, Colors[i].b ); ++i;
		C_CYAN			= SDL_MapRGB( gamescreen->format, Colors[i].r, Colors[i].g, Colors[i].b ); ++i;

		C_RED			= SDL_MapRGB( gamescreen->format, Colors[i].r, Colors[i].g, Colors[i].b ); ++i;
		C_MAGENTA		= SDL_MapRGB( gamescreen->format, Colors[i].r, Colors[i].g, Colors[i].b ); ++i;
		C_ORANGE		= SDL_MapRGB( gamescreen->format, Colors[i].r, Colors[i].g, Colors[i].b ); ++i;
		C_LIGHTGRAY		= SDL_MapRGB( gamescreen->format, Colors[i].r, Colors[i].g, Colors[i].b ); ++i;

		C_DARKGRAY		= SDL_MapRGB( gamescreen->format, Colors[i].r, Colors[i].g, Colors[i].b ); ++i;
		C_LIGHTBLUE		= SDL_MapRGB( gamescreen->format, Colors[i].r, Colors[i].g, Colors[i].b ); ++i;
		C_LIGHTGREEN	= SDL_MapRGB( gamescreen->format, Colors[i].r, Colors[i].g, Colors[i].b ); ++i;
		C_LIGHTCYAN		= SDL_MapRGB( gamescreen->format, Colors[i].r, Colors[i].g, Colors[i].b ); ++i;

		C_LIGHTRED		= SDL_MapRGB( gamescreen->format, Colors[i].r, Colors[i].g, Colors[i].b ); ++i;
		C_LIGHTMAGENTA	= SDL_MapRGB( gamescreen->format, Colors[i].r, Colors[i].g, Colors[i].b ); ++i;
		C_YELLOW		= SDL_MapRGB( gamescreen->format, Colors[i].r, Colors[i].g, Colors[i].b ); ++i;
		C_WHITE			= SDL_MapRGB( gamescreen->format, Colors[i].r, Colors[i].g, Colors[i].b ); ++i;
	}
	else
	{
		SDL_SetColors( gamescreen, Colors, 256-16, 16 );
		C_BLACK			= 240;
		C_BLUE			= 241;
		C_GREEN			= 242;
		C_CYAN			= 243;

		C_RED			= 244;
		C_MAGENTA		= 245;
		C_ORANGE		= 246;
		C_LIGHTGRAY		= 247;

		C_DARKGRAY		= 248;
		C_LIGHTBLUE		= 249;
		C_LIGHTGREEN	= 250;
		C_LIGHTCYAN		= 251;

		C_LIGHTRED		= 252;
		C_LIGHTMAGENTA	= 253;
		C_YELLOW		= 254;
		C_WHITE			= 255;
	}

	if ( sge_TTF_Init() )
	{
		fprintf(stderr, "couldn't start ttf engine: %s\n", SDL_GetError());
		return -1;
	}

	sge_TTF_AAOff();
	
	inkFont = LoadTTF( "aardvark.ttf", 20 );
	if ( !inkFont ) return -1;
	impactFont = LoadTTF( "bradybun.ttf", 20 );	// gooddogc.ttf, 20
	if ( !impactFont ) return -1;
	titleFont = LoadTTF( "deadgrit.ttf", 48 );		// deadgrit.ttf, 48
	if ( !titleFont ) return -1;
	chatFont = LoadTTF( "thin.ttf", 20 );		// deadgrit.ttf, 48
	if ( !chatFont ) return -1;

	fastFont = LoadBMPFont( "brandybun3.png" );
	if ( !fastFont ) return -1;
	creditsFont = LoadBMPFont( "CreditsFont2.png" );//"fangfont.png" );
	if ( !creditsFont ) return -1;
	storyFont = LoadBMPFont( "glossyfont.png" );
	if ( !storyFont ) return -1;
	
    return 0;
}


int init2()
{
	if ( !g_oBackend.Construct() )
	{

		fprintf(stderr, "couldn't start backend.\n" );
		return -1;
	}
	return 0;
}





int DrawMainScreen()
{
	SDL_Surface* background = LoadBackground( "Mortal.jpg", 240 );
	
	DrawTextMSZ( "Version " VERSION " - European Union Editition", inkFont, 320, 430, UseShadow | AlignHCenter, C_WHITE, background, false );
	SDL_Rect r;
	r.x = r.y = 0;
	
	std::string sStaffFilename = DATADIR;
	sStaffFilename += "/characters/staff.dat";
	RlePack pack( sStaffFilename.c_str(), 256 );
	pack.ApplyPalette();
	SDL_BlitSurface( background, NULL, gamescreen, &r );
	SDL_Flip( gamescreen );

/*	char* filename[15] = {
		"Jacint.pl", "Jozsi.pl", "Agent.pl", "Mrsmith.pl",
		"Sleepy.pl", "Tejszin.pl",
		"UPi.pl", "Zoli.pl", "Ulmar.pl", "Bence.pl",
		"Descant.pl", "Grizli.pl", "Sirpi.pl", "Macy.pl", "Cumi.pl" };*/
	int x[14] = {
		0, 26, 67, 125, 159, 209,
		249, 289, 358, 397, 451, 489, 532, 161 };
	int y[14] = {
		5, 4, 5, 5, 5, 7, 
		4, 0, 7, 5, 5, 6, 5, 243 };

	/*
	int i;

	g_oBackend.PerlEvalF( "eval( \"require '%s/characters/Kinga.pl';\" )", DATADIR );
	g_oBackend.PerlEvalF( "eval( \"require '%s/characters/Ambrus.pl';\" )", DATADIR );
	g_oBackend.PerlEvalF( "eval( \"require '%s/characters/Dani.pl';\" )", DATADIR );

	for ( i=0; i<15; ++i )
	{
		pack.Draw( i, x[i], y[i], false );
		SDL_Flip( gamescreen );
		if ( filename[i] != NULL )
		{
			debug( "Loading fighter %s", filename[i] );
			g_oBackend.PerlEvalF( "eval( \"require '%s/characters/%s';\" )", DATADIR, filename[i] );


		}
	}
	
	int retval = 0;
	i = 0;
	*/

	int iNumFighterFiles, i;

#ifdef MACOSX
	//[segabor]
	char char_buf[256];
	sprintf(char_buf, "%s/characters", DATADIR);
	g_oBackend.PerlEvalF( "$CppRetval = GetNumberOfFighterFiles('%s')", char_buf );
#else
	g_oBackend.PerlEvalF( "$CppRetval = GetNumberOfFighterFiles('%s')", DATADIR "/characters" );
#endif
	iNumFighterFiles = g_oBackend.GetPerlInt( "CppRetval" );
	
	for ( i=0; i<iNumFighterFiles; ++i )
	{
		g_oBackend.PerlEvalF( "LoadFighterFile(%d);", i );
		
		if ( i < 15 ) {
			pack.Draw( i, x[i], y[i], false );
			SDL_Flip( gamescreen );
		}
	}
	
    SDL_FreeSurface( background );
	return 0;
	
}





int InitJoystick();




/**
The game loop consists of the following events:

\li Player selection
\li DoGame
\li GameOver and FighterStatsDemo (not in network mode)

The loop ends if the game mode changes to a non-game mode (e.g. IN_DEMO or IN_CHAT)
*/

void GameLoop()
{
	class CVideoModeChange
	{
	public:
		CVideoModeChange( bool a_bWide )
		{
			m_bWide = a_bWide;
			if ( m_bWide ) SetVideoMode( true, g_oState.m_bFullscreen );
		}
		~CVideoModeChange()
		{
			if ( m_bWide ) SetVideoMode( false, g_oState.m_bFullscreen );
		}
		bool m_bWide;
	} oVideoModeChanger( g_oState.m_iNumPlayers > 2 );

#define IS_GAME_MODE (g_oState.m_enGameMode != SState::IN_DEMO \
	&& g_oState.m_enGameMode != SState::IN_CHAT \
	&& !g_oState.m_bQuitFlag)
	
	Audio->PlaySample( "GAME_NEW" );
	Audio->PlayMusic( "GameMusic" );

	bool bNetworkGame = SState::IN_NETWORK == g_oState.m_enGameMode;
	
	if ( bNetworkGame )
	{
		g_oState.m_enTeamMode = SState::Team_ONE_VS_ONE;
	}
	
	while ( IS_GAME_MODE )
	{
		if ( SState::Team_GOOD_VS_EVIL == g_oState.m_enTeamMode )
		{
			std::vector<FighterEnum>& roTeam0 = g_oPlayerSelect.EditPlayerInfo(0).m_aenTeam;
			std::vector<FighterEnum>& roTeam1 = g_oPlayerSelect.EditPlayerInfo(1).m_aenTeam;
			roTeam0.clear();
			roTeam1.clear();

			roTeam0.push_back( SIRPI );
			roTeam0.push_back( MACI );
			roTeam0.push_back( GRIZLI );
			roTeam0.push_back( DANI );
			roTeam0.push_back( KINGA );
			roTeam0.push_back( CUMI );

			roTeam1.push_back( ZOLI );
			roTeam1.push_back( ULMAR );
			roTeam1.push_back( BENCE );
			roTeam1.push_back( AMBRUS );
			roTeam1.push_back( DESCANT );	// Temporary assignment
			roTeam1.push_back( UPI );

			for ( int i=0; i<10; ++i )
			{
				int j = rand() % ( roTeam0.size() -1 );
				int k = rand() % ( roTeam0.size() -1 );
				FighterEnum enTemp;
				enTemp = roTeam0[j]; roTeam0[j] = roTeam0[k]; roTeam0[k] = enTemp;

				j = rand() % ( roTeam1.size() -1 );
				k = rand() % ( roTeam1.size() -1 );
				enTemp = roTeam1[j]; roTeam1[j] = roTeam1[k]; roTeam1[k] = enTemp;
			}
		}
		else
		{
			g_oPlayerSelect.DoPlayerSelect();
		}
		
		if ( !IS_GAME_MODE ) break;

		//sprintf( acReplayFile, "/tmp/msz%d.replay", ++iGameNumber );

		int iGameResult = DoGame( NULL, false, bDebug );
		//int iGameResult = DoGame( acReplayFile, false, bDebug );
		//DoGame( acReplayFile, true, bDebug );
		debug ( "iGameResult = %d\n", iGameResult );

		if ( !IS_GAME_MODE ) break;
		
		if ( iGameResult >= 0 && !bNetworkGame )
		{
			GameOver( iGameResult );
			FighterStatsDemo oDemo( g_oPlayerSelect.GetPlayerInfo( iGameResult ).m_enFighter );
			oDemo.Run();
		}
		
		if ( !IS_GAME_MODE ) break;
	}

	if ( bNetworkGame && !g_oState.m_bQuitFlag )
	{
		DrawTextMSZ( "Connection closed.", inkFont, 320, 210, AlignHCenter | UseShadow, C_WHITE, gamescreen );
		DrawTextMSZ( g_poNetwork->GetLastError(), impactFont, 320, 250, AlignHCenter | UseShadow, C_WHITE, gamescreen );
		SDL_Delay( 1000 );
		GetKey( true );
	}

	if ( !g_oState.m_bQuitFlag )
	{
		Audio->PlayMusic( "DemoMusic" );
	}

}



/**
The chat loop consists of:

\li DoOnlineChat
\li GameLoop (if a game was started

The loop ends if DoOnlineChat returns with a quit or disconnect
(not IN_NETWORK mode).
*/

void ChatLoop()
{
	while (1)
	{
		DoOnlineChat();
		
		if ( g_oState.m_bQuitFlag ||
			SState::IN_CHAT != g_oState.m_enGameMode )
		{
			break;
		}
		
		if ( SState::IN_NETWORK == g_oState.m_enGameMode )
		{
			GameLoop();
		}
		
		if ( g_oState.m_bQuitFlag ) break;

		g_oState.m_enGameMode = SState::IN_CHAT;
	}
}


void PgTest();


int main(int argc, char *argv[])
{
	srand( (unsigned int)time(NULL) );
	if ( 0 != init2() )
	{
		fprintf( stderr, "init2() failed." );
		return -1;
	}
	
	g_oState.m_pcArgv0 = argv[0];
	g_oState.Load();
	CMortalNetwork::Create();
	
	bDebug = false;

	int i;
	for ( i=1; i<argc; ++i )
	{
		if ( !strcmp(argv[i], "-debug") )
		{
			bDebug = true;
		}
/*
		else if ( !strcmp(argv[i], "-fullscreen") )
		{
			iFlags |= SDL_FULLSCREEN;
		}
		else if ( !strcmp(argv[i], "-hwsurface") )
		{
			iFlags |= SDL_HWSURFACE;
		}
		else if ( !strcmp(argv[i], "-doublebuf") )
		{
			iFlags |= SDL_DOUBLEBUF;
		}
		else if ( !strcmp(argv[i], "-anyformat") )
		{
			iFlags |= SDL_ANYFORMAT;
		}

*/
		else
		{
//			printf( "Usage: %s [-debug] [-fullscreen] [-hwsurface] [-doublebuf] [-anyformat]\n", argv[0] );
			printf( "Usage: %s [-debug]\n", argv[0] );
			return 0;
		}
	}

	if (init()<0)
	{
		return -1;
	}

	InitJoystick();
	
	g_oState.SetLanguage( g_oState.m_acLanguage );
	
	new MszAudio;
//	Audio->LoadMusic( "Last_Ninja_-_The_Wilderness.mid", "DemoMusic" );
	Audio->LoadMusic( "ride.mod", "DemoMusic" );
	Audio->PlayMusic( "DemoMusic" );
	Audio->LoadMusic( "2nd_pm.s3m", "GameMusic" );
	
	DrawMainScreen();
	
	g_oPlayerSelect.SetPlayer( 0, UPI );
	g_oPlayerSelect.SetPlayer( 1, ULMAR );
	
	/*
	{
		int iGameNumber=0;
		char acReplayFile[1024];
		
		for ( i=0; i<15; ++i )
		{
			sprintf( acReplayFile, DATADIR "/msz%i.replay", i );
			DrawTextMSZ( acReplayFile, impactFont, 10, 10, 0, C_WHITE, gamescreen );
			SDL_Delay(5000 );
			
			DoGame( acReplayFile, true, bDebug );
		}
	}

	*/

/*	while ( !g_oState.m_bQuitFlag )
	{
		g_oState.m_enGameMode = SState::IN_MULTI;
		g_oState.m_bTeamMultiselect = false;
		g_oState.m_iTeamSize = 3;
		g_oState.m_enTeamMode = SState::Team_CUSTOM;
		
		std::vector<FighterEnum>& roTeam0 = g_oPlayerSelect.EditPlayerInfo(0).m_aenTeam;
		std::vector<FighterEnum>& roTeam1 = g_oPlayerSelect.EditPlayerInfo(1).m_aenTeam;
		
		roTeam0.clear();
		roTeam0.push_back( ZOLI );
		roTeam0.push_back( SIRPI );
		roTeam0.push_back( MACI );
		roTeam1.clear();
		roTeam1.push_back( UPI );
		roTeam1.push_back( ZOLI );
		roTeam1.push_back( ULMAR );
		
		DoGame( NULL, false, false );
	}
*/	
	while ( 1 )
	{
		if ( g_oState.m_bQuitFlag ) break;
		
		switch ( g_oState.m_enGameMode )
		{
		case SState::IN_DEMO:
			DoDemos();
			continue;
			
		case SState::IN_CHAT:
			ChatLoop();
			continue;
			
		default:
			GameLoop();
			continue;
		}

#if 0
		// Remaining are game modes: IN_SINGLE, IN_MULTI, IN_NETWORK
		
		Audio->PlaySample( "car_start.voc" );
		Audio->PlayMusic( "GameMusic" );

		bNetworkGame = false;

		while ( g_oState.m_enGameMode != SState::IN_DEMO
			&& g_oState.m_enGameMode != SState::IN_CHAT
			&& !g_oState.m_bQuitFlag )
		{
			bNetworkGame = SState::IN_NETWORK == g_oState.m_enGameMode;
			
			g_oPlayerSelect.DoPlayerSelect();
			if ( g_oState.m_bQuitFlag || g_oState.m_enGameMode == SState::IN_DEMO  ) break;
			
			//sprintf( acReplayFile, "/tmp/msz%d.replay", ++iGameNumber );
			
			int iGameResult = DoGame( NULL, false, bDebug );
			//int iGameResult = DoGame( acReplayFile, false, bDebug );
			//DoGame( acReplayFile, true, bDebug );
			if ( g_oState.m_bQuitFlag || g_oState.m_enGameMode == SState::IN_DEMO  ) break;
			
			debug ( "iGameResult = %d\n", iGameResult );
			
			if ( iGameResult >= 0 && !bNetworkGame )
			{
				GameOver( iGameResult );


				FighterStatsDemo oDemo( g_oPlayerSelect.GetPlayerInfo( iGameResult ).m_enFighter );
				oDemo.Run();		
			}
			if ( g_oState.m_bQuitFlag || g_oState.m_enGameMode == SState::IN_DEMO  ) break;
		}
		
		if ( bNetworkGame && !g_oState.m_bQuitFlag )
		{
			DrawTextMSZ( "Connection closed.", inkFont, 320, 210, AlignHCenter | UseShadow, C_WHITE, gamescreen );
			DrawTextMSZ( g_poNetwork->GetLastError(), impactFont, 320, 250, AlignHCenter | UseShadow, C_WHITE, gamescreen );
			SDL_Delay( 1000 );
			GetKey( true );
		}
		
		if ( g_oState.m_bQuitFlag ) break;
		Audio->PlayMusic( "DemoMusic" );
#endif
	}
	
	g_oState.Save();
	
	SDL_Quit();
	
	return EXIT_SUCCESS;
}

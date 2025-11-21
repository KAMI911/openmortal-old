/***************************************************************************
                          State.cpp  -  description
                             -------------------
    begin                : Mon Aug 12 2003
    copyright            : (C) 2003 by upi
    email                : upi@apocalypse.rulez.org
 ***************************************************************************/

//[segabor] using Xcode to build OpenMortal makes config.h unneccessary
#ifndef MACOSX
#include "../config.h"
#endif
#include "gfx.h"
#include "common.h"
#include "State.h"

#include "SDL_keysym.h"
#include "SDL_mixer.h"

#include <string>
#include <fstream>
#ifdef MACOSX
//[segabor]
#include <CoreFoundation/CoreFoundation.h>
#endif
//include <unistd.h>

#include "Backend.h"
#include "MszPerl.h"

extern PerlInterpreter* my_perl;

SState g_oState;



std::string GetConfigHeader()
{
	std::string sHeader( "Simple config file " );
	sHeader += PACKAGE " " VERSION;
	return sHeader;
}


std::string GetConfigFilename()
{
#ifdef _WINDOWS
	if ( NULL != g_oState.m_pcArgv0 )
	{
		return std::string(g_oState.m_pcArgv0) + ".ini";
	}
	return "c:\\openmortal.ini";	
#elif defined(MACOSX)
	//[segabor] get os-ified path
	return std::string(getenv("HOME")) + "/Library/Preferences/OpenMortal.cfg";
#else
	return std::string(getenv("HOME")) + "/.openmortalrc";
#endif
}






SState::SState()
{
	// 1. SET THE TRIVIAL DEFAULTS

	m_enGameMode = IN_DEMO;

	m_bQuitFlag = false;
	m_pcArgv0 = NULL;

	m_iNumPlayers = 2;
	m_enTeamMode = Team_ONE_VS_ONE;
	m_iTeamSize = 5;
	m_bTeamMultiselect = false;

	m_iGameTime = 60;
	m_iHitPoints = 100;
	m_iGameSpeed = 12;

	#ifdef _WINDOWS
		#ifdef _DEBUG
			m_bFullscreen = false;
		#else
			m_bFullscreen = true;
		#endif
	#elif defined(MACOSX)
		//[segabor]
		#ifdef DEBUG
			m_bFullscreen = false;
		#else
			m_bFullscreen = true;
		#endif
	#else
		m_bFullscreen = false;
	#endif

	m_iChannels = 2;
	m_iMixingRate = MIX_DEFAULT_FREQUENCY;
	m_iMixingBits = 2;
	m_iMusicVolume = 50;
	m_iSoundVolume = 100;

	static const int aiDefaultKeys[MAXPLAYERS][9] = {
  		{ SDLK_UP, SDLK_DOWN, SDLK_LEFT, SDLK_RIGHT, SDLK_PAGEDOWN,
            SDLK_DELETE, SDLK_INSERT, SDLK_END, SDLK_HOME },
  		{ SDLK_w, SDLK_s, SDLK_a, SDLK_d, SDLK_x,
            SDLK_f, SDLK_r, SDLK_g, SDLK_t },
		{ SDLK_u, SDLK_j, SDLK_h, SDLK_k, SDLK_i,
			SDLK_b, SDLK_n, SDLK_m, SDLK_COMMA },
		{ SDLK_F2, SDLK_F3, SDLK_F4, SDLK_F5, SDLK_F6,
			SDLK_F7, SDLK_F8, SDLK_F9, SDLK_F10 },
	};

	for ( int i=0; i<MAXPLAYERS; ++i )
		for ( int j=0; j<9; ++j )
			m_aiPlayerKeys[i][j] = aiDefaultKeys[i][j];


	strcpy( m_acLatestServer, "apocalypse.rulez.org" );
	m_bServer = false;

	strcpy( m_acNick, "Mortal" );
	strcpy( m_acLanguage, "en" );

	// 2. SO FAR THESE WERE THE EASY DEFAULTS
	// NOW MOVE ON TO THE TRICKIER ONES.

	// 2.1. FIND THE LANGUAGE
#ifdef _WINDOWS
	LANGID iLangID = GetUserDefaultLangID() & 0x007f;
	const char* pcLang;
	switch ( iLangID )
	{
	case 0x0e: pcLang = "hu";
	case 0x0c: pcLang = "fr";
	case 0x0a: pcLang = "es";
	default: pcLang = "en";
	}

	strcpy( m_acNick, pcLang );
	

#elif defined(MACOSX)
	//[segabor] OS X style locale handling
	CFLocaleRef userLocaleRef = CFLocaleCopyCurrent();
	char cbuff[255];
	
	CFStringGetCString(CFLocaleGetIdentifier(userLocaleRef),
					   cbuff,
					   255,
					   kCFStringEncodingASCII);
	m_acLanguage[0] = cbuff[0];
	m_acLanguage[1] = cbuff[1];
	m_acLanguage[2] = 0;
	
	debug("Language code is %s\n", m_acLanguage);
#else
	// Read the locale from the operating system
	char* pcLocale = setlocale( LC_CTYPE, NULL );
	debug( "The locale returned by the operating system is '%s'\n", pcLocale ? pcLocale : "NULL" );
	
	if ( NULL == pcLocale
		|| strcmp( pcLocale, "C") == 0 )
	{
		// Try the 'GETENV' method
		pcLocale = getenv( "LANG" );
		debug( "The LANG envvar is '%s'\n", pcLocale ? pcLocale : "NULL" );
	}

	if ( NULL != pcLocale )
	{
		strncpy( m_acLanguage, pcLocale, 2 );
		m_acLanguage[2] = 0;
	}
	else
	{
		strcpy( m_acLanguage, "en" );
	}
#endif

	// 2.2. FIND THE USER NAME

#ifdef _WINDOWS
	m_acNick[0] = 0;
	DWORD iLen = 127;
	BOOL iResult = GetUserName( m_acNick, &iLen );
	if ( 0 == iResult ) 
	{
		m_acNick[0] = 0;
		debug( "GetUserName failed: %d.\n", iResult );
	}
#else
	int iResult = getlogin_r( m_acNick, 127 );
	if ( iResult )
	{
		debug( "getlogin_r failed: %d\n", iResult );
		strcpy( m_acNick, getenv("USER") );
	}
#endif
	
	if ( !m_acNick[0] )
	{
		strcpy( m_acNick, "Mortal");	// Last-ditch default..
	}

};




void SState::ToggleFullscreen()
{
	m_bFullscreen = !m_bFullscreen;

	bool bPaletted = ( gamescreen->format->BitsPerPixel <= 8 );
	SDL_Color aoPalette[256];
	int iNumColors = 0;

	if ( bPaletted )
	{
		iNumColors = gamescreen->format->palette->ncolors;
		if ( iNumColors > 256 ) iNumColors = 256;
		for ( int i=0; i<iNumColors; ++i )
		{
			aoPalette[i].r = gamescreen->format->palette->colors[i].r;
			aoPalette[i].g = gamescreen->format->palette->colors[i].g;
			aoPalette[i].b = gamescreen->format->palette->colors[i].b;
			aoPalette[i].unused = 0;
		}
	}

	SetVideoMode( gamescreen->w > 640, m_bFullscreen );

	if ( bPaletted )
	{
		SDL_SetPalette( gamescreen, SDL_LOGPAL | SDL_PHYSPAL, aoPalette, 0, iNumColors );
	}
}



void SState::SetLanguage( const char* a_pcLanguage )
{
	if ( m_acLanguage != a_pcLanguage )
	{
		strncpy( m_acLanguage, a_pcLanguage, 9 );
		m_acLanguage[9] = 0;
	}
	g_oBackend.PerlEvalF( "SetLanguage('%s');", m_acLanguage );
	SV* poSv = get_sv("LanguageNumber", FALSE);
	if (poSv)
	{
		m_iLanguageCode = SvIV( poSv );
	}
	else
	{
		m_iLanguageCode = 0;
	}
}



void SState::SetServer( const char* a_pcServer )
{
	if ( a_pcServer )
	{
		strncpy( m_acLatestServer, a_pcServer, 255 );
		m_acLatestServer[255] = 0;
		m_bServer = false;
	}
	else
	{
		m_bServer = true;
	}
}


void SState::Load()
{
	std::string sFilename = GetConfigFilename();
	g_oBackend.PerlEvalF( "ParseConfig('%s');", sFilename.c_str() );
	
	SV* poSv;

//	poSv = get_sv("", FALSE); if (poSv) m_ = SvIV( poSv );

	poSv = get_sv("NUMPLAYERS", FALSE); if (poSv) m_iNumPlayers = SvIV( poSv );
	poSv = get_sv("TEAMMODE", FALSE); if (poSv) m_enTeamMode = (TTeamModeEnum) SvIV( poSv );
	poSv = get_sv("TEAMSIZE", FALSE); if (poSv) m_iTeamSize = SvIV( poSv );
	poSv = get_sv("TEAMMULTISELECT", FALSE); if (poSv) m_bTeamMultiselect = SvIV( poSv );

	poSv = get_sv("GAMETIME", FALSE); if (poSv) m_iGameTime = SvIV( poSv );
	poSv = get_sv("HITPOINTS", FALSE); if (poSv) m_iHitPoints = SvIV( poSv );
	poSv = get_sv("GAMESPEED", FALSE); if (poSv) m_iGameSpeed = SvIV( poSv );

	poSv = get_sv("FULLSCREEN", FALSE); if (poSv) m_bFullscreen = SvIV( poSv );
	poSv = get_sv("CHANNELS", FALSE); if (poSv) m_iChannels = SvIV( poSv );
	poSv = get_sv("MIXINGRATE", FALSE); if (poSv) m_iMixingRate = SvIV( poSv );
	poSv = get_sv("MIXINGBITS", FALSE); if (poSv) m_iMixingBits = SvIV( poSv );
	poSv = get_sv("MUSICVOLUME", FALSE); if (poSv) m_iMusicVolume = SvIV( poSv );
	poSv = get_sv("SOUNDVOLUME", FALSE); if (poSv) m_iSoundVolume = SvIV( poSv );
	poSv = get_sv("LANGUAGE", FALSE); if (poSv) { strncpy( m_acLanguage, SvPV_nolen( poSv ), 9 ); m_acLanguage[9] = 0; }

	poSv = get_sv("LATESTSERVER", FALSE); if (poSv) { strncpy( m_acLatestServer, SvPV_nolen( poSv ), 255 ); m_acLatestServer[255] = 0; }
	poSv = get_sv("SERVER", FALSE); if (poSv) m_bServer = SvIV( poSv );
	poSv = get_sv("NICK", FALSE); if (poSv) { strncpy( m_acNick, SvPV_nolen( poSv ), 127 ); m_acNick[127] = 0; }
	
	char pcBuffer[1024];
	for ( int i=0; i<MAXPLAYERS; ++i )
	{
		for ( int j=0; j<9; ++j )
		{
			sprintf( pcBuffer, "PLAYER%dKEY%d", i, j );
			poSv = get_sv(pcBuffer, FALSE); if (poSv) m_aiPlayerKeys[i][j] = SvIV( poSv );
		}
	}	
}


void SState::Save()
{
	std::string sFilename = GetConfigFilename();
	std::ofstream oStream( sFilename.c_str(), std::ios_base::out | std::ios_base::trunc );
	if ( oStream.rdstate() & std::ios::failbit )
	{
		debug( "Unable to open config file: %s\n", sFilename.c_str() );
		return;
	}
	
	oStream << GetConfigHeader() << '\n';

//	oStream << "=" << m_ << '\n';

	oStream << "NUMPLAYERS=" << m_iNumPlayers << '\n';
	oStream << "TEAMMODE=" << m_enTeamMode << '\n';
	oStream << "TEAMSIZE=" << m_iTeamSize << '\n';
	oStream << "TEAMMULTISELECT=" << m_bTeamMultiselect << '\n';

	oStream << "GAMETIME=" << m_iGameTime << '\n';
	oStream << "HITPOINTS=" << m_iHitPoints << '\n';
	oStream << "GAMESPEED=" << m_iGameSpeed << '\n';

	oStream << "FULLSCREEN=" << m_bFullscreen << '\n';
	oStream << "CHANNELS=" << m_iChannels << '\n';
	oStream << "MIXINGRATE=" << m_iMixingRate << '\n';
	oStream << "MIXINGBITS=" << m_iMixingBits << '\n';
	oStream << "MUSICVOLUME=" << m_iMusicVolume << '\n';
	oStream << "SOUNDVOLUME=" << m_iSoundVolume << '\n';
	oStream << "LANGUAGE=" << m_acLanguage << '\n';

	oStream << "LATESTSERVER=" << m_acLatestServer << '\n';
	oStream << "SERVER=" << m_bServer << '\n';
	oStream << "NICK=" << m_acNick << '\n';
	
	for ( int i=0; i<MAXPLAYERS; ++i )
	{
		for ( int j=0; j<9; ++j )
		{
			oStream << "PLAYER" <<i<< "KEY" <<j<< '=' << m_aiPlayerKeys[i][j] << '\n';
		}
	}
	
	oStream.flush();
	oStream.close();
}



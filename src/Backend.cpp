/***************************************************************************
                          Backend.cpp  -  description
                             -------------------
    begin                : Mon Dec 9 2002
    copyright            : (C) 2002 by upi
    email                : upi@apocalypse.rulez.org
 ***************************************************************************/


#include "common.h"
#include "Backend.h"
#include "Audio.h"
#include "State.h"

#include <string>
#include <stdarg.h>
#include "MszPerl.h"


/***************************************************************************
                     PUBLIC EXPORTED VARIABLES
***************************************************************************/

PerlInterpreter*	my_perl;
CBackend			g_oBackend;


/***************************************************************************
                     PRIVATE VARIABLES (perl variable space)
***************************************************************************/

SV
	*perl_bgx, *perl_bgy,
	*perl_px[MAXPLAYERS], *perl_py[MAXPLAYERS], *perl_pf[MAXPLAYERS], 
	*perl_ph[MAXPLAYERS], *perl_phreal[MAXPLAYERS],
	*perl_gametick, *perl_over, *perl_ko;

SV
	*perl_doodad_x, *perl_doodad_y,
	*perl_doodad_t, *perl_doodad_f,
	*perl_doodad_dir, *perl_doodad_gfxowner,
	*perl_doodad_text;

SV
	*perl_sound, *perl_Translated;



/***************************************************************************
                  		TRANSLATION SERVICES
***************************************************************************/

const char* Translate( const char* a_pcText )
{
	dSP ;
	
	ENTER ;
	SAVETMPS ;
	
	PUSHMARK(SP) ;
	XPUSHs(sv_2mortal(newSVpv(a_pcText, 0)));
	PUTBACK ;
	
	call_pv("Translate", G_DISCARD);
	FREETMPS ;
	LEAVE ;
	
	if ( NULL == perl_Translated )
	{
		perl_Translated = get_sv("Translated", TRUE);
	}
	return SvPV_nolen( perl_Translated );
}


const char* TranslateUTF8( const char* a_pcText )
{
	dSP ;
	
	ENTER ;
	SAVETMPS ;
	
	PUSHMARK(SP) ;
	XPUSHs(sv_2mortal(newSVpv(a_pcText, 0)));
	PUTBACK ;
	
	call_pv("Translate", G_DISCARD);
	FREETMPS ;
	LEAVE ;
	
	if ( NULL == perl_Translated )
	{
		perl_Translated = get_sv("Translated", TRUE);
	}
	return SvPVutf8_nolen( perl_Translated );
}


/***************************************************************************
                  		BACKEND CLASS IMPLEMENTATION
***************************************************************************/


#define PERLEVAL(A) eval_pv(A, TRUE);

#define PERLCALL(PROC,A,B) {							\
    dSP;												\
    ENTER;												\
    SAVETMPS;											\
    PUSHMARK(SP);										\
    XPUSHs(sv_2mortal(newSViv(A)));						\
    XPUSHs(sv_2mortal(newSViv(B)));						\
    PUTBACK ;											\
														\
    call_pv( (PROC),  G_DISCARD );						\
														\
    FREETMPS;											\
    LEAVE;												\
}


CBackend::CBackend()
{
	m_iBgX = m_iBgY = 0;
	m_iNumDoodads = m_iNumSounds = 0;
	for ( int i=0; i<MAXPLAYERS; ++i )
	{
		m_aoPlayers[i].m_iX = m_aoPlayers[i].m_iY = 0;
		m_aoPlayers[i].m_iFrame = 0;
		m_aoPlayers[i].m_iHitPoints = 0;
	}
}


CBackend::~CBackend()
{
	if ( NULL != my_perl )
	{
		perl_destruct( my_perl );
		perl_free( my_perl );
		my_perl = NULL;
	}
}


bool CBackend::Construct()
{
	if ( my_perl != NULL )
	{
		// Already inited
		return false;
	}
	
	perl_bgx = NULL;
	perl_doodad_x = NULL;
	
	std::string sFileName = DATADIR;
	sFileName += "/script";

#ifndef _WINDOWS
	if (chdir(sFileName.c_str()) != 0) {
	    perror("chdir failed");
        return false;  // vagy más megfelelő hibakezelés
}
#endif
	
//	char *perl_argv[] = {"", "-d:Trace", "Backend.pl"};
//	int perl_argc = 3;
	char perl_argv0[] = "";
	char perl_argv1[] = "Backend.pl";
	char *perl_argv[] = { perl_argv0, perl_argv1 };
	//const char *perl_argv[] = {"", "Backend.pl"};
	int perl_argc = 2;
	my_perl = perl_alloc();
	if ( my_perl == NULL )
	{
		return false;
	}
	
	perl_construct( my_perl );
	if ( perl_parse( my_perl, NULL, perl_argc, perl_argv, (char**)NULL ) )
	{
		char *error = SvPV_nolen(get_sv("@", FALSE));
		fprintf( stderr, "%s", error );
		return false;
	}
	
	if ( perl_run( my_perl ) )
	{
		char *error = SvPV_nolen(get_sv("@", FALSE));
		fprintf( stderr, "%s", error );
		return false;
	}

	return true;
}


const char* CBackend::PerlEvalF( const char* a_pcFormat, ... )
{
	va_list ap;
	va_start( ap, a_pcFormat );
	
	char acBuffer[1024];
	vsnprintf( acBuffer, 1023, a_pcFormat, ap );
	acBuffer[1023] = 0;
	eval_pv(acBuffer,FALSE);
	
	const char *pcError = SvPV_nolen(get_sv("@", FALSE));
	if ( pcError && *pcError )
	{
		debug( "eval '%s': '%s'\n", acBuffer, pcError );
		exit(0);
	}
	
	va_end( ap );
	
	return pcError;
}


const char* CBackend::GetPerlString( const char* acScalarName )
{
	SV* poScalar = get_sv( acScalarName, FALSE );
	if ( NULL == poScalar )
	{
		return "";
	}
	
	return SvPV_nolen( poScalar );
}


int CBackend::GetPerlInt( const char* acScalarName )
{
	SV* poScalar = get_sv( acScalarName, FALSE );
	if ( NULL == poScalar )
	{
		return 0;
	}
	
	return SvIV( poScalar );
}



/** Returns the total number of registered fighters in the backend.
This may be more than the actual number of playable characters, as
some or many characters may not be ready or installed.

\see GetFighterID
\see GetNumberOfAvailableFighters
*/
int CBackend::GetNumberOfFighters()
{
	PerlEvalF( "$::CppNumberOfFighters = scalar keys %%::FighterStats;" );
	return GetPerlInt( "CppNumberOfFighters" );
}


/** Returns the ID of a fighter. The index parameter should start from
zero, and be less than the value returned by GetNumberOfFighters().

\see GetNumberOfFighters
*/
FighterEnum CBackend::GetFighterID( int a_iIndex )
{
	PerlEvalF( "$::CppFighterID = (sort { $a - $b } keys %%::FighterStats)[%d];", a_iIndex );
	return (FighterEnum) GetPerlInt( "CppFighterID" );
}


/** Returns the number of fighters that are locally availaible for play.
This number is smaller or equal to the value returned by GetNumberOfFighters().

Note that the method GetFighterID() returns ID's of non-available
fighters as well, so it can only be used in conjunction with GetNumberOfFighters()

\see GetNumberOfFighters
*/
int CBackend::GetNumberOfAvailableFighters()
{
	PerlEvalF( "GetNumberOfAvailableFighters();" );	// Defined in FighterStats.pl
	return GetPerlInt( "CppNumberOfAvailableFighters" );
}


/**
Makes the perl interpreter advance from the current scene to the next one. 
This should be called a constant number of time per second to make the game 
running seamlessly. Most speedup or slowdown effects are accomplished by 
changing the frequency at which this method is called.
*/
void CBackend::AdvancePerl()
{
	PerlEvalF("GameAdvance();");
}



void CBackend::ReadFromPerl()
{
	int i;

	if ( perl_bgx == NULL )
	{
		perl_gametick = get_sv("gametick", TRUE);
		perl_bgx = get_sv("bgx", TRUE);
		perl_bgy = get_sv("bgy", TRUE);
		perl_over= get_sv("over", TRUE);
		perl_ko = get_sv("ko", TRUE);

		char acVarName[128];

		for ( i=0; i<MAXPLAYERS; ++i )
		{
			sprintf( acVarName, "p%dx", i+1 );
			perl_px[i] = get_sv(acVarName, TRUE);
			sprintf( acVarName, "p%dy", i+1 );
			perl_py[i] = get_sv(acVarName, TRUE);
			sprintf( acVarName, "p%df", i+1 );
			perl_pf[i] = get_sv(acVarName, TRUE);
			sprintf( acVarName, "p%dh", i+1 );
			perl_ph[i] = get_sv(acVarName, TRUE);
			sprintf( acVarName, "p%dhreal", i+1 );
			perl_phreal[i] = get_sv(acVarName, TRUE);
		}
	}

	m_iGameTick = SvIV( perl_gametick );
	m_iBgX = SvIV( perl_bgx );
	m_iBgY = SvIV( perl_bgy );
	m_iGameOver = SvIV( perl_over );
	m_bKO = SvIV( perl_ko ) != 0;

	for ( i=0; i<g_oState.m_iNumPlayers; ++i )
	{
		m_aoPlayers[i].m_iX = SvIV( perl_px[i] );
		m_aoPlayers[i].m_iY = SvIV( perl_py[i] );
		m_aoPlayers[i].m_iFrame = SvIV( perl_pf[i] );
		m_aoPlayers[i].m_iHitPoints = SvIV( perl_ph[i] ) / 10;
		m_aoPlayers[i].m_iRealHitPoints = SvIV( perl_phreal[i] );
	}
	
	// READ DOODAD DATA
	
	if ( perl_doodad_x == NULL )
	{
		perl_doodad_x = get_sv("doodad_x", TRUE);
		perl_doodad_y = get_sv("doodad_y", TRUE);
		perl_doodad_t = get_sv("doodad_t", TRUE);
		perl_doodad_f = get_sv("doodad_f", TRUE);
		perl_doodad_dir = get_sv("doodad_dir", TRUE);
		perl_doodad_gfxowner = get_sv("doodad_gfxowner", TRUE);
		perl_doodad_text = get_sv("doodad_text", TRUE);
	}
	
	for ( m_iNumDoodads=0; m_iNumDoodads<MAXDOODADS; ++m_iNumDoodads )
	{
		PERLEVAL("GetNextDoodadData();");
		
		SDoodad& oDoodad = m_aoDoodads[m_iNumDoodads];
		
		oDoodad.m_iType = SvIV(perl_doodad_t);
		
		if ( oDoodad.m_iType < 0 )
		{
			break;
		}
		
		oDoodad.m_iX = SvIV(perl_doodad_x);
		oDoodad.m_iY = SvIV(perl_doodad_y);
		oDoodad.m_iFrame = SvIV(perl_doodad_f);
		oDoodad.m_iDir = SvIV(perl_doodad_dir);
		oDoodad.m_iGfxOwner = SvIV(perl_doodad_gfxowner);

		if ( oDoodad.m_iType == 0 )
		{
			oDoodad.m_sText = SvPV_nolen(perl_doodad_text);
		}
		else
		{
			oDoodad.m_sText = "";
		}
	}
	
	
	// READ SOUND DATA
	
	if ( perl_sound == NULL )
	{
		perl_sound = get_sv("sound", TRUE);
	}
	
	for ( m_iNumSounds=0; m_iNumSounds<MAXSOUNDS; ++m_iNumSounds )
	{
		PERLEVAL("GetNextSoundData();");
		const char* pcSound = SvPV_nolen(perl_sound);
	
		if ( NULL == pcSound
			|| 0 == *pcSound )
		{
			break;
		}
		
		m_asSounds[ m_iNumSounds ] = pcSound;
		//Audio->PlaySample( pcSound );
	}
}


bool CBackend::IsDead( int a_iPlayer )
{
	return m_aoPlayers[ a_iPlayer ].m_iRealHitPoints <= 0;
}


void CBackend::PlaySounds()
{
	for ( int i=0; i<m_iNumSounds; ++i )
	{
		Audio->PlaySample( m_asSounds[i].c_str() );
	}
}



/***************************************************************************
                  	PLAYBACK STRING CONVERSION ROUTINES
***************************************************************************/


void CBackend::WriteToString( std::string& a_rsOutString )
{
	char acBuffer[2048];
	int iNumChars = sprintf( acBuffer, "%d %d  %d %d %d %d  %d %d %d %d  %d  ",
		m_iBgX, m_iBgY,
		m_aoPlayers[0].m_iX, m_aoPlayers[0].m_iY, m_aoPlayers[0].m_iFrame, m_aoPlayers[0].m_iHitPoints,
		m_aoPlayers[1].m_iX, m_aoPlayers[1].m_iY, m_aoPlayers[1].m_iFrame, m_aoPlayers[1].m_iHitPoints,
		m_iNumDoodads );
	
	int i;
	for ( i = 0; i<m_iNumDoodads; ++i )
	{
		SDoodad& roDoodad = m_aoDoodads[i];
		// iNumChars += sprintf( acBuffer+iNumChars, "%d %d %d %d %d %d %d %s  ",
		iNumChars += sprintf(acBuffer + iNumChars, "%d %d %d %d %d %d %zu %s  ",
			roDoodad.m_iX, roDoodad.m_iY, roDoodad.m_iType, roDoodad.m_iFrame,
			roDoodad.m_iDir, roDoodad.m_iGfxOwner,
			roDoodad.m_sText.size(), roDoodad.m_sText.c_str() );
	}
	
	iNumChars += sprintf( acBuffer+iNumChars, "%d ", m_iNumSounds );
	for ( i = 0; i<m_iNumSounds; ++i )
	{
		iNumChars += sprintf( acBuffer+iNumChars, " %zu %s",
			m_asSounds[i].size(), m_asSounds[i].c_str() );
	}
	
	// debug( "Frame: '%s'\n", acBuffer );
	a_rsOutString = acBuffer;
}


void CBackend::ReadFromString( const std::string& a_rsString )
{
	ReadFromString( a_rsString.c_str() );
}


void CBackend::ReadFromString( const char* a_pcBuffer )
{
	if ( strlen( a_pcBuffer ) < 10 )
	{
		m_iNumDoodads = m_iNumSounds = 0;
		return;
	}
	
	int iNumMatches;
	int iOffset, iTotal;
	iNumMatches = sscanf( a_pcBuffer, "%d %d %d %d %d %d %d %d %d %d %d%n",
		&m_iBgX, &m_iBgY,
		&m_aoPlayers[0].m_iX, &m_aoPlayers[0].m_iY, &m_aoPlayers[0].m_iFrame, &m_aoPlayers[0].m_iHitPoints,
		&m_aoPlayers[1].m_iX, &m_aoPlayers[1].m_iY, &m_aoPlayers[1].m_iFrame, &m_aoPlayers[1].m_iHitPoints,
		&m_iNumDoodads, &iTotal );
	
	if ( m_iNumDoodads > MAXDOODADS )
	{
		m_iNumDoodads = m_iNumSounds = 0;
		return;
	}
	
	int i, j;
	for ( i=0; i<m_iNumDoodads; ++i )
	{
		SDoodad& roDoodad = m_aoDoodads[i];
		iNumMatches += sscanf( a_pcBuffer+iTotal, "%d %d %d %d %d %d %d %n",
			&roDoodad.m_iX, &roDoodad.m_iY, &roDoodad.m_iType, &roDoodad.m_iFrame,
			&roDoodad.m_iDir, &roDoodad.m_iGfxOwner,
			&j, &iOffset );
		iTotal += iOffset;
		roDoodad.m_sText.assign( a_pcBuffer + iTotal, j );
		iTotal += j;
	}
	
	iNumMatches += sscanf( a_pcBuffer + iTotal, "%d%n",
		&m_iNumSounds, &iOffset );
		
	if ( m_iNumSounds > MAXSOUNDS )
	{
		m_iNumSounds = 0;
		return;
	}
	
	iTotal += iOffset;
	
	for ( i=0; i<m_iNumSounds; ++i )
	{
		iNumMatches += sscanf( a_pcBuffer+iTotal, "%d %n",
			&j, &iOffset );
		iTotal += iOffset;
		m_asSounds[i].assign( a_pcBuffer + iTotal, j );
		iTotal += j;
	}
}


/***************************************************************************
                          common.h  -  description
                             -------------------
    begin                : Fri Aug 24 2001
    copyright            : (C) 2001 by upi
    email                : upi@apocalypse.rulez.org
 ***************************************************************************/

#ifndef __COMMON_H
#define __COMMON_H


#ifndef _SDL_types_h
#include "SDL_types.h"
#endif

struct SDL_Surface;
#define MAXPLAYERS 4


void debug( const char* format, ... );
#ifndef ABS
#define ABS(A) ( (A>=0) ? (A) : -(A) )
#endif
#ifndef MAX
#define MAX(A,B) ( (A) > (B) ? (A) : (B) )
#endif
#ifndef MIN
#define MIN(A,B) ( (A) < (B) ? (A) : (B) )
#endif

// -----------------------------------------------------------------------
// Runtime data directory
// -----------------------------------------------------------------------
// MSZ_DATADIR normally compiles in the autoconf-time pkgdatadir, which
// on Windows is the CI build machine's own absolute install prefix
// (e.g. D:\a\_temp\msys64\ucrt64\share\openmortal) -- it doesn't exist
// on the end user's machine. Confirmed via Process Monitor across all
// three Windows toolchains (mingw64, ucrt64, clang64): Backend.cpp's
// chdir() into "<that path>\script" failed with
// STATUS_OBJECT_PATH_NOT_FOUND, cascading into "couldn't start
// backend." On Windows the binary is relocatable instead: g_szDataDir
// is filled in once at startup (main.cpp's init_data_dir(), via
// GetModuleFileName) from the exe's own location, so the game works
// wherever it's installed. On all other platforms MSZ_DATADIR stays
// the compile-time constant Makefile.am sets via -DMSZ_DATADIR=...
#ifdef _WIN32
extern char g_szDataDir[];
#undef MSZ_DATADIR
#define MSZ_DATADIR ((const char*)g_szDataDir)
#endif

// -----------------------------------------------------------------------
// Main program methods
// -----------------------------------------------------------------------

void DoMenu();
void GameOver( int a_iPlayerWon );
void DoDemos();
int  DoGame( char* replay, bool isReplay, bool bDebug );
void DoOnlineChat();

// -----------------------------------------------------------------------
// Other subroutines
// -----------------------------------------------------------------------

bool Connect( const char* a_pcHostname );

const char* Translate( const char* a_pcText );
const char* TranslateUTF8( const char* a_pcText );

// -----------------------------------------------------------------------
// Global variables
// -----------------------------------------------------------------------

struct SDL_Surface;
extern SDL_Surface* gamescreen;

extern Uint32 C_BLACK;
extern Uint32 C_BLUE;
extern Uint32 C_GREEN;
extern Uint32 C_CYAN;

extern Uint32 C_RED;
extern Uint32 C_MAGENTA;
extern Uint32 C_ORANGE;
extern Uint32 C_LIGHTGRAY;

extern Uint32 C_DARKGRAY;
extern Uint32 C_LIGHTBLUE;
extern Uint32 C_LIGHTGREEN;
extern Uint32 C_LIGHTCYAN;

extern Uint32 C_LIGHTRED;
extern Uint32 C_LIGHTMAGENTA;
extern Uint32 C_YELLOW;
extern Uint32 C_WHITE;


#endif

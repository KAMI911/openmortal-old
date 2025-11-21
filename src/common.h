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

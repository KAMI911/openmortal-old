/***************************************************************************
                           Jukebox.cpp  -  description
                             -------------------
    begin                : 2026
    copyright            : (C) 2026 OpenMortal contributors
 ***************************************************************************/

#include "Jukebox.h"
#include "Audio.h"
#include "State.h"
#include "common.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

#include <cstdlib>
#include <cstring>
#include <cctype>
#include <algorithm>

// Static member definitions
std::vector<std::string> Jukebox::m_aoTracks[13];
std::vector<std::string> Jukebox::m_aoAllGameTracks;


static bool IsMusicFile( const char* a_pcName )
{
    const char* pcDot = strrchr( a_pcName, '.' );
    if ( !pcDot ) return false;
    char acExt[8] = {0};
    for ( int i = 0; i < 7 && pcDot[i+1]; ++i )
        acExt[i] = (char)tolower( (unsigned char)pcDot[i+1] );
    return strcmp(acExt, "mod") == 0
        || strcmp(acExt, "s3m") == 0
        || strcmp(acExt, "xm")  == 0
        || strcmp(acExt, "it")  == 0;
}


void Jukebox::ScanMusicDir()
{
    std::string sMusicDir = OM_DATADIR;
    sMusicDir += "/sound/music";

#ifdef _WIN32
    std::string sPattern = sMusicDir + "\\*";
    WIN32_FIND_DATA oFindData;
    HANDLE hFind = FindFirstFile( sPattern.c_str(), &oFindData );
    if ( INVALID_HANDLE_VALUE == hFind )
    {
        debug( "Jukebox: could not open music dir: %s\n", sMusicDir.c_str() );
        return;
    }
    do
    {
        const char* pcName = oFindData.cFileName;
        if ( strlen(pcName) > 3
             && isdigit((unsigned char)pcName[0])
             && isdigit((unsigned char)pcName[1])
             && pcName[2] == '_'
             && IsMusicFile(pcName) )
        {
            int iLevel = (pcName[0]-'0') * 10 + (pcName[1]-'0');
            if ( iLevel >= 0 && iLevel <= 12 )
            {
                std::string sRelPath = std::string("music/") + pcName;
                m_aoTracks[iLevel].push_back( sRelPath );
                if ( iLevel > 0 )
                    m_aoAllGameTracks.push_back( sRelPath );
            }
        }
    }
    while ( FindNextFile( hFind, &oFindData ) );
    FindClose( hFind );
#else
    DIR* poDir = opendir( sMusicDir.c_str() );
    if ( !poDir )
    {
        debug( "Jukebox: could not open music dir: %s\n", sMusicDir.c_str() );
        return;
    }
    struct dirent* poEntry;
    while ( (poEntry = readdir(poDir)) != NULL )
    {
        const char* pcName = poEntry->d_name;
        if ( strlen(pcName) > 3
             && isdigit((unsigned char)pcName[0])
             && isdigit((unsigned char)pcName[1])
             && pcName[2] == '_'
             && IsMusicFile(pcName) )
        {
            int iLevel = (pcName[0]-'0') * 10 + (pcName[1]-'0');
            if ( iLevel >= 0 && iLevel <= 12 )
            {
                std::string sRelPath = std::string("music/") + pcName;
                m_aoTracks[iLevel].push_back( sRelPath );
                if ( iLevel > 0 )
                    m_aoAllGameTracks.push_back( sRelPath );
            }
        }
    }
    closedir( poDir );
#endif

    // Sort each list for deterministic ordering before random selection
    for ( int i = 0; i <= 12; ++i )
        std::sort( m_aoTracks[i].begin(), m_aoTracks[i].end() );
    std::sort( m_aoAllGameTracks.begin(), m_aoAllGameTracks.end() );

    debug( "Jukebox: %d menu tracks, %d game tracks total\n",
           (int)m_aoTracks[0].size(), (int)m_aoAllGameTracks.size() );
}


void Jukebox::Init()
{
    // Clear any previous state
    for ( int i = 0; i <= 12; ++i )
        m_aoTracks[i].clear();
    m_aoAllGameTracks.clear();

    ScanMusicDir();
}


std::string Jukebox::PickRandom( const std::vector<std::string>& a_roList )
{
    if ( a_roList.empty() ) return "";
    return a_roList[ rand() % a_roList.size() ];
}


void Jukebox::LoadAndPlay( const std::string& a_rsRelPath )
{
    if ( a_rsRelPath.empty() ) return;

    // Swap out the single jukebox slot
    Audio->UnloadMusic( "JukeboxCurrent" );
    Audio->LoadMusic( a_rsRelPath.c_str(), "JukeboxCurrent" );
    Audio->PlayMusic( "JukeboxCurrent" );
}


void Jukebox::PlayMenuMusic()
{
    if ( g_oState.m_enMusicMode == SState::Music_OFF )
    {
        Audio->StopMusic();
        return;
    }
    std::string sTrack = PickRandom( m_aoTracks[0] );
    if ( sTrack.empty() )
    {
        debug( "Jukebox: no menu tracks (00_*) found\n" );
        return;
    }
    LoadAndPlay( sTrack );
}


void Jukebox::PlayLevelMusic( int a_iLevel )
{
    if ( g_oState.m_enMusicMode == SState::Music_OFF )
    {
        Audio->StopMusic();
        return;
    }
    if ( g_oState.m_enMusicMode == SState::Music_RANDOM )
    {
        PlayRandom();
        return;
    }
    // Music_ORIGINAL: use the level-specific list, fall back to random
    if ( a_iLevel < 1 ) a_iLevel = 1;
    if ( a_iLevel > 12 ) a_iLevel = 12;

    std::string sTrack = PickRandom( m_aoTracks[a_iLevel] );
    if ( sTrack.empty() )
    {
        debug( "Jukebox: no tracks for level %d, falling back to random\n", a_iLevel );
        PlayRandom();
        return;
    }
    LoadAndPlay( sTrack );
}


void Jukebox::PlayRandom()
{
    if ( g_oState.m_enMusicMode == SState::Music_OFF )
    {
        Audio->StopMusic();
        return;
    }
    std::string sTrack = PickRandom( m_aoAllGameTracks );
    if ( sTrack.empty() )
    {
        debug( "Jukebox: no game tracks found\n" );
        return;
    }
    LoadAndPlay( sTrack );
}


void Jukebox::Stop()
{
    Audio->StopMusic();
}

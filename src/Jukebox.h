/***************************************************************************
                           Jukebox.h  -  description
                             -------------------
    begin                : 2026
    copyright            : (C) 2026 OpenMortal contributors
 ***************************************************************************/

#ifndef JUKEBOX_H
#define JUKEBOX_H

#include <vector>
#include <string>

/**
\class Jukebox
\brief Music jukebox for OpenMortal.
\ingroup Media

Scans DATADIR/sound/music/ at startup and groups tracker module files by
their numeric prefix:
  - 00_*.mod/s3m/xm  ->  menu music  (played during demo/lobby)
  - NN_*.mod/s3m/xm  ->  level music for level N  (N = 01..12)

Supports three modes controlled by SState::m_enMusicMode:
  - Music_ORIGINAL : menu gets 00_ tracks; each level gets its own NN_ track
  - Music_RANDOM   : menu gets 00_ tracks; game gets a random non-00_ track
  - Music_OFF      : no music

All play methods are no-ops when mode is Music_OFF or no files are found.
*/

class Jukebox
{
public:
    /// Scan the music directory and populate internal track lists.
    static void Init();

    /// Play a random menu track (00_ prefix). No-op if mode is Music_OFF.
    static void PlayMenuMusic();

    /// Play a random track assigned to the given level (1-12).
    /// Falls back to PlayRandom() if no tracks exist for that level.
    /// No-op if mode is Music_OFF.
    static void PlayLevelMusic( int a_iLevel );

    /// Play a random track from all non-00_ tracks.
    /// No-op if mode is Music_OFF.
    static void PlayRandom();

    /// Stop music immediately (used when switching to Music_OFF).
    static void Stop();

private:
    static void ScanMusicDir();
    static std::string PickRandom( const std::vector<std::string>& a_roList );
    static void LoadAndPlay( const std::string& a_rsRelPath );

    // m_aoTracks[0]    = menu tracks  (00_ prefix)
    // m_aoTracks[1..12] = per-level tracks
    static std::vector<std::string> m_aoTracks[13];
    static std::vector<std::string> m_aoAllGameTracks; // all non-00_ tracks (for random mode)
};

#endif // JUKEBOX_H

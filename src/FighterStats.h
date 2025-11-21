/***************************************************************************
                          FighterStats.h  -  description
                             -------------------
    begin                : Sun Jan 25 2004
    copyright            : (C) 2004 by upi
    email                : upi@feel
 ***************************************************************************/

#ifndef FIGHTERSTATS_H
#define FIGHTERSTATS_H
 
#include "Demo.h"
#include <string>


/**
\class CFighterStatDemo
\ingroup Demo
\brief This is a specialized CDemo which displays the stats of a fighter.

The fighter is either random, or the winner of the last game.
*/

class FighterStatsDemo: public Demo
{
public:
	FighterStatsDemo( FighterEnum a_iFighter = UNKNOWN );
	virtual ~FighterStatsDemo();

	int Advance( int a_iNumFrames, bool a_bFlip );

protected:
	int				m_iTimeLeft;
	FighterEnum		m_enFighter;
	RlePack*		m_poStaff;
	std::string		m_sKeys;
	std::string		m_sStory;

	static int mg_iLastFighter;		// index of the last fighter in the fighter order
	static FighterEnum mg_aenFighterOrder[LASTFIGHTER-1];
};

#endif // FIGHTERSTATS_H

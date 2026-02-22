/***************************************************************************
                          FighterEnum.h  -  description
                             -------------------
    begin                : 2003-09-03
    copyright            : (C) 2003 by upi
    email                : upi@apocalypse.rulez.org
 ***************************************************************************/


#ifndef FIGHTERENUM_H
#define FIGHTERENUM_H


/// FIGHTER ENUMERABLE

/**
\ingroup GameLogic
*/
enum FighterEnum {
	UNKNOWN = 0,		///< Must be the first element, must be 0 (used by iterations)
	
	ULMAR,
	UPI,
	ZOLI,
	CUMI,
	SIRPI,
	MACI,
	BENCE,
	GRIZLI,
	DESCANT,
	SURBA,
	AMBRUS,
	DANI,
	KINGA,
	MISI,
	
	LASTFIGHTER,		///< This must terminate the list, iterations use it.
};





/** The TintEnum contains values that can be passed to RlePack::SetTint.

The tint is some modification of the original palette of an RlePack. This
is used for two things: 

\li In case both players choose the same fighter, player 2's fighter is
tinted so they won't get confused.
\li Some special effects (e.g. frozen) make the fighter tinted as well.

The Tint of players is stored by PlayerSelect and applied by RlePack::SetTint().
*/

enum TintEnum {
	NO_TINT = 0,
	ZOMBIE_TINT,
	GRAY_TINT,
	DARK_TINT,
	INVERTED_TINT,
	FROZEN_TINT,
};

#endif // FIGHTERENUM_H

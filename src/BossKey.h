/***************************************************************************
                         BossKey.h  -  description
                            -------------------
   begin                : 2025
   copyright            : (C) 2025 OpenMortal contributors
***************************************************************************/

#ifndef MSZ_BOSS_KEY_H
#define MSZ_BOSS_KEY_H

/**
\defgroup BossKey Boss Key (Shift+Esc)

Pressing Shift+Esc overlays a fake Microsoft Excel spreadsheet on screen
and silences music until any key is pressed.  The feature is compiled in
by default and can be removed with ./configure --disable-boss-key.
*/

#ifdef MSZ_BOSS_KEY
void ShowBossKey();
#endif

#endif // MSZ_BOSS_KEY_H

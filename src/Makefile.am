## Process this file with automake to produce Makefile.in

bin_PROGRAMS = openmortal
openmortal_SOURCES = \
	Audio.cpp         FlyingChars.cpp  MortalNetworkImpl.cpp       sge_primitives.cpp \
	Backend.cpp       Game.cpp         OnlineChat.cpp              sge_surface.cpp \
	Background.cpp    GameOver.cpp     PlayerSelectController.cpp  sge_tt_text.cpp \
	Chooser.cpp       gfx.cpp          PlayerSelect.cpp            State.cpp \
	common.cpp        Joystick.cpp     PlayerSelectView.cpp        TextArea.cpp \
	Demo.cpp          main.cpp         RlePack.cpp \
	FighterStats.cpp  menu.cpp         sge_bm_text.cpp

EXTRA_DIST = $(openmortal_SOURCES)\
	Audio.h       Event.h         menu.h                    PlayerSelectView.h  sge_tt_text.h \
	Backend.h     FighterEnum.h   MortalNetwork.h           RlePack.h           State.h \
	Background.h  FighterStats.h  MortalNetworkImpl.h       sge_bm_text.h       TextArea.h \
	Chooser.h     FlyingChars.h   MszPerl.h                 sge_config.h \
	common.h      Game.h          OnlineChat.h              sge_internal.h \
	Demo.h        gfx.h           PlayerSelectController.h  sge_primitives.h \
	DrawRle.h     Joystick.h      PlayerSelect.h            sge_surface.h

CXXFLAGS= @CXXFLAGS@ -DDATADIR=\"${pkgdatadir}\" -Wall

# set the include path found by configure
#INCLUDES= $(all_includes)
 
# the library search path.
#msz_LDFLAGS = $(all_libraries) 
                                                                                

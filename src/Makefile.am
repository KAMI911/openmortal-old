## Process this file with automake to produce Makefile.in

bin_PROGRAMS = openmortal
openmortal_SOURCES = \
	AnimBG.cpp        FlyingChars.cpp  MortalNetworkImpl.cpp       sge_primitives.cpp \
	Audio.cpp         Game.cpp         OnlineChat.cpp              sge_surface.cpp \
	Backend.cpp       GameOver.cpp     PlayerSelectController.cpp  sge_tt_text.cpp \
	Background.cpp    gfx.cpp          PlayerSelect.cpp            State.cpp \
	Chooser.cpp       Joystick.cpp     PlayerSelectView.cpp        TextArea.cpp \
	common.cpp        main.cpp         RlePack.cpp \
	Demo.cpp          menu.cpp         sge_bm_text.cpp \
	FighterStats.cpp

EXTRA_DIST = $(openmortal_SOURCES)\
	AnimBG.h      Event.h         menu.h                    PlayerSelectView.h  sge_tt_text.h \
	Audio.h       FighterEnum.h   MortalNetwork.h           RlePack.h           State.h \
	Backend.h     FighterStats.h  MortalNetworkImpl.h       sge_bm_text.h       TextArea.h \
	Background.h  FlyingChars.h   MszPerl.h                 sge_config.h \
	Chooser.h     Game.h          OnlineChat.h              sge_internal.h \
	common.h      gfx.h           PlayerSelectController.h  sge_primitives.h \
	Demo.h        DrawRle.h       Joystick.h                PlayerSelect.h      sge_surface.h

CXXFLAGS= @CXXFLAGS@ -DDATADIR=\"${pkgdatadir}\" -Wall

# set the include path found by configure
#INCLUDES= $(all_includes)
 
# the library search path.
#msz_LDFLAGS = $(all_libraries) 
                                                                                

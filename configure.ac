dnl without this order in this file, autoconf will not work!
dnl the argument is a source file out of your sources. But
dnl acinclude.m4 makes the job for all programs ;-)
AC_INIT(acinclude.m4)

dnl enable the following if you want to use autoconf/automake 
dnl framework from a certain directory (like kde-common)
dnl AC_CONFIG_AUX_DIR(admin)

AC_CANONICAL_SYSTEM
AC_ARG_PROGRAM

AM_INIT_AUTOMAKE(openmortal, 0.7)

AC_PREFIX_DEFAULT(/usr/local)
if test "x$prefix" = "xNONE"; then
  prefix=$ac_default_prefix
  ac_configure_args="$ac_configure_args --prefix $prefix"
fi

dnl without this order in this file, automake will be confused!
dnl 
AM_CONFIG_HEADER(config.h)

AC_C_CONST
AC_PROG_CXX

PKG_PROG_PKG_CONFIG

dnl
dnl Switches
dnl These allow packagers to "trim" openmortal, and split it into smaller packages.
dnl
                                                                                   
AC_ARG_ENABLE(main,
[  --enable-main           Include main data and binary [default: yes]],
[case "${enableval}" in
  yes) main=true ;;
  no)  main=false ;;
  *) AC_MSG_ERROR(bad value ${enableval} for --enable-main) ;;
esac],[main=true])
AM_CONDITIONAL(MAIN, test x$main = xtrue)

AC_ARG_ENABLE(characters,
[  --enable-characters     Include the first batch of characters [default: yes]],
[case "${enableval}" in
  yes) characters=true ;;
  no)  characters=false ;;
  *) AC_MSG_ERROR(bad value ${enableval} for --enable-characters) ;;
esac],[characters=true])
AM_CONDITIONAL(CHARACTERS, test x$characters = xtrue)

AC_ARG_ENABLE(additional,
[  --enable-additional     Include the second batch of characters [default: yes]],
[case "${enableval}" in
  yes) additional=true ;;
  no)  additional=false ;;
  *) AC_MSG_ERROR(bad value ${enableval} for --enable-additional) ;;
esac],[additional=true])
AM_CONDITIONAL(ADDITIONAL, test x$additional = xtrue)

AC_ARG_ENABLE(thirdparty,
[  --enable-thirdparty     Include the 3rd party characters [default: yes]],
[case "${enableval}" in
  yes) thirdparty=true ;;
  no)  thirdparty=false ;;
  *) AC_MSG_ERROR(bad value ${enableval} for --enable-thirdparty) ;;
esac],[thirdparty=true])
AM_CONDITIONAL(THIRDPARTY, test x$thirdparty = xtrue)





dnl checks for programs.
dnl first check for c/c++ compilers
dnl AC_CHECK_COMPILERS

dnl create only shared libtool-libraries
dnl AC_ENABLE_SHARED(yes)

dnl set the following to yes, if you want to create static
dnl libtool-libraries
dnl AC_ENABLE_STATIC(no)

AC_SUBST(AUTODIRS)

dnl in this space add everything you want to check for
dnl examples are specific headers, libraries, etc.

dnl Check for SDL
SDL_VERSION=1.2.0
AM_PATH_SDL($SDL_VERSION,
	:,
	AC_MSG_ERROR([*** SDL version $SDL_VERSION not found!])
)
CXXFLAGS="$CXXFLAGS $SDL_CFLAGS"
LIBS="$LIBS $SDL_LIBS"

dnl Check for SDL_image

AC_CHECK_LIB(SDL_image,
	IMG_Load,
	LIBS="$LIBS -lSDL_image",
	AC_MSG_ERROR([*** SDL_image library not found!])
)

dnl Check for SDL_mixer

AC_CHECK_LIB(SDL_mixer,
	Mix_OpenAudio,
	LIBS="$LIBS -lSDL_mixer",
	AC_MSG_ERROR([*** SDL_mixer library not found!])
)

dnl Check for SDL_net

AC_CHECK_LIB(SDL_net,
	SDLNet_ResolveHost,
	LIBS="$LIBS -lSDL_net",
	AC_MSG_ERROR([*** SDL_net library not found!])
)

dnl Check for Freetype 2 library

PKG_CHECK_MODULES(FT2, freetype2,,
	AC_MSG_ERROR([*** FreeType2 not found!])
)
CXXFLAGS="$CXXFLAGS $FT2_CFLAGS"
LIBS="$LIBS $FT2_LIBS"

dnl Perl-related stuff...

OMAC_PERL_EMBED_FLAGS
CXXFLAGS="$CXXFLAGS $perl_embed_ccflags"
LIBS="$LIBS $perl_embed_ldflags"

dnl perform program name transformation
AC_ARG_PROGRAM

dnl add here all your Makefiles. These will be created by configure
AC_OUTPUT(Makefile src/Makefile
data/Makefile
data/characters/Makefile
data/fonts/Makefile 
data/gfx/Makefile 
data/script/Makefile 
data/sound/Makefile)

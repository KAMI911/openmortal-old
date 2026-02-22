/***************************************************************************
                          MszPerl.h  -  description
                             -------------------
    begin                : Mon Jan 5 2004
    copyright            : (C) 2004 by upi
    email                : upi@feel
 ***************************************************************************/

/**
\file MszPerl.h
\ingroup GameLogic

This file helps include perl.h
The windows version of perl.h contains a lot of stupid #defines, this
file helps undefine them.
*/
 
#ifndef __MSZPERL_H
#define __MSZPERL_H

#include <EXTERN.h>
#include <perl.h>

#if defined(_WIN32) || defined(WIN32) || defined(_WINDOWS)
#undef bool
#undef chdir
#undef close
#undef open
#undef read
#undef write
#undef eof

#define vsnprintf _vsnprintf
#endif

#endif  // __MSZPERL_H

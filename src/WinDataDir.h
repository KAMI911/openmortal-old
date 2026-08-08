#ifndef WINDATADIR_H
#define WINDATADIR_H

#if defined(_WIN32) || defined(WIN32) || defined(_WINDOWS)

#include <string>

/* MSZ_DATADIR normally compiles in the autoconf-time pkgdatadir, which
 * on Windows is the CI build machine's own absolute install prefix
 * (e.g. D:\a\_temp\msys64\ucrt64\share\openmortal) -- it doesn't exist
 * on the end user's machine. Confirmed via Process Monitor across all
 * three Windows toolchains (mingw64, ucrt64, clang64): Backend.cpp's
 * chdir() into "<that path>\script" failed with
 * STATUS_OBJECT_PATH_NOT_FOUND, which made every later relative Perl
 * -I/script lookup resolve from the wrong (unchanged) cwd too --
 * cascading into "couldn't start backend."
 *
 * A literal relative override (e.g. "../share/openmortal") isn't
 * enough either: Backend::Construct() permanently chdir()s the process
 * into share/openmortal/script for the rest of the run (Perl's own
 * relative opens, e.g. DataHelper.pl's "../characters/...", depend on
 * that cwd staying put), so anything computed relative to the
 * *original* cwd breaks for every MSZ_DATADIR use that happens after
 * Backend::Construct() runs -- which is nearly all of them, since
 * init2() runs first in main().
 *
 * Resolve the data dir once, as an absolute path relative to the exe's
 * own location, so it stays correct no matter what the cwd is at the
 * time of use. Implemented in WinMainWin32.cpp, the only file that can
 * safely include windows.h -- see the comment there.
 */
extern std::string GetOpenMortalDataDir();

#undef MSZ_DATADIR
#define MSZ_DATADIR GetOpenMortalDataDir().c_str()

#endif

#endif

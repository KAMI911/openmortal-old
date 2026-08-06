/* Kept in its own translation unit, deliberately isolated from SDL.h and
 * perl.h: sdl-config passes -Dmain=SDL_main (rewriting every bare "main"
 * token), and Perl's own headers pull in windows.h internally with a huge
 * amount of macro redefinition. Mixing either of those with a direct
 * <windows.h> include in the same file breaks the build in ways that are
 * miserable to untangle -- see git history for the failed attempts.
 *
 * SDL's prebuilt Windows entry point (SDLmain, which we no longer link
 * against -- see configure.ac) unconditionally calls freopen() on
 * stdout.txt/stderr.txt next to the exe before the app gets control. When
 * installed somewhere the current user can't write to (e.g. Program Files
 * without elevation), that freopen() fails and the very next CRT stdio
 * call crashes with an access violation deep in ntdll.dll -- confirmed via
 * Process Monitor on real hardware. We provide our own WinMain and
 * redirect to a location that's always writable, regardless of install
 * location or privilege level.
 */
#ifdef _WIN32

/* Makefile.am's CXXFLAGS defines DATADIR globally for every .cpp file
 * (the game's own data directory path) -- but windows.h's objidl.h
 * declares an unrelated enum type also named DATADIR, so our macro
 * clobbers it into invalid syntax the moment windows.h is included.
 * main.cpp has carried its own "GRRR.. windows keyword..." #undef for
 * this since the DOS-era codebase; this file doesn't use DATADIR at all,
 * so just drop it before pulling windows.h in. */
#undef DATADIR

#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <string.h>
#include <string>

/* Not extern "C" -- main.cpp names its entry point SDL_main directly on
 * Windows and keeps ordinary C++ linkage, so this declaration must match
 * that (SDL_main.h itself declares it the same way). */
extern int SDL_main(int argc, char *argv[]);

static void RedirectStdioToAppData()
{
	char szPath[MAX_PATH];
	if ( FAILED( SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, szPath) ) )
		return;

	strncat(szPath, "\\OpenMortal", MAX_PATH - strlen(szPath) - 1);
	CreateDirectoryA(szPath, NULL);

	std::string szOut = std::string(szPath) + "\\stdout.txt";
	std::string szErr = std::string(szPath) + "\\stderr.txt";
	freopen(szOut.c_str(), "w", stdout);
	freopen(szErr.c_str(), "w", stderr);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	RedirectStdioToAppData();
	return SDL_main(__argc, __argv);
}

#endif

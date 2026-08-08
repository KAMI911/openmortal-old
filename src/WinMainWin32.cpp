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

#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <string.h>
#include <string>

/* extern "C" is required here, confirmed via nm on a real build: main.cpp
 * includes SDL.h, which pulls in SDL_main.h -- and like every C library
 * header, SDL wraps its declarations in extern "C" for C++ callers. That
 * earlier extern "C" declaration of SDL_main makes main.cpp's actual
 * definition inherit C linkage too (a linkage-specification, once
 * established for a name, sticks for all following declarations/
 * definitions of it), producing an unmangled "SDL_main" symbol. This file
 * never sees SDL_main.h, so without this, our own declaration would get
 * ordinary mangled C++ linkage instead -- a mismatch the linker reports
 * as an undefined symbol despite the function very much existing. */
extern "C" int SDL_main(int argc, char *argv[]);

/* %LOCALAPPDATA%\OpenMortal -- always writable regardless of install
 * location or privilege level, unlike next to the exe (Program Files).
 * Shared with State.cpp (openmortal.ini) via GetOpenMortalAppDataDir()
 * below: that file can't include <windows.h> itself for the same reason
 * this one is a separate translation unit in the first place (see the
 * file-level comment above).
 */
std::string GetOpenMortalAppDataDir()
{
	char szPath[MAX_PATH];
	if ( FAILED( SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, szPath) ) )
		return std::string();

	strncat(szPath, "\\OpenMortal", MAX_PATH - strlen(szPath) - 1);
	CreateDirectoryA(szPath, NULL);
	return std::string(szPath);
}

static void RedirectStdioToAppData()
{
	std::string szPath = GetOpenMortalAppDataDir();
	if ( szPath.empty() )
		return;

	std::string szOut = szPath + "\\stdout.txt";
	std::string szErr = szPath + "\\stderr.txt";
	freopen(szOut.c_str(), "w", stdout);
	freopen(szErr.c_str(), "w", stderr);
}

/* Resolves MSZ_DATADIR (see WinDataDir.h) to an absolute path relative
 * to the exe's own location: {app}\bin\openmortal.exe -> {app}\share\
 * openmortal, matching the packaging layout (share\ is a sibling of
 * bin\ in both the portable tarball and the InnoSetup install -- see
 * unified-build.yml's "Collect DLLs and package" step and
 * packaging/openmortal.iss). Cached after the first call since the
 * exe's own location never changes at runtime, regardless of what the
 * process cwd gets chdir()'d to later (see Backend.cpp).
 */
std::string GetOpenMortalDataDir()
{
	static std::string s_sDataDir;
	if ( !s_sDataDir.empty() )
		return s_sDataDir;

	char szPath[MAX_PATH];
	GetModuleFileNameA( NULL, szPath, MAX_PATH );

	std::string sExePath( szPath );
	size_t iSlash = sExePath.find_last_of( '\\' );
	std::string sBinDir = ( iSlash != std::string::npos ) ? sExePath.substr( 0, iSlash ) : ".";

	s_sDataDir = sBinDir + "\\..\\share\\openmortal";
	return s_sDataDir;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	RedirectStdioToAppData();
	return SDL_main(__argc, __argv);
}

#endif

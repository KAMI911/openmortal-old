; OpenMortal Windows Installer Script
; Requires Inno Setup 6.x  —  https://jrsoftware.org/isinfo.php
;
; Typical invocation from CI (PowerShell):
;
;   iscc `
;     "/DSourceDir=C:\path\to\windows-x86_64_package\mingw64" `
;     "/DAppArch=x86_64" `
;     "packaging\openmortal.iss"
;
; SourceDir must contain the staged install tree, i.e.:
;   bin\openmortal.exe   – game executable
;   bin\*.dll            – bundled runtime DLLs
;   share\openmortal\*   – game data (characters, gfx, fonts, sound, script)
;
; The finished setup EXE is written to the installer\ folder next to this
; script (packaging\installer\openmortal-<ver>-windows-<arch>-setup.exe).
;
; ─────────────────────────────────────────────────────────────────────────────

; ── Preprocessor defines (override with /D on the command line) ──────────────

#ifndef SourceDir
  #define SourceDir "."
#endif

#ifndef AppArch
  #define AppArch "x86_64"
#endif

#define AppName        "OpenMortal"
#define AppVersion     "0.7.2"
#define AppPublisher   "OpenMortal Team"
#define AppURL         "https://openmortal.sourceforge.net/"
#define AppExeName     "openmortal.exe"
#define AppDescription "Open Mortal — a humorous parody of Mortal Kombat"

; ── [Setup] ──────────────────────────────────────────────────────────────────

[Setup]
; IMPORTANT: keep AppId constant across all future releases so the Windows
; installer can find and upgrade an existing installation.
AppId={{D4F8E7A3-9CB5-4CC2-BD1F-0A3D5C8B2E6F}

AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}
AppUpdatesURL={#AppURL}

; Default install locations
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
AllowNoIcons=yes

; 64-bit builds install into 64-bit Program Files; 32-bit builds use the
; 32-bit location automatically because autopf respects the installer arch.
ArchitecturesInstallIn64BitMode=x64compatible arm64

; Don't require admin rights. SDL's Windows entry point (SDLmain) redirects
; stdout/stderr to stdout.txt/stderr.txt next to the exe before our own
; main() ever runs; when installed to Program Files (default, admin-write-
; protected) that freopen() fails with ACCESS_DENIED, and the very next
; CRT stdio call in the app crashes with an access violation deep in
; ntdll.dll — reproduced 100% of the time on real Windows via Process
; Monitor. With PrivilegesRequired=lowest, {autopf} above resolves to
; %LOCALAPPDATA%\Programs instead of Program Files, which is writable, so
; the redirect succeeds and the crash never happens. Users who still want
; a machine-wide install can elevate via the privileges dialog.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog

; Output
; NOTE: relative to this script's own folder (packaging\), not SourceDir —
; SourceDir points at the per-arch staged package tree, which doesn't
; contain openmortal.ico or the installer\ output folder.
OutputDir={#SourcePath}installer
OutputBaseFilename=openmortal-{#AppVersion}-windows-{#AppArch}-setup

; Visuals
SetupIconFile={#SourcePath}openmortal.ico
WizardStyle=modern
WizardResizable=yes

; Compression
Compression=lzma2/ultra64
SolidCompression=yes

; Uninstall
UninstallDisplayIcon={app}\bin\{#AppExeName}
UninstallDisplayName={#AppName} {#AppVersion}

; Version metadata embedded in the installer EXE
VersionInfoVersion={#AppVersion}.0
VersionInfoCompany={#AppPublisher}
VersionInfoDescription={#AppName} {#AppVersion} Installer
VersionInfoProductName={#AppName}
VersionInfoProductVersion={#AppVersion}.0
VersionInfoCopyright=Copyright (C) 2003-2024 OpenMortal Team

; ── [Languages] ──────────────────────────────────────────────────────────────

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

; ── [Tasks] ──────────────────────────────────────────────────────────────────

[Tasks]
Name: "desktopicon"; \
  Description: "{cm:CreateDesktopIcon}"; \
  GroupDescription: "{cm:AdditionalIcons}"; \
  Flags: unchecked

; ── [Files] ──────────────────────────────────────────────────────────────────

[Files]
; Game executable and data go in bin\ / share\openmortal\, matching the
; portable tarball layout exactly — init_data_dir() in main.cpp locates
; game data at "<exe dir>\..\share\openmortal" (stepping up one level
; when the exe's own directory is literally named "bin"). Installing
; straight into {app}\ with data under {app}\data (the previous layout)
; put data somewhere the exe never looks, so it started but couldn't
; find anything and silently failed.
Source: "{#SourceDir}\bin\{#AppExeName}"; \
  DestDir: "{app}\bin"; \
  Flags: ignoreversion

; Bundled runtime DLLs (SDL, FreeType, Perl, etc.)
Source: "{#SourceDir}\bin\*.dll"; \
  DestDir: "{app}\bin"; \
  Flags: ignoreversion

; Game data — characters, graphics, fonts, sounds, scripts
Source: "{#SourceDir}\share\openmortal\*"; \
  DestDir: "{app}\share\openmortal"; \
  Flags: ignoreversion recursesubdirs createallsubdirs

; Embedded Perl's own standard library (core_perl etc.) — its compiled-in
; @INC expects this as a sibling of bin\, same convention as share\ above.
; Without it perl_parse() can't find its own bootstrap modules and the
; process crashes instead of failing gracefully. Optional flag: skipped
; for legs where the source tree has none (e.g. if ever absent).
Source: "{#SourceDir}\lib\perl5\*"; \
  DestDir: "{app}\lib\perl5"; \
  Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

; ── [Icons] / Start Menu ─────────────────────────────────────────────────────

[Icons]
; Start Menu entry
Name: "{group}\{#AppName}"; \
  Filename: "{app}\bin\{#AppExeName}"; \
  WorkingDir: "{app}\bin"; \
  Comment: "{#AppDescription}"

; Uninstall entry in Start Menu
Name: "{group}\{cm:UninstallProgram,{#AppName}}"; \
  Filename: "{uninstallexe}"

; Optional desktop shortcut (unchecked by default — see [Tasks])
Name: "{autodesktop}\{#AppName}"; \
  Filename: "{app}\bin\{#AppExeName}"; \
  WorkingDir: "{app}\bin"; \
  Comment: "{#AppDescription}"; \
  Tasks: desktopicon

; ── [Run] — post-install launch offer ────────────────────────────────────────

[Run]
Filename: "{app}\bin\{#AppExeName}"; \
  Description: "{cm:LaunchProgram,{#StringChange(AppName, '&', '&&')}}"; \
  WorkingDir: "{app}\bin"; \
  Flags: nowait postinstall skipifsilent

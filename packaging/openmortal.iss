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
ArchitecturesInstallIn64BitMode=x64compatible arm64compatible

; Output
OutputDir={#SourceDir}\..\installer
OutputBaseFilename=openmortal-{#AppVersion}-windows-{#AppArch}-setup

; Visuals
SetupIconFile={#SourceDir}\..\openmortal.ico
WizardStyle=modern
WizardResizable=yes

; Compression
Compression=lzma2/ultra64
SolidCompression=yes

; Uninstall
UninstallDisplayIcon={app}\{#AppExeName}
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
; Game executable
Source: "{#SourceDir}\bin\{#AppExeName}"; \
  DestDir: "{app}"; \
  Flags: ignoreversion

; Bundled runtime DLLs (SDL, FreeType, Perl, etc.)
Source: "{#SourceDir}\bin\*.dll"; \
  DestDir: "{app}"; \
  Flags: ignoreversion

; Game data — characters, graphics, fonts, sounds, scripts
Source: "{#SourceDir}\share\openmortal\*"; \
  DestDir: "{app}\data"; \
  Flags: ignoreversion recursesubdirs createallsubdirs

; ── [Icons] / Start Menu ─────────────────────────────────────────────────────

[Icons]
; Start Menu entry
Name: "{group}\{#AppName}"; \
  Filename: "{app}\{#AppExeName}"; \
  WorkingDir: "{app}"; \
  Comment: "{#AppDescription}"

; Uninstall entry in Start Menu
Name: "{group}\{cm:UninstallProgram,{#AppName}}"; \
  Filename: "{uninstallexe}"

; Optional desktop shortcut (unchecked by default — see [Tasks])
Name: "{autodesktop}\{#AppName}"; \
  Filename: "{app}\{#AppExeName}"; \
  WorkingDir: "{app}"; \
  Comment: "{#AppDescription}"; \
  Tasks: desktopicon

; ── [Run] — post-install launch offer ────────────────────────────────────────

[Run]
Filename: "{app}\{#AppExeName}"; \
  Description: "{cm:LaunchProgram,{#StringChange(AppName, '&', '&&')}}"; \
  WorkingDir: "{app}"; \
  Flags: nowait postinstall skipifsilent

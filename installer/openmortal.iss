; OpenMortal Windows Installer Script
; Built automatically by GitHub Actions using Inno Setup 6
;
; IMPORTANT — fixed install location:
;   The binary is compiled with --prefix=/OpenMortal so it looks for game
;   data at /OpenMortal/share/openmortal/ at runtime.  On Windows a path
;   that starts with a single slash is rooted at the current drive, so the
;   game expects its data under  <SystemDrive>\OpenMortal\share\openmortal\.
;   The install directory is therefore fixed to {sd}\OpenMortal so that the
;   hardcoded path always resolves correctly regardless of the drive letter.
;
; To make the installer truly relocatable the source would need a runtime
; path-detection mechanism (e.g. GetModuleFileName).  That is left as a
; future improvement.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "dist"
#endif

#define AppName    "OpenMortal"
#define AppExeName "openmortal.exe"
#define AppURL     "https://github.com/KAMI911/openmortal-old"
#define AppContact "https://github.com/KAMI911/openmortal-old/issues"

[Setup]
AppId={{6F3A2D9E-4B71-4C08-8E5F-1D9A7C3B0F42}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=OpenMortal Community
AppPublisherURL={#AppURL}
AppSupportURL={#AppContact}
AppUpdatesURL={#AppURL}/releases
; Fixed path — must match the --prefix=/OpenMortal used at compile time
DefaultDirName={sd}\OpenMortal
DisableDirPage=yes
DefaultGroupName={#AppName}
AllowNoIcons=yes
LicenseFile=..\COPYING
SetupIconFile=openmortal.ico
OutputDir=Output
OutputBaseFilename=openmortal-{#AppVersion}-win64-setup
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64compatible

[Languages]
Name: "english";   MessagesFile: "compiler:Default.isl"
Name: "hungarian"; MessagesFile: "compiler:Languages\Hungarian.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; Main executable
Source: "{#SourceDir}\bin\{#AppExeName}"; DestDir: "{app}";            Flags: ignoreversion
; Runtime DLLs (collected by ntldd during the CI build)
Source: "{#SourceDir}\bin\*.dll";         DestDir: "{app}";            Flags: ignoreversion
; Game data tree — placed under share\openmortal so the hardcoded
; /OpenMortal/share/openmortal data path resolves to {app}\share\openmortal
Source: "{#SourceDir}\share\openmortal\*"; DestDir: "{app}\share\openmortal"; \
  Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#AppName}";                                  Filename: "{app}\{#AppExeName}"
Name: "{group}\{cm:UninstallProgram,{#AppName}}";            Filename: "{uninstallexe}"
Name: "{commondesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExeName}"; \
  Description: "{cm:LaunchProgram,{#AppName}}"; \
  Flags: nowait postinstall skipifsilent

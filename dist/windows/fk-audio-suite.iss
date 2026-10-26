; Inno-Setup-Skript für den Windows-Installer. Wird von dist\package-windows.ps1 mit
; /DAppVersion, /DSourceDir, /DOutputDir und /DOutputBase aufgerufen.

#define AppName "FK Audio Suite"
#define AppPublisher "FK Online Services"
#define AppExe "FK Audio Suite.exe"
#ifndef BundledPlugins
  #define BundledPlugins "False"
#endif

[Setup]
AppId={{6F2C1B7E-3A54-4C8B-9E2D-FA1D0C7B5E31}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL=https://fk-audiosuite.com
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
LicenseFile={#SourceDir}\LICENSE.txt
OutputDir={#OutputDir}
OutputBaseFilename={#OutputBase}
SetupIconFile=..\..\resources\icons\fk-audio-suite.ico
UninstallDisplayIcon={app}\{#AppExe}
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Languages]
Name: "german"; MessagesFile: "compiler:Languages\German.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Excludes: "VST3"; Flags: ignoreversion recursesubdirs
; Lite-Plugins (FK Rig) nur im Standardordner für VST3: dieselbe Stelle wie der Einzel-Installer des
; Plugins, dort findet sie jede DAW und FK Audio Suite (bundled-plugins.txt). Ohne ignoreversion
; ersetzt eine ältere DAW-Auslieferung kein neueres, einzeln installiertes Plugin. sharedfile zählt
; mit: Die Deinstallation der DAW entfernt das Plugin nur, wenn kein Plugin-Installer es noch braucht.
Source: "{#SourceDir}\VST3\*"; DestDir: "{commoncf64}\VST3"; Flags: recursesubdirs createallsubdirs sharedfile skipifsourcedoesntexist; Check: HasBundledPlugins

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Code]
function HasBundledPlugins: Boolean;
begin
  Result := {#BundledPlugins};
end;

[Run]
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent

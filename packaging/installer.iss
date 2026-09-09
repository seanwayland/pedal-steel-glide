; Inno Setup script for Pedal Steel Glide (Windows).
; Run from the repo root after a Release build:
;   iscc packaging\installer.iss
; Produces  packaging\Output\Pedal Steel Glide-<version>-Windows.exe

#define Version   Trim(FileRead(FileOpen("..\VERSION")))
#define ProjectName "PedalSteelGlide"
#define ProductName "Pedal Steel Glide"
#define Publisher   "Wayland Audio"
#define Year GetDateTimeString("yyyy","","")
#define Artefacts "..\build\" + ProjectName + "_artefacts\Release"

[Types]
Name: "full";   Description: "Full installation"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "vst3";       Description: "VST3 plugin";           Types: full custom
Name: "standalone"; Description: "Standalone application"; Types: full custom

[Setup]
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
AppName={#ProductName}
AppVersion={#Version}
AppPublisher={#Publisher}
AppCopyright=Copyright (C) {#Year} {#Publisher} - GPLv3
OutputBaseFilename={#ProductName}-{#Version}-Windows
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
LicenseFile=resources\LICENSE
UninstallFilesDir={commonappdata}\{#ProductName}\uninstall

[Files]
Source: "{#Artefacts}\VST3\{#ProductName}.vst3\*"; DestDir: "{commoncf64}\VST3\{#ProductName}.vst3\"; \
    Excludes: "*.ilk,*.exp,*.lib"; Flags: ignoreversion recursesubdirs; Components: vst3
Source: "{#Artefacts}\Standalone\{#ProductName}.exe"; DestDir: "{commonpf64}\{#Publisher}\{#ProductName}"; \
    Flags: ignoreversion; Components: standalone

[Icons]
Name: "{autoprograms}\{#ProductName}"; Filename: "{commonpf64}\{#Publisher}\{#ProductName}\{#ProductName}.exe"; Components: standalone
Name: "{autoprograms}\Uninstall {#ProductName}"; Filename: "{uninstallexe}"

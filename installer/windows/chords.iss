; Windows installer for Alea Chord Randomizer (Inno Setup).
; Built by CI: ISCC /DVersion=x.y.z installer\windows\chords.iss

[Setup]
AppName=Alea Chord Randomizer
AppVersion={#Version}
AppPublisher=Yuval Egozi
AppPublisherURL=https://github.com/yuvalEG/Alea
DefaultDirName={autopf}\Alea Chord Randomizer
DisableProgramGroupPage=yes
OutputBaseFilename=AleaChordRandomizer-{#Version}-Windows-Setup
OutputDir=..\..\build
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible

[Files]
Source: "..\..\build\AleaChords_artefacts\Release\Standalone\Alea Chord Randomizer.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\Alea Chord Randomizer"; Filename: "{app}\Alea Chord Randomizer.exe"

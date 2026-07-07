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

[Types]
Name: "full"; Description: "Everything"
Name: "custom"; Description: "Choose components"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 Plugin (Ableton Live, Cubase, Bitwig...)"; Types: full
Name: "clap"; Description: "CLAP Plugin (Bitwig, Reaper...)"; Types: full
Name: "standalone"; Description: "Standalone App (built-in synth, no DAW needed)"; Types: full

[Files]
Source: "..\..\build\AleaChords_artefacts\Release\VST3\Alea Chord Randomizer.vst3\*"; DestDir: "{commoncf64}\VST3\Alea Chord Randomizer.vst3"; Components: vst3; Flags: recursesubdirs ignoreversion
Source: "..\..\build\AleaChords_artefacts\Release\CLAP\Alea Chord Randomizer.clap"; DestDir: "{commoncf64}\CLAP"; Components: clap; Flags: ignoreversion
Source: "..\..\build\AleaChords_artefacts\Release\Standalone\Alea Chord Randomizer.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\Alea Chord Randomizer"; Filename: "{app}\Alea Chord Randomizer.exe"; Components: standalone

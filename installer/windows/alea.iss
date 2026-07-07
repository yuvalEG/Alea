; Windows installer for Alea Scale Shifter (Inno Setup).
; Built by CI: ISCC /DVersion=x.y.z installer\windows\alea.iss

[Setup]
AppName=Alea Scale Shifter
AppVersion={#Version}
AppPublisher=Yuval Egozi
AppPublisherURL=https://github.com/yuvalEG/Alea
DefaultDirName={autopf}\Alea Scale Shifter
DisableProgramGroupPage=yes
OutputBaseFilename=Alea-{#Version}-Windows-Setup
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
Source: "..\..\build\Alea_artefacts\Release\VST3\Alea Scale Shifter.vst3\*"; DestDir: "{commoncf64}\VST3\Alea Scale Shifter.vst3"; Components: vst3; Flags: recursesubdirs ignoreversion
Source: "..\..\build\Alea_artefacts\Release\CLAP\Alea Scale Shifter.clap"; DestDir: "{commoncf64}\CLAP"; Components: clap; Flags: ignoreversion
Source: "..\..\build\Alea_artefacts\Release\Standalone\Alea Scale Shifter.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\Alea Scale Shifter"; Filename: "{app}\Alea Scale Shifter.exe"; Components: standalone

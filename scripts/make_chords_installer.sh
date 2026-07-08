#!/bin/bash
# Builds the macOS installer for Alea Chord Randomizer
# (AleaChordRandomizer-<version>.pkg) with selectable components:
# VST3, AU, CLAP, and/or the standalone app.
#
# Usage: scripts/make_chords_installer.sh   (expects a Release build in ./build)
#
# Note: the resulting pkg is UNSIGNED, like the Scale Shifter pkg - see
# make_installer.sh for the signing/notarization notes.

set -euo pipefail
cd "$(dirname "$0")/.."

VERSION=$(sed -n 's/^set(CHORDS_VERSION \([0-9.]*\))$/\1/p' CMakeLists.txt)
VST3="build/AleaChords_artefacts/Release/VST3/Alea Chord Randomizer.vst3"
AU="build/AleaChords_artefacts/Release/AU/Alea Chord Randomizer.component"
CLAP="build/AleaChords_artefacts/Release/CLAP/Alea Chord Randomizer.clap"
APP="build/AleaChords_artefacts/Release/Standalone/Alea Chord Randomizer.app"
OUT="build/installer-chords"

for artefact in "$VST3" "$AU" "$CLAP" "$APP"; do
    [ -d "$artefact" ] || { echo "Missing $artefact - run a Release build first."; exit 1; }
done

rm -rf "$OUT"
mkdir -p "$OUT/vst3root/Library/Audio/Plug-Ins/VST3" \
         "$OUT/auroot/Library/Audio/Plug-Ins/Components" \
         "$OUT/claproot/Library/Audio/Plug-Ins/CLAP" \
         "$OUT/approot/Applications"
cp -R "$VST3" "$OUT/vst3root/Library/Audio/Plug-Ins/VST3/"
cp -R "$AU"   "$OUT/auroot/Library/Audio/Plug-Ins/Components/"
cp -R "$CLAP" "$OUT/claproot/Library/Audio/Plug-Ins/CLAP/"
cp -R "$APP"  "$OUT/approot/Applications/"

pkgbuild --root "$OUT/vst3root" --identifier com.alea-audio.chords.vst3 \
         --version "$VERSION" --install-location / "$OUT/Chords-VST3.pkg" > /dev/null
pkgbuild --root "$OUT/auroot" --identifier com.alea-audio.chords.au \
         --version "$VERSION" --install-location / "$OUT/Chords-AU.pkg" > /dev/null
pkgbuild --root "$OUT/claproot" --identifier com.alea-audio.chords.clap \
         --version "$VERSION" --install-location / "$OUT/Chords-CLAP.pkg" > /dev/null
pkgbuild --root "$OUT/approot" --identifier com.alea-audio.chords.app \
         --version "$VERSION" --install-location / "$OUT/Chords-App.pkg" > /dev/null

cp Assets/installer-bg.png "$OUT/background.png"

cat > "$OUT/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
    <title>Alea Chord Randomizer $VERSION</title>
    <options customize="always" require-scripts="false" rootVolumeOnly="true"/>
    <background file="background.png" mime-type="image/png" alignment="bottomleft" scaling="none"/>
    <background-darkAqua file="background.png" mime-type="image/png" alignment="bottomleft" scaling="none"/>
    <welcome language="en" mime-type="text/plain">Alea Chord Randomizer. Roll random chords, loop them, improvise over them. Choose which versions to install.</welcome>
    <choices-outline>
        <line choice="vst3"/>
        <line choice="au"/>
        <line choice="clap"/>
        <line choice="standalone"/>
    </choices-outline>
    <choice id="vst3" title="VST3 Plugin" description="For Ableton Live, Cubase, Bitwig and other VST3 hosts. Installs to /Library/Audio/Plug-Ins/VST3.">
        <pkg-ref id="com.alea-audio.chords.vst3"/>
    </choice>
    <choice id="au" title="AU Plugin" description="For Logic Pro, GarageBand and other Audio Unit hosts. Installs to /Library/Audio/Plug-Ins/Components.">
        <pkg-ref id="com.alea-audio.chords.au"/>
    </choice>
    <choice id="clap" title="CLAP Plugin" description="For Bitwig, Reaper and other CLAP hosts. Installs to /Library/Audio/Plug-Ins/CLAP.">
        <pkg-ref id="com.alea-audio.chords.clap"/>
    </choice>
    <choice id="standalone" title="Standalone App" description="Runs on its own with a built-in synth or direct MIDI output. Installs to /Applications.">
        <pkg-ref id="com.alea-audio.chords.app"/>
    </choice>
    <pkg-ref id="com.alea-audio.chords.vst3" version="$VERSION">Chords-VST3.pkg</pkg-ref>
    <pkg-ref id="com.alea-audio.chords.au" version="$VERSION">Chords-AU.pkg</pkg-ref>
    <pkg-ref id="com.alea-audio.chords.clap" version="$VERSION">Chords-CLAP.pkg</pkg-ref>
    <pkg-ref id="com.alea-audio.chords.app" version="$VERSION">Chords-App.pkg</pkg-ref>
</installer-gui-script>
EOF

productbuild --distribution "$OUT/distribution.xml" --package-path "$OUT" --resources "$OUT" \
             "build/AleaChordRandomizer-$VERSION.pkg" > /dev/null

echo "Built build/AleaChordRandomizer-$VERSION.pkg (unsigned)"

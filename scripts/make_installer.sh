#!/bin/bash
# Builds a macOS installer pkg with selectable components: VST3, AU, CLAP,
# and/or the standalone app. One script, both products - they differed only
# in names, and keeping two copies meant every installer change had to be
# made twice (and stayed right only by luck).
#
# Usage: scripts/make_installer.sh shifter   (expects a Release build in ./build)
#        scripts/make_installer.sh chords
#
# Note: the resulting pkg is UNSIGNED. Distributing outside this Mac without
# Gatekeeper warnings requires an Apple Developer ID for signing and
# notarization (productsign + notarytool).

set -euo pipefail
cd "$(dirname "$0")/.."

PRODUCT="${1:-}"
case "$PRODUCT" in
    shifter)
        NAME="Alea Scale Shifter"
        ARTEFACTS="Alea_artefacts"
        BUNDLE="com.alea-audio.alea"     # component identifiers derive from this
        COMPONENT="Alea"                 # component pkg filename prefix
        PKGNAME="Alea"                   # build/Alea-<version>.pkg
        OUT="build/installer"
        BACKGROUND="Assets/installer-bg-scale-shifter.png"
        TITLE="Alea Scale Shifter"
        WELCOME="Alea Scale Shifter. Choose which versions to install."
        VERSION=$(sed -n 's/^project(Alea VERSION \([0-9.]*\))$/\1/p' CMakeLists.txt)
        ;;
    chords)
        NAME="Alea Chord Randomizer"
        ARTEFACTS="AleaChords_artefacts"
        BUNDLE="com.alea-audio.chords"
        COMPONENT="Chords"
        PKGNAME="AleaChordRandomizer"
        OUT="build/installer-chords"
        BACKGROUND="Assets/installer-bg-chords.png"
        TITLE="Alea Chord Randomizer"
        WELCOME="Alea Chord Randomizer. Roll random chords, loop them, improvise over them. Choose which versions to install."
        VERSION=$(sed -n 's/^set(CHORDS_VERSION \([0-9.]*\))$/\1/p' CMakeLists.txt)
        ;;
    *)
        echo "Usage: scripts/make_installer.sh <shifter|chords>"
        exit 2
        ;;
esac

VST3="build/$ARTEFACTS/Release/VST3/$NAME.vst3"
AU="build/$ARTEFACTS/Release/AU/$NAME.component"
CLAP="build/$ARTEFACTS/Release/CLAP/$NAME.clap"
APP="build/$ARTEFACTS/Release/Standalone/$NAME.app"

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

pkgbuild --root "$OUT/vst3root" --identifier "$BUNDLE.vst3" \
         --version "$VERSION" --install-location / "$OUT/$COMPONENT-VST3.pkg" > /dev/null
pkgbuild --root "$OUT/auroot" --identifier "$BUNDLE.au" \
         --version "$VERSION" --install-location / "$OUT/$COMPONENT-AU.pkg" > /dev/null
pkgbuild --root "$OUT/claproot" --identifier "$BUNDLE.clap" \
         --version "$VERSION" --install-location / "$OUT/$COMPONENT-CLAP.pkg" > /dev/null
pkgbuild --root "$OUT/approot" --identifier "$BUNDLE.app" \
         --version "$VERSION" --install-location / "$OUT/$COMPONENT-App.pkg" > /dev/null

cp "$BACKGROUND" "$OUT/background.png"

cat > "$OUT/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
    <title>$TITLE $VERSION</title>
    <options customize="always" require-scripts="false" rootVolumeOnly="true"/>
    <background file="background.png" mime-type="image/png" alignment="bottomleft" scaling="none"/>
    <background-darkAqua file="background.png" mime-type="image/png" alignment="bottomleft" scaling="none"/>
    <welcome language="en" mime-type="text/plain">$WELCOME</welcome>
    <choices-outline>
        <line choice="vst3"/>
        <line choice="au"/>
        <line choice="clap"/>
        <line choice="standalone"/>
    </choices-outline>
    <choice id="vst3" title="VST3 Plugin" description="For Ableton Live, Cubase, Bitwig and other VST3 hosts. Installs to /Library/Audio/Plug-Ins/VST3.">
        <pkg-ref id="$BUNDLE.vst3"/>
    </choice>
    <choice id="au" title="AU Plugin" description="For Logic Pro, GarageBand and other Audio Unit hosts. Installs to /Library/Audio/Plug-Ins/Components.">
        <pkg-ref id="$BUNDLE.au"/>
    </choice>
    <choice id="clap" title="CLAP Plugin" description="For Bitwig, Reaper and other CLAP hosts. Installs to /Library/Audio/Plug-Ins/CLAP.">
        <pkg-ref id="$BUNDLE.clap"/>
    </choice>
    <choice id="standalone" title="Standalone App" description="Runs on its own with a built-in synth or direct MIDI output. Installs to /Applications.">
        <pkg-ref id="$BUNDLE.app"/>
    </choice>
    <pkg-ref id="$BUNDLE.vst3" version="$VERSION">$COMPONENT-VST3.pkg</pkg-ref>
    <pkg-ref id="$BUNDLE.au" version="$VERSION">$COMPONENT-AU.pkg</pkg-ref>
    <pkg-ref id="$BUNDLE.clap" version="$VERSION">$COMPONENT-CLAP.pkg</pkg-ref>
    <pkg-ref id="$BUNDLE.app" version="$VERSION">$COMPONENT-App.pkg</pkg-ref>
</installer-gui-script>
EOF

productbuild --distribution "$OUT/distribution.xml" --package-path "$OUT" --resources "$OUT" \
             "build/$PKGNAME-$VERSION.pkg" > /dev/null

echo "Built build/$PKGNAME-$VERSION.pkg (unsigned)"

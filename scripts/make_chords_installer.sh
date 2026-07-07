#!/bin/bash
# Builds the macOS installer for Alea Chord Randomizer
# (AleaChordRandomizer-<version>.pkg) - standalone app only.
#
# Usage: scripts/make_chords_installer.sh   (expects a Release build in ./build)
#
# Note: the resulting pkg is UNSIGNED, like the Scale Shifter pkg - see
# make_installer.sh for the signing/notarization notes.

set -euo pipefail
cd "$(dirname "$0")/.."

VERSION=$(sed -n 's/^set(CHORDS_VERSION \([0-9.]*\))$/\1/p' CMakeLists.txt)
APP="build/AleaChords_artefacts/Release/Standalone/Alea Chord Randomizer.app"
OUT="build/installer-chords"

[ -d "$APP" ] || { echo "Missing $APP - run a Release build first."; exit 1; }

rm -rf "$OUT"
mkdir -p "$OUT/approot/Applications"
cp -R "$APP" "$OUT/approot/Applications/"

pkgbuild --root "$OUT/approot" --identifier com.alea-audio.chords.app \
         --version "$VERSION" --install-location / "$OUT/Chords-App.pkg" > /dev/null

cp Assets/installer-bg.png "$OUT/background.png"

cat > "$OUT/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
    <title>Alea Chord Randomizer $VERSION</title>
    <options customize="never" require-scripts="false" rootVolumeOnly="true"/>
    <background file="background.png" mime-type="image/png" alignment="bottomleft" scaling="none"/>
    <background-darkAqua file="background.png" mime-type="image/png" alignment="bottomleft" scaling="none"/>
    <welcome language="en" mime-type="text/plain">Alea Chord Randomizer - roll random chords, loop them, improvise over them. Installs the app to /Applications.</welcome>
    <choices-outline>
        <line choice="app"/>
    </choices-outline>
    <choice id="app" title="Alea Chord Randomizer" description="The standalone app. Installs to /Applications.">
        <pkg-ref id="com.alea-audio.chords.app"/>
    </choice>
    <pkg-ref id="com.alea-audio.chords.app" version="$VERSION">Chords-App.pkg</pkg-ref>
</installer-gui-script>
EOF

productbuild --distribution "$OUT/distribution.xml" --package-path "$OUT" --resources "$OUT" \
             "build/AleaChordRandomizer-$VERSION.pkg" > /dev/null

echo "Built build/AleaChordRandomizer-$VERSION.pkg (unsigned)"

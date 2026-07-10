#!/bin/bash
# Builds a macOS installer (Alea-<version>.pkg) with selectable components:
# VST3, AU, CLAP, and/or the standalone app.
#
# Usage: scripts/make_installer.sh   (expects a Release build in ./build)
#
# Note: the resulting pkg is UNSIGNED. Distributing outside this Mac without
# Gatekeeper warnings requires an Apple Developer ID for signing and
# notarization (productsign + notarytool).

set -euo pipefail
cd "$(dirname "$0")/.."

VERSION=$(sed -n 's/^project(Alea VERSION \([0-9.]*\))$/\1/p' CMakeLists.txt)
VST3="build/Alea_artefacts/Release/VST3/Alea Scale Shifter.vst3"
AU="build/Alea_artefacts/Release/AU/Alea Scale Shifter.component"
CLAP="build/Alea_artefacts/Release/CLAP/Alea Scale Shifter.clap"
APP="build/Alea_artefacts/Release/Standalone/Alea Scale Shifter.app"
OUT="build/installer"

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

pkgbuild --root "$OUT/vst3root" --identifier com.alea-audio.alea.vst3 \
         --version "$VERSION" --install-location / "$OUT/Alea-VST3.pkg" > /dev/null
pkgbuild --root "$OUT/auroot" --identifier com.alea-audio.alea.au \
         --version "$VERSION" --install-location / "$OUT/Alea-AU.pkg" > /dev/null
pkgbuild --root "$OUT/claproot" --identifier com.alea-audio.alea.clap \
         --version "$VERSION" --install-location / "$OUT/Alea-CLAP.pkg" > /dev/null
pkgbuild --root "$OUT/approot" --identifier com.alea-audio.alea.app \
         --version "$VERSION" --install-location / "$OUT/Alea-App.pkg" > /dev/null

cp Assets/installer-bg-scale-shifter.png "$OUT/background.png"

cat > "$OUT/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
    <title>Alea $VERSION</title>
    <options customize="always" require-scripts="false" rootVolumeOnly="true"/>
    <background file="background.png" mime-type="image/png" alignment="bottomleft" scaling="none"/>
    <background-darkAqua file="background.png" mime-type="image/png" alignment="bottomleft" scaling="none"/>
    <welcome language="en" mime-type="text/plain">Alea Scale Shifter. Choose which versions to install.</welcome>
    <choices-outline>
        <line choice="vst3"/>
        <line choice="au"/>
        <line choice="clap"/>
        <line choice="standalone"/>
    </choices-outline>
    <choice id="vst3" title="VST3 Plugin" description="For Ableton Live, Cubase, Bitwig and other VST3 hosts. Installs to /Library/Audio/Plug-Ins/VST3.">
        <pkg-ref id="com.alea-audio.alea.vst3"/>
    </choice>
    <choice id="au" title="AU Plugin" description="For Logic Pro, GarageBand and other Audio Unit hosts. Installs to /Library/Audio/Plug-Ins/Components.">
        <pkg-ref id="com.alea-audio.alea.au"/>
    </choice>
    <choice id="clap" title="CLAP Plugin" description="For Bitwig, Reaper and other CLAP hosts. Installs to /Library/Audio/Plug-Ins/CLAP.">
        <pkg-ref id="com.alea-audio.alea.clap"/>
    </choice>
    <choice id="standalone" title="Standalone App" description="Runs on its own with a built-in synth or direct MIDI output. Installs to /Applications.">
        <pkg-ref id="com.alea-audio.alea.app"/>
    </choice>
    <pkg-ref id="com.alea-audio.alea.vst3" version="$VERSION">Alea-VST3.pkg</pkg-ref>
    <pkg-ref id="com.alea-audio.alea.au" version="$VERSION">Alea-AU.pkg</pkg-ref>
    <pkg-ref id="com.alea-audio.alea.clap" version="$VERSION">Alea-CLAP.pkg</pkg-ref>
    <pkg-ref id="com.alea-audio.alea.app" version="$VERSION">Alea-App.pkg</pkg-ref>
</installer-gui-script>
EOF

productbuild --distribution "$OUT/distribution.xml" --package-path "$OUT" --resources "$OUT" \
             "build/Alea-$VERSION.pkg" > /dev/null

echo "Built build/Alea-$VERSION.pkg (unsigned)"

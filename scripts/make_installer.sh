#!/bin/bash
# Builds a macOS installer (Alea-<version>.pkg) with selectable components:
# the user picks VST3 and/or the standalone app. AU/CLAP slot in here once
# they exist (M6).
#
# Usage: scripts/make_installer.sh   (expects a Release build in ./build)
#
# Note: the resulting pkg is UNSIGNED. Distributing outside this Mac without
# Gatekeeper warnings requires an Apple Developer ID for signing and
# notarization (productsign + notarytool).

set -euo pipefail
cd "$(dirname "$0")/.."

VERSION=$(sed -n 's/^project(Alea VERSION \([0-9.]*\))$/\1/p' CMakeLists.txt)
VST3="build/Alea_artefacts/Release/VST3/Alea.vst3"
APP="build/Alea_artefacts/Release/Standalone/Alea.app"
OUT="build/installer"

[ -d "$VST3" ] || { echo "Missing $VST3 - run a Release build first."; exit 1; }
[ -d "$APP" ]  || { echo "Missing $APP - run a Release build first."; exit 1; }

rm -rf "$OUT"
mkdir -p "$OUT/vst3root/Library/Audio/Plug-Ins/VST3" "$OUT/approot/Applications"
cp -R "$VST3" "$OUT/vst3root/Library/Audio/Plug-Ins/VST3/"
cp -R "$APP"  "$OUT/approot/Applications/"

pkgbuild --root "$OUT/vst3root" --identifier com.alea-audio.alea.vst3 \
         --version "$VERSION" --install-location / "$OUT/Alea-VST3.pkg" > /dev/null
pkgbuild --root "$OUT/approot" --identifier com.alea-audio.alea.app \
         --version "$VERSION" --install-location / "$OUT/Alea-App.pkg" > /dev/null

cat > "$OUT/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
    <title>Alea $VERSION</title>
    <options customize="always" require-scripts="false" rootVolumeOnly="true"/>
    <welcome language="en" mime-type="text/plain">Alea - Aleatoric Scale Shifter. Choose which versions to install.</welcome>
    <choices-outline>
        <line choice="vst3"/>
        <line choice="standalone"/>
    </choices-outline>
    <choice id="vst3" title="VST3 Plugin" description="For Ableton Live, Cubase, Bitwig and other VST3 hosts. Installs to /Library/Audio/Plug-Ins/VST3.">
        <pkg-ref id="com.alea-audio.alea.vst3"/>
    </choice>
    <choice id="standalone" title="Standalone App" description="Runs on its own with a built-in synth or direct MIDI output. Installs to /Applications.">
        <pkg-ref id="com.alea-audio.alea.app"/>
    </choice>
    <pkg-ref id="com.alea-audio.alea.vst3" version="$VERSION">Alea-VST3.pkg</pkg-ref>
    <pkg-ref id="com.alea-audio.alea.app" version="$VERSION">Alea-App.pkg</pkg-ref>
</installer-gui-script>
EOF

productbuild --distribution "$OUT/distribution.xml" --package-path "$OUT" \
             "build/Alea-$VERSION.pkg" > /dev/null

echo "Built build/Alea-$VERSION.pkg (unsigned)"

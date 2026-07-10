#!/bin/bash
# Regenerates the README UI screenshots for BOTH products from the current
# build. Run after ANY UI change, then commit docs/ui.png and
# docs/chords-ui.png (the README embeds them side by side, so they keep the
# same aspect ratio - Scale Shifter's natural frame, ~1.33).
#
# Usage: scripts/update_screenshots.sh   (expects a Release build in ./build)

set -euo pipefail
cd "$(dirname "$0")/.."

./build/AleaUISnapshot_artefacts/Release/AleaUISnapshot docs/ui.png
# 3 pre-rolls (history filled), sevenths+simplify on, posed mid-playing,
# at the default window size.
./build/ChordsUISnapshot_artefacts/Release/ChordsUISnapshot docs/chords-ui.png 3 1 1 1 960 720

echo "Updated docs/ui.png and docs/chords-ui.png - review and commit them."

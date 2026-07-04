# Alea

A generative MIDI plugin: pick a set of notes, press play, and Alea streams
random notes from that set into your DAW — with the ability to slowly morph
toward a second note set over time.

Full product spec: [docs/SPEC.md](docs/SPEC.md) (snapshot of the canonical
Notion spec, "RandoMidi → Spec v2").

## Status

**Milestone 1** — project scaffold. Builds a VST3 plugin and a standalone app
from one JUCE codebase. While the DAW transport is playing, the plugin emits
one fixed note (C3) per beat on MIDI channel 1, to prove MIDI output works
end to end. No generator, no morph, no UI yet.

## Building

Requires: macOS, Xcode command-line tools, CMake ≥ 3.22
(`brew install cmake`). JUCE is downloaded automatically on first configure.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The VST3 is copied to `~/Library/Audio/Plug-Ins/VST3/` after a successful
build, so it shows up in your DAW's next plugin scan. The standalone app ends
up under `build/Alea_artefacts/`.

## Trying it in a DAW

1. Build (above), then rescan plugins in your DAW.
2. Add **Alea** as a MIDI-effect / instrument-track plugin.
3. Route Alea's MIDI output to an instrument (per-DAW routing varies).
4. Press Play — you should hear C3 once per beat.

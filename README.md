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

Alea is classified as a VST3 *instrument* that outputs MIDI (and silent
audio) — Ableton Live won't load VST3s with no audio output, and hosts have
no common slot for third-party "MIDI effect" plugins.

In Ableton Live:

1. Build (above). In Live: Settings → Plug-Ins → Rescan (hold Alt and click
   Rescan for a full rescan if Alea doesn't appear — Live caches plugins
   that previously failed to load).
2. Drop **Alea** (under Plug-Ins) onto a MIDI track.
3. On a second MIDI track with an instrument: set **MIDI From** to the Alea
   track, choose **Alea** in the second chooser, and set Monitor to **In**.
4. Press Play — you should hear C3 once per beat.

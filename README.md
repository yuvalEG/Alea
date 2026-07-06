# Alea

**Aleatoric Scale Shifter** - a generative MIDI plugin. Pick a set of notes,
press play, and Alea streams random notes from that set into your DAW, slowly
morphing toward a second note set over time.

Alea was made with a particular vision in mind: exploring the relationship
between an improvising human player and a machine that randomly shifts from a
diatonic scale to complete dodecaphony over time.

![Alea UI](docs/ui.png)

## Features

- **Two scales** (A and B): pick pitch classes on a keyboard, set octave and
  velocity ranges, and add weighted rests (2 bars down to 1/16) that roll
  like notes do.
- **Scale Morph**: blend the probability of drawing from A vs. B - drag the
  bar, automate it, bind a MIDI CC (right-click the bar), or engage
  **AUTO-SWEEP** to travel on its own: One-Shot, Loop, or Bounce, over bars
  or free time, shaped by linear/exponential/S/logarithmic curves. Grab the
  bar mid-sweep to scrub; the sweep re-anchors and keeps going.
- **Timing**: note rate and length synced to the host (4 bars down to a
  1/128 note), free-running in ms/seconds, or rolled randomly per note.
- **Roots and transpose**: each scale picks the pitch its octave span starts
  from; a global transpose shifts the whole output.
- **Internal synth**: a warm 8-voice pad (detuned, phasing, velocity-aware,
  with delay and reverb) so the AU, and the standalone app, make sound with
  zero routing.
- **Performance controls**: Freeze (hold the stream), Panic (all notes off).
- **Monitoring**: activity LED, current note, bar/beat, an 88-key strip, and
  an event history ticker - everything colored by which scale it came from.
- **10 factory presets** - from *Just an Arp* through the eerie *Hexatonic
  Pole* to *Order → Chaos*, the three-minute journey from five quiet notes
  to full dodecaphony - plus save/load of your own patches as `.alea` files.
- Deterministic per session: loop playback re-rolls the same choices, so what
  you heard is what you'll hear again.

## Status

**v0.2** - VST3, AU and CLAP plugins plus a standalone app, on macOS.
Every build passes [pluginval](https://github.com/Tracktion/pluginval) at
strictness 10; the AU passes Apple's auval.

`scripts/make_installer.sh` builds a pkg installer with selectable
components (unsigned for now).

## Installing

**Alea is macOS only for now** (Windows is on the wish list). You don't need
to build anything:

1. Download `Alea-x.y.z.pkg` from the
   [latest release](https://github.com/yuvalEG/Alea/releases).
2. Double-click it. The installer is unsigned for now, so macOS may refuse
   the first open - right-click (Control-click) the file, choose **Open**,
   and confirm.
3. Tick the versions you want: VST3, AU, CLAP, and/or the standalone app.
4. Restart your DAW (or rescan plugins) - it appears as **Alea Scale
   Shifter**.

Which format to load:

- **Ableton Live, Cubase**: VST3. In Live specifically, don't use the AU -
  Live cannot route MIDI out of AU plugins, so Alea's whole routing story
  only works with the VST3 there.
- **Logic Pro, GarageBand**: AU. Logic can't route AU MIDI either, so set
  OUT to **Internal Synth** to hear Alea directly there.
- **Bitwig, Reaper**: CLAP (or VST3 - both work)
- **No DAW at all**: the standalone app, with its built-in synth or direct
  MIDI output to hardware.

## Building

Requires: macOS, Xcode command-line tools, CMake ≥ 3.22
(`brew install cmake`). JUCE is downloaded automatically on first configure.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The VST3 and AU are copied to `~/Library/Audio/Plug-Ins/` after a successful
build, so they show up in your DAW's next plugin scan. The CLAP and the
standalone app end up under `build/Alea_artefacts/Release/`.

## Using it in a DAW

Alea generates MIDI notes - by default it makes no sound of its own (flip
OUT to **Internal Synth** if you want it to). It is classified as an
*instrument* (with silent audio) because hosts have no common slot for
third-party MIDI-effect plugins.

In Ableton Live (VST3):

1. Build (above), then rescan plugins in Live's settings if Alea doesn't
   appear (hold Alt for a full rescan - Live caches failed loads).
2. Drop **Alea** onto a MIDI track.
3. On a second MIDI track with any instrument: set **MIDI From** to the Alea
   track and pick **Alea** in the chooser below it.
4. Arm the instrument track and press Play - you'll hear notes drawn from
   Scale A. Pick a preset, or hit AUTO-SWEEP and let it travel.

## Troubleshooting

**No sound from the plugin?**

1. Alea makes no audio of its own - its MIDI must reach an instrument on
   another track.
2. On the instrument track, set **MIDI From** to the Alea track and pick
   **Alea** in the chooser below it (not "Post FX"):

   ![Routing Alea to an instrument track in Ableton Live](docs/routing.png)

3. Arm the instrument track (record button) so it receives MIDI.
4. Press Play - Alea follows the host transport; the dot in Alea's header
   should read "playing".
5. Still nothing? Check the morph bar: at 100% B with an empty Scale B there
   is nothing to play. Hit PANIC once if a note seems stuck.
6. Alea missing from Live's browser? Settings > Plug-Ins, hold Alt and click
   Rescan (Live caches plugins that previously failed to load).
7. Using the AU in Live? That's the trap - Live cannot route MIDI from AU
   plugins at all. Load the VST3 instead.

**Standalone app silent?** Pick **Internal Synth** in the OUT dropdown
(OUTPUT panel) and press PLAY.

## Feedback

I'll be more than happy to hear your feedback, ideas, and music made with
Alea - open an issue here or write to yuvalprod@gmail.com.

Plugin made by Yuval Egozi.

## License

Alea is open source under the [GPLv3](LICENSE) (it is built on
[JUCE](https://juce.com), whose free tier requires it). It also uses
[clap-juce-extensions](https://github.com/free-audio/clap-juce-extensions)
and the [CLAP](https://github.com/free-audio/clap) SDK (both MIT), and
embeds the Space Grotesk font (SIL Open Font License).

The Alea name, wordmark, and icon artwork are copyright Yuval Egozi and are
not covered by the code license - please don't reuse the branding in forks.

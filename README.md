# Alea

Small instruments for improvising musicians, built around dice
(*alea* is Latin for dice). Two products live in this repo:

| | What it is | Get it |
|---|---|---|
| **[Alea Scale Shifter](#alea-scale-shifter)** | Generative MIDI plugin + app: random notes from a scale, morphing into another scale over time | [Latest release](https://github.com/yuvalEG/Alea/releases?q=%22v0.%22&expanded=false) (`Alea-x.y.z` files) |
| **[Alea Chord Randomizer](#alea-chord-randomizer)** | Practice app: roll random chords, loop them, improvise over them | [Latest release](https://github.com/yuvalEG/Alea/releases?q=chords&expanded=false) (`AleaChordRandomizer-x.y.z` files) |

Each product releases on its own: Scale Shifter releases are tagged
`vX.Y.Z`, Chord Randomizer releases `chords-vX.Y.Z`, and every release
carries only that product's installers - download just the one you want.
Both apps check for their own updates from the in-app menu.

---

## Alea Scale Shifter

A generative MIDI plugin. Pick a set of notes, press play, and Alea streams
random notes from that set into your DAW, slowly morphing toward a second
note set over time - made to explore the relationship between an improvising
human player and a machine that drifts from a diatonic scale to complete
dodecaphony.

![Alea Scale Shifter UI](docs/ui.png)

### Features

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

**Status: v0.3** - VST3, AU and CLAP plugins plus a standalone app, for
macOS and Windows. Every build passes
[pluginval](https://github.com/Tracktion/pluginval) at strictness 10; the
AU passes Apple's auval.

### Installing Scale Shifter

Grab the [latest `vX.Y.Z` release](https://github.com/yuvalEG/Alea/releases) -
no building needed.

**macOS**: download `Alea-x.y.z.pkg` and double-click it. The installer is
unsigned for now, so macOS may refuse the first open - right-click
(Control-click) the file, choose **Open**, and confirm. Tick the versions
you want: VST3, AU, CLAP, and/or the standalone app.

**Windows**: download `Alea-x.y.z-Windows-Setup.exe` and run it (SmartScreen
may warn about an unknown publisher - choose "More info" then "Run anyway"),
or grab the portable zip and drop the files where you like. Components:
VST3, CLAP, and the standalone app.

Then restart your DAW (or rescan plugins) - it appears as **Alea Scale
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

### Using it in a DAW

Alea generates MIDI notes - by default it makes no sound of its own (flip
OUT to **Internal Synth** if you want it to). It is classified as an
*instrument* (with silent audio) because hosts have no common slot for
third-party MIDI-effect plugins.

In Ableton Live (VST3):

1. Rescan plugins in Live's settings if Alea doesn't appear (hold Alt for a
   full rescan - Live caches failed loads).
2. Drop **Alea** onto a MIDI track.
3. On a second MIDI track with any instrument: set **MIDI From** to the Alea
   track and pick **Alea** in the chooser below it.
4. Arm the instrument track and press Play - you'll hear notes drawn from
   Scale A. Pick a preset, or hit AUTO-SWEEP and let it travel.

### Troubleshooting

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
(OUTPUT panel) and press play.

---

## Alea Chord Randomizer

A practice partner. Roll a short series of random chords, press play, and
the app loops them - each chord held for its bars at your tempo - while you
improvise over them on a real instrument. Born from an improvisation
exercise by guitar teacher Yonatan Benaroche: a progression you did not
choose forces your ear and hands out of familiar shapes.

![Alea Chord Randomizer UI](docs/chords-ui.png)

### Features

- **The dice**: series of 1-8 chords; chord size (triads / 7th chords / 9th
  chords), sus chords, a guitar-friendly Simplify mode, and a meticulous
  chord vocabulary - every chord uniquely and correctly spelled.
- **Key lock**: roll only the diatonic chords of a chosen key and scale -
  major, minor, or harmonic minor - with the flavors kept strictly in-scale.
- **The loop**: tempo, 1/2/4 bars per chord, block-chord voicings doubled
  across your choice of octaves, played through the family synth (four
  flavours) or sent as MIDI to any device.
- **Practice flow**: auto roll every N loops (hands never leave the
  instrument), pin the chords you love and reroll the rest, click a chord to
  jump the loop there, click a past roll in HISTORY to bring it back.
- **Switching you can see**: while a new roll waits for the chord boundary,
  the sounding chord stays purple and the incoming chords preview in cyan.
- **Performance controls**: metronome click with its own volume, FREEZE
  (hold the chord, time stops), PANIC, and a monitor keyboard showing
  exactly which notes sound.
- Keyboard: **Space** play/stop, **R** roll, **A** auto-roll on/off.

**Status: v0.1** - standalone app for macOS and Windows. Plugin formats may
come later.

### Installing Chord Randomizer

Grab the [latest `chords-vX.Y.Z` release](https://github.com/yuvalEG/Alea/releases).

**macOS**: download `AleaChordRandomizer-x.y.z.pkg` and double-click it
(unsigned - right-click, **Open**, confirm, as above). Installs the app to
/Applications.

**Windows**: download `AleaChordRandomizer-x.y.z-Windows-Setup.exe` and run
it, or grab the portable zip.

Open the app, press ROLL, press play, and jam. Sound comes out of the box
via the built-in synth; switch OUT to a MIDI device to drive hardware or a
DAW instrument instead.

---

## Building

Requires: macOS (or Windows for the Windows targets), CMake ≥ 3.22
(`brew install cmake`). JUCE is downloaded automatically on first configure.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

One build makes both products: Scale Shifter's VST3/AU are copied to
`~/Library/Audio/Plug-Ins/` (macOS), and both standalone apps end up under
`build/*_artefacts/Release/`. Installer scripts:
`scripts/make_installer.sh` (Scale Shifter pkg, selectable components) and
`scripts/make_chords_installer.sh` (Chord Randomizer pkg).

## Feedback

I'll be more than happy to hear your feedback, ideas, and music made with
either Alea - open an issue here or write to yuvalprod@gmail.com.

Made by Yuval Egozi.

## License

Alea is open source under the [GPLv3](LICENSE) (it is built on
[JUCE](https://juce.com), whose free tier requires it). It also uses
[clap-juce-extensions](https://github.com/free-audio/clap-juce-extensions)
and the [CLAP](https://github.com/free-audio/clap) SDK (both MIT), and
embeds the Space Grotesk font (SIL Open Font License).

The Alea name, wordmark, and icon artwork are copyright Yuval Egozi and are
not covered by the code license - please don't reuse the branding in forks.

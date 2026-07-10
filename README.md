# Alea

Small instruments for improvising musicians.
Built around random elements (*alea* is Latin for dice, as in aleatoric
music).

![Alea Scale Shifter](docs/ui.png)

![Alea Chord Randomizer](docs/chords-ui.png)

| | What it is | Get it |
|---|---|---|
| **[Alea Scale Shifter](#alea-scale-shifter)** | Random notes from a scale, slowly morphing into another scale | [Latest release](https://github.com/yuvalEG/Alea/releases?q=%22v0.%22&expanded=false) (`Alea-x.y.z` files) |
| **[Alea Chord Randomizer](#alea-chord-randomizer)** | Random chords, looped. Improvise over them | [Latest release](https://github.com/yuvalEG/Alea/releases?q=chords&expanded=false) (`AleaChordRandomizer-x.y.z` files) |

Each product releases on its own. Download just the one you want.
Both come as VST3, AU and CLAP plugins plus a standalone app, for macOS
and Windows. Every build passes
[pluginval](https://github.com/Tracktion/pluginval) at strictness 10 and
Apple's auval.

## Installing

**macOS**: download the `.pkg` and double-click it. Tick the formats you
want: VST3, AU, CLAP, and/or the standalone app.

The installers are unsigned for now, so macOS will block the first open
and say the package "was blocked from use because it is not from an
identified developer". To install anyway:

1. Dismiss the warning.
2. Open **System Settings > Privacy & Security** and scroll down to the
   **Security** section.
3. You will see a note about the blocked installer. Click **Open Anyway**
   and confirm with your password.

On older versions of macOS, right-clicking the `.pkg` and choosing
**Open** also works.

**Windows**: download the `-Windows-Setup.exe` and run it (components:
VST3, CLAP, standalone), or grab the portable zip and drop the files where
you like. SmartScreen may warn about an unknown publisher. Choose "More
info" and then "Run anyway".

Then restart your DAW or rescan plugins. Which format to load:

- **Ableton Live, Cubase**: VST3. In Live, don't use the AU. Live cannot
  route MIDI out of AU plugins, so only the VST3 works there.
- **Logic Pro, GarageBand**: AU, with OUT set to **Internal Synth** (Logic
  can't route AU MIDI either).
- **Bitwig, Reaper**: CLAP or VST3.
- **No DAW at all**: the standalone app, with its built-in synth or direct
  MIDI output to hardware.

---

## Alea Scale Shifter

Pick a set of notes and press play. Alea streams an endless, never-repeating
line from them, slowly drifting toward a second set, from a scale you know
all the way to full dodecaphony, while you play against the drift.

- **Two scales**, A and B: notes, octaves, velocities, and rests that roll
  just like notes.
- **Scale Morph**: blend between them by hand, by automation, by MIDI CC,
  or let **AUTO-SWEEP** travel on its own.
- **Timing**: locked to your DAW, free-running, or rolled anew per note.
- **A warm built-in synth** in many flavours, so it makes sound anywhere
  with zero routing.
- **10 presets**, from *Just an Arp* to *Order → Chaos*, plus your own as
  `.alea` files.
- Replayable randomness: loop a section and it rolls the same dice again.

### Using it in a DAW

Alea generates MIDI notes. By default it makes no sound of its own (flip
OUT to **Internal Synth** if you want it to). In Ableton Live:

1. Drop **Alea** onto a MIDI track.
2. On a second MIDI track with any instrument: set **MIDI From** to the
   Alea track and pick **Alea** in the chooser below it (not "Post FX"):

   ![Routing Alea to an instrument track in Ableton Live](docs/routing.png)

3. Arm the instrument track and press Play. You'll hear notes drawn from
   Scale A. Pick a preset, or hit AUTO-SWEEP and let it travel.

### Troubleshooting

**No sound?** Check the routing above first. The header dot should read
"playing" while the host runs. At 100% B with an empty Scale B there is
nothing to play. A stuck note ends with one PANIC press. If Alea is missing
from Live's browser, hold Alt and click Rescan in the plugin settings (Live
caches plugins that previously failed to load). And in the standalone app,
pick **Internal Synth** under OUT.

---

## Alea Chord Randomizer

Roll a handful of random chords, press play, and improvise over the loop.
Born from an improvisation exercise by guitar teacher Yonatan Benaroche:
a progression you did not choose forces your ear and hands out of familiar
shapes.

- **The dice**: a series of chords, with control over their complexity,
  and a meticulous chord vocabulary.
- **Key lock**: stay diatonic in any key, in major, minor, or harmonic minor.
- **The loop**: your tempo, your bars, your octaves, your voicings, through
  the built-in synth or any MIDI device.
- **Practice flow**: auto roll every few loops, pin the chords you love,
  bring back any past roll.
- Metronome, FREEZE, PANIC, and a keyboard that shows what is sounding.

Open the app, press ROLL, press play, and jam. Sound comes out of the box
via the built-in synth. In a DAW it works like Scale Shifter: route its
MIDI to an instrument track, or flip OUT to the built-in synth.

---

## Building

Requires CMake ≥ 3.22. JUCE is downloaded automatically on first configure.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

One build makes both products. Installer scripts:
`scripts/make_installer.sh` and `scripts/make_chords_installer.sh`.

## Feedback

I'll be more than happy to hear your feedback, ideas, and music made with
either Alea. Open an issue here or write to yuvalprod@gmail.com.

Made by Yuval Egozi.

## License

Alea is open source under the [GPLv3](LICENSE) (it is built on
[JUCE](https://juce.com), whose free tier requires it). It also uses
[clap-juce-extensions](https://github.com/free-audio/clap-juce-extensions)
and the [CLAP](https://github.com/free-audio/clap) SDK (both MIT), and
embeds the Space Grotesk font (SIL Open Font License). The built-in piano
uses samples from the
[Salamander Grand Piano](https://github.com/sfzinstruments/SalamanderGrandPiano)
by Alexander Holm ([CC BY 3.0](https://creativecommons.org/licenses/by/3.0/)).

The Alea name, wordmark, and icon artwork are copyright Yuval Egozi and are
not covered by the code license. Please don't reuse the branding in forks.

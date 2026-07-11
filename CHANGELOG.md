# Changelog - Alea Scale Shifter

(Chord Randomizer's changelog lives in [CHANGELOG-chords.md](CHANGELOG-chords.md).)

## v0.3.3 - 2026-07-11

A preset polish pass and two timing fixes.

- Five → One is reborn. A dark five-note Japanese scale collapses over
  one minute into a single low tolling A. It is the first preset built
  on free timing, so it drifts at its own pace no matter the tempo.
- Soft ↔ Loud now swells over 4 bars instead of pumping every 2. The
  presets that travel back and forth say so in their names with a
  two-way arrow. One-way journeys keep the one-way arrow.
- Pentatonic Drift now drifts there and back instead of snapping to the
  start. Octave Climb became an endless ascent that climbs, returns to
  the basement and climbs again.
- Fixed: with two scales in different keys sharing a note, the keyboard
  could light a key outside the scale while the right note sounded. The
  highlight now always agrees with your ears.
- Fixed: free timing reacted to tempo changes. Notes set in seconds and
  sweeps set in seconds now keep their real pace when the tempo moves
  under them. Musical divisions still follow the tempo, as they should.

## v0.3.2 - 2026-07-11

The hardware faceplate update. The whole window is now a brushed metal
panel you could imagine bolting into a rack.

- Every panel is an engraved module plate with corner screws. Buttons are
  backlit keys. Knobs are physical, with a knurled grip, a lit value arc
  and a pointer that catches the light.
- Selected buttons light up white. Green now belongs to the transport and
  the synth chrome, amber to the morph section, purple and cyan to the
  scales, as always.
- The morph bar is a proper fader. Its fill carries the whole purple to
  cyan blend under a glossy sheen, with a glowing amber position cap.
- The note screen is green glass that takes on the colour of the sounding
  side. The BPM readout is a small glass display with a drag strip.
- The keyboards feel alive. The played key presses into the bed and casts
  green light, and how hard it was hit sets how far the light reaches.
- The 88 key monitor is a real miniature keyboard now, and the lit note
  glows through the glass.
- Clearer names in TIMING: the knobs read NOTE RATE and NOTE LENGTH, and
  the morph knob reads SWEEP DURATION.
- The window opens a little smaller and tighter.

## v0.3.1 - 2026-07-08

- The transport is a drawn play/stop button, a design shared with the new
  Alea Chord Randomizer; it also remembers its state when the window
  reopens mid-playback.
- FREEZE wears an icy blue when active - its old cyan belonged to Scale B.
- PANIC calmed down: red text on a quiet button.
- The tempo bar reads "120 BPM" by itself; the app drops the TEMPO label.

## v0.3.0 - 2026-07-07

- **Root is the key**: scales now store their shape, and the root transposes
  it - load any preset, pick Root D, and it plays in D. The keyboard keeps
  the shape in place while the note letters move underneath, starting at the
  root.
- **Four synth voices** in the OUT chooser: Classic (the Alea sound, default),
  Pure Sine, Soft Saw, and Strings. Pure Sine's tails now audibly overlap.
- Note releases scale with how long each note was held - staccato snaps,
  drones ring.
- Global transpose (its own line in OUTPUT), velocity shown in the keys, the
  history, and a meter with a volume knob for the synth.
- Wordmark, Space Grotesk typography, resizable responsive window, growing
  keyboards, and an 88-key monitor with out-of-range arrows.
- Two new presets (Five -> One, Hexatonic Pole), 1/128-note timing, 2-bar
  rests.
- Check for Updates now really checks.

## v0.2.0 - 2026-07-07

First public release.

- Two scales (pitch classes, octave and velocity ranges, weighted rests,
  roots), morphing stream with auto-sweep (One-Shot / Loop / Bounce, four
  curves), scrub-during-sweep, MIDI CC learn.
- Timing: sync (4 bars to 1/128 note), free (10 ms to 60 s), or random per
  note; global transpose.
- Freeze and Panic performance controls.
- Internal synth with four voices (Warm Pad, Pure Sine, Soft Saw, Strings),
  held-time release, volume knob and output meter.
- 10 factory presets, save/load of .alea patches.
- Full monitoring: source-colored note display, 88-key strip, velocity-aware
  event history including rests.
- Resizable, responsive UI. VST3, AU, CLAP and standalone app for macOS,
  installable from a single pkg.

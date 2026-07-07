# Changelog

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

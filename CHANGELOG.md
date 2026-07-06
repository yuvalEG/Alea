# Changelog

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

# Changelog - Alea Chord Randomizer

(Scale Shifter's changelog lives in [CHANGELOG.md](CHANGELOG.md).)

## chords-v0.4.0 - 2026-07-11

The hardware faceplate update, shared with Alea Scale Shifter. Same
loop, whole new body.

- The chords sit on backlit glass pads. The sounding chord glows purple
  with its progress strip, and incoming chords glow cyan while a switch
  is pending.
- ROLL flashes when the dice actually roll, whether you pressed it, hit
  R, or AUTO rolled for you. AUTO is now a lit key under ROLL with an
  EVERY selector for its cadence of 1, 2 or 4 loops.
- Checkboxes became physical toggle switches that slide when flipped.
- The BPM readout is a small glass display. Drag it to set the tempo.
- MONITOR is a purple glass screen holding a real miniature keyboard.
  The sounding notes light up behind the glass.
- HISTORY is engraved into its panel, with metal page keys at the edges.
- The window opens at a slightly smaller size, matched in width to the
  Scale Shifter.

## chords-v0.3.0 - 2026-07-08

Voicings. How the printed chord becomes sounding notes, all in the new
VOICING row of the LOOP panel, all off by default.

- **Smooth voicing**: each chord takes the inversion that moves least
  from the previous one, so the loop connects like a keyboard player
  voicing it instead of jumping between root positions.
- **Close / open spacing**: close stacks the chord as before. Open
  spreads its notes out for an airier sound, and with several octaves
  checked it distributes them across the whole range instead of
  doubling.
- **Add bass note**: the root lands just below the voicing, like a
  bass player anchoring the chord.
- **MONITOR panel**: the keyboard now spans the full window width and
  covers C1 to C7, the whole range the voicings can reach. Make the
  window shorter to tuck it away and find the chord's notes yourself,
  a nice theory workout.
- The window opens at the same size every time, with the full keyboard
  visible. The About dialog got shorter.

## chords-v0.2.1 - 2026-07-08

- The plugin grew Scale Shifter's "No sound? Routing Help" footer link.

## chords-v0.2.0 - 2026-07-07

Into the DAW.

- **Plugin formats**: VST3, AU (macOS) and CLAP, alongside the standalone
  app - pluginval strictness 10 and auval clean. Installers gained
  selectable components on both platforms.
- **The host owns time**: in a DAW the loop locks to the host timeline -
  boundaries land exactly on bars, looping a section replays identically,
  and rolling mid-loop swaps chords in place at the next boundary. The
  transport and tempo follow the host.
- **OUT in the plugin**: "MIDI to DAW" by default (route it to any
  instrument), with the built-in synth one click away - so the AU makes
  sound even where hosts can't route plugin MIDI (Logic).
- No metronome in the plugin - DAWs bring their own.
- Play/stop transport with drawn icons, shared family design; FREEZE in
  ice blue; PANIC unified; sus rate tuned (~1 in 5); dim/aug rarer; key
  lock scale types read "Major / Minor / Harm Min".
- Fixed: Live refusing to open the VST3 (it requires a MIDI-in bus).

## chords-v0.1.0 - 2026-07-07

First release. The old Random Chord Generator's meticulously tuned chord
vocabulary, reborn in the Alea family - and it plays now.

- **Roll** a series of 1-8 chords; chord size (triads / 7ths / 9ths), sus
  chords, guitar-friendly Simplify mode. Every chord uniquely and correctly
  spelled, down to m7b5 vs dim7 and the parenthesized rarities m(Maj7) and
  dim(Maj7).
- **Key lock**: diatonic rolls in a chosen key - major, minor, or harmonic
  minor - flavors included, kept strictly in-scale.
- **The loop**: tempo, 1/2/4 bars per chord, block voicings doubled across
  chosen octaves, through the family synth (Warm Pad / Pure Sine / Soft Saw /
  Strings) or out to any MIDI device.
- **Practice flow**: auto roll every N loops, pin-and-reroll, click a chord
  to jump there, scrollable history with click-to-recall, metronome click
  with its own volume, FREEZE and PANIC.
- **Honest UI**: the sounding chord is purple, incoming chords preview in
  cyan, and nothing ever sounds except the printed chord.
- Keyboard: Space play/stop, R roll, A auto-roll.

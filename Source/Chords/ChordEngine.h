#pragma once

#include <juce_core/juce_core.h>

// The chord vocabulary of the original Random Chord Generator, ported per
// the Chord Randomizer spec section 4, extended in M3 with sus qualities,
// ninths and key lock. Every rollable chord has exactly one spelling, and
// the voicing tables are the single source of truth for what sounds.
namespace chords
{

enum class Quality { major, minor, dim, aug, sus2, sus4 };
enum class Seventh { none, seven, majSeven, sevenFlat5 };

struct Chord
{
    juce::String root;
    Quality quality = Quality::major;
    Seventh seventh = Seventh::none;
    bool ninth = false;   // 9 / m9 / Maj9 / add9 (never on dim, aug, sus, m7b5)

    juce::String text() const;
    bool operator== (const Chord& o) const
    { return root == o.root && quality == o.quality && seventh == o.seventh && ninth == o.ninth; }
};

// 21 roots: A-G in natural, flat and sharp form, minus the four evil
// enharmonic spellings (B#, Cb, E#, Fb).
const juce::StringArray& fullRoots();

// 12 guitar-friendly roots (the original app's list, plus the E it forgot).
const juce::StringArray& simpleRoots();

// Key lock scale types. Keys that would need an illegal spelling (E#, Cb,
// double sharps) are simply not offered: Major and Minor each have 11 keys,
// Harmonic Minor 8.
enum class ScaleType { major = 0, minorNatural = 1, minorHarmonic = 2 };
const juce::StringArray& scaleTypeNames();
const juce::StringArray& keyNamesFor (ScaleType);

struct RollOptions
{
    bool simplified = true;
    bool sevenths = false;
    bool sus = false;         // adds sus2/sus4 to the quality pool
    bool ninths = false;      // eligible SEVENTH chords may extend to 9ths (50%)
    bool keyLock = false;     // roll only the key's seven diatonic chords
    int scaleType = 0;        // (int) ScaleType
    int keyIndex = 0;         // into keyNamesFor (scaleType)
};

Chord roll (juce::Random&, const RollOptions&);

// Root-position block voicing for playback (spec M2): the root sits in the
// chosen octave (2, 3 or 4; octave 3 puts the root at MIDI 48-59). Voicing
// lives here at the output stage - the rolled chord itself never carries
// octave or inversion.
juce::Array<int> midiNotes (const Chord&, int octave = 3);

// M5 voicing options - strictly the output stage: only octave placement
// ever moves, never pitch classes (the standing rule; the snapshot tool's
// --vocab audit holds this to account across every combination).
struct VoicingOptions
{
    int octaveMask = 0b010;  // bits for octaves 2/3/4
    bool smooth = false;     // inversions chosen for minimal movement from the previous chord
    bool open = false;       // tones spread across the checked octaves instead of doubled
    bool bass = false;       // the root, an octave below the lowest voiced note
};

// Voice one chord of a series. `anchor` carries the previous chord's chosen
// inversion between calls (pass it in empty for the first chord): smooth
// voice-leading measures movement against it, and the inversion chosen here
// is written back for the next chord. A series is voiced once, in order, at
// adoption - every pass of the loop sounds identical and nothing drifts.
juce::Array<int> voice (const Chord&, const VoicingOptions&, juce::Array<int>& anchor);

} // namespace chords

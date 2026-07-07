#pragma once

#include <juce_core/juce_core.h>

// The chord vocabulary of the original Random Chord Generator, ported per
// the Chord Randomizer spec section 4. Full mode pairs any quality with a
// dominant or major seventh (minor also rolls 7b5); simplified mode narrows
// the roots and weights the qualities toward major/minor. Every rollable
// chord has exactly one spelling.
namespace chords
{

enum class Quality { major, minor, dim, aug };
enum class Seventh { none, seven, majSeven, sevenFlat5 };

struct Chord
{
    juce::String root;
    Quality quality = Quality::major;
    Seventh seventh = Seventh::none;

    juce::String text() const;
};

// 21 roots: A-G in natural, flat and sharp form, minus the four evil
// enharmonic spellings (B#, Cb, E#, Fb).
const juce::StringArray& fullRoots();

// 12 guitar-friendly roots (the original app's list, plus the E it forgot).
const juce::StringArray& simpleRoots();

Chord roll (juce::Random&, bool simplified, bool sevenths);

} // namespace chords

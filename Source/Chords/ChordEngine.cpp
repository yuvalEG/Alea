#include "ChordEngine.h"

namespace chords
{

juce::String Chord::text() const
{
    juce::String s = root;

    switch (quality)
    {
        case Quality::major: break;
        case Quality::minor: s << "m";   break;
        case Quality::dim:   s << "dim"; break;
        case Quality::aug:   s << "+";   break;
    }

    switch (seventh)
    {
        case Seventh::none:       break;
        case Seventh::seven:      s << "7";   break;
        case Seventh::sevenFlat5: s << "7b5"; break;
        // The rare pairings read better parenthesized: m(Maj7), dim(Maj7).
        case Seventh::majSeven:
            s << ((quality == Quality::minor || quality == Quality::dim) ? "(Maj7)" : "Maj7");
            break;
    }

    return s;
}

const juce::StringArray& fullRoots()
{
    static const juce::StringArray roots = []
    {
        juce::StringArray r;
        for (auto letter : { "A", "B", "C", "D", "E", "F", "G" })
            for (auto accidental : { "", "b", "#" })
            {
                const auto note = juce::String (letter) + accidental;
                if (! juce::StringArray { "B#", "Cb", "E#", "Fb" }.contains (note))
                    r.add (note);
            }
        return r;
    }();
    return roots;
}

const juce::StringArray& simpleRoots()
{
    static const juce::StringArray roots {
        "A", "Bb", "B", "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab" };
    return roots;
}

Chord roll (juce::Random& rng, bool simplified, bool sevenths)
{
    Chord c;

    if (simplified)
    {
        c.root = simpleRoots()[rng.nextInt (simpleRoots().size())];

        // 20% diminished, 40% minor, 40% major.
        static constexpr Quality qualities[] = {
            Quality::dim, Quality::minor, Quality::minor, Quality::major, Quality::major };
        c.quality = qualities[rng.nextInt (5)];

        if (sevenths)
            switch (c.quality)
            {
                case Quality::dim:   c.seventh = Seventh::seven; break; // always dim7
                case Quality::minor: c.seventh = rng.nextBool() ? Seventh::sevenFlat5 : Seventh::seven; break;
                default:             c.seventh = rng.nextBool() ? Seventh::seven : Seventh::majSeven; break;
            }
    }
    else
    {
        c.root = fullRoots()[rng.nextInt (fullRoots().size())];

        static constexpr Quality qualities[] = {
            Quality::dim, Quality::minor, Quality::major, Quality::aug };
        c.quality = qualities[rng.nextInt (4)];

        if (sevenths)
        {
            if (c.quality == Quality::minor)
            {
                const auto r = rng.nextInt (3);
                c.seventh = r == 0 ? Seventh::sevenFlat5
                          : r == 1 ? Seventh::seven
                                   : Seventh::majSeven;
            }
            else
            {
                c.seventh = rng.nextBool() ? Seventh::seven : Seventh::majSeven;
            }
        }
    }

    return c;
}

juce::Array<int> midiNotes (const Chord& c, int octave)
{
    // Root pitch class from the name (letter + optional accidental).
    static constexpr int letterPc[] = { 9, 11, 0, 2, 4, 5, 7 }; // A B C D E F G
    int pc = letterPc[juce::jlimit (0, 6, (int) (c.root[0] - 'A'))];
    if (c.root.endsWith ("b")) --pc;
    if (c.root.endsWith ("#")) ++pc;
    const int root = 12 * (juce::jlimit (2, 4, octave) + 1) + ((pc + 12) % 12);

    // The full quality-and-seventh interval table. Note the two cases where
    // the seventh reshapes the triad's fifth or seventh: m7b5 flattens the
    // fifth, dim7 takes the diminished (double-flat) seventh.
    juce::Array<int> intervals { 0 };
    switch (c.quality)
    {
        case Quality::major: intervals.addArray ({ 4, 7 }); break;
        case Quality::minor: intervals.addArray ({ 3, c.seventh == Seventh::sevenFlat5 ? 6 : 7 }); break;
        case Quality::dim:   intervals.addArray ({ 3, 6 }); break;
        case Quality::aug:   intervals.addArray ({ 4, 8 }); break;
    }
    switch (c.seventh)
    {
        case Seventh::none:       break;
        case Seventh::sevenFlat5: intervals.add (10); break;
        case Seventh::seven:      intervals.add (c.quality == Quality::dim ? 9 : 10); break;
        case Seventh::majSeven:   intervals.add (11); break;
    }

    juce::Array<int> notes;
    for (auto i : intervals)
        notes.add (root + i);
    return notes;
}

} // namespace chords

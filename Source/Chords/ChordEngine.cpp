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

} // namespace chords

#include "ChordEngine.h"

namespace chords
{

juce::String Chord::text() const
{
    juce::String s = root;

    // Sus chords order differently: the 7 (and only ever a dominant-style 7)
    // comes before the sus - A7sus4, not Asus47.
    if (quality == Quality::sus2 || quality == Quality::sus4)
    {
        if (seventh == Seventh::seven)
            s << "7";
        s << (quality == Quality::sus2 ? "sus2" : "sus4");
        return s;
    }

    switch (quality)
    {
        case Quality::major: break;
        case Quality::minor: s << "m";   break;
        case Quality::dim:   s << "dim"; break;
        case Quality::aug:   s << "+";   break;
        default: break;
    }

    if (ninth)
    {
        // The ninth absorbs the seventh's name: 9 / m9 / Maj9; a plain
        // triad plus ninth is an add9.
        switch (seventh)
        {
            case Seventh::none:     s << (quality == Quality::minor ? "(add9)" : "add9"); break;
            case Seventh::seven:    s << "9";    break;
            case Seventh::majSeven: s << "Maj9"; break;
            default: break; // never rolled
        }
        return s;
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

const juce::StringArray& keyNames()
{
    static const juce::StringArray keys { "C", "Db", "D", "Eb", "E", "F", "G", "Ab", "A", "Bb", "B" };
    return keys;
}

namespace
{
    // Correctly spelled major scales for the 11 offered keys (same order as
    // keyNames). Every leading tone lands on a legal spelling - the reason
    // F#/Gb is not offered.
    const char* const keyScales[11][7] = {
        { "C",  "D",  "E",  "F",  "G",  "A",  "B"  },
        { "Db", "Eb", "F",  "Gb", "Ab", "Bb", "C"  },
        { "D",  "E",  "F#", "G",  "A",  "B",  "C#" },
        { "Eb", "F",  "G",  "Ab", "Bb", "C",  "D"  },
        { "E",  "F#", "G#", "A",  "B",  "C#", "D#" },
        { "F",  "G",  "A",  "Bb", "C",  "D",  "E"  },
        { "G",  "A",  "B",  "C",  "D",  "E",  "F#" },
        { "Ab", "Bb", "C",  "Db", "Eb", "F",  "G"  },
        { "A",  "B",  "C#", "D",  "E",  "F#", "G#" },
        { "Bb", "C",  "D",  "Eb", "F",  "G",  "A"  },
        { "B",  "C#", "D#", "E",  "F#", "G#", "A#" },
    };

    Chord rollInKey (juce::Random& rng, int keyIndex, bool sevenths)
    {
        const int key = juce::jlimit (0, 10, keyIndex);
        const int degree = rng.nextInt (7);

        Chord c;
        c.root = keyScales[key][degree];

        // Diatonic qualities: I ii iii IV V vi vii(dim); with sevenths:
        // Imaj7 iim7 iiim7 IVmaj7 V7 vim7 viim7b5.
        static constexpr Quality triadQualities[7] = {
            Quality::major, Quality::minor, Quality::minor, Quality::major,
            Quality::major, Quality::minor, Quality::dim };
        c.quality = triadQualities[degree];

        if (sevenths)
        {
            switch (degree)
            {
                case 0: case 3: c.seventh = Seventh::majSeven; break;   // Imaj7, IVmaj7
                case 4:         c.seventh = Seventh::seven;    break;   // V7
                case 6:         c.quality = Quality::minor;             // viim7b5 is spelled m7b5
                                c.seventh = Seventh::sevenFlat5; break;
                default:        c.seventh = Seventh::seven;    break;   // iim7, iiim7, vim7
            }
        }
        return c;
    }
}

Chord roll (juce::Random& rng, const RollOptions& opts)
{
    if (opts.keyLock)
        return rollInKey (rng, opts.keyIndex, opts.sevenths);

    Chord c;

    // Roots and quality pool; the sus toggle widens the pool in both modes.
    juce::Array<Quality> qualities;
    if (opts.simplified)
    {
        c.root = simpleRoots()[rng.nextInt (simpleRoots().size())];
        qualities = { Quality::dim, Quality::minor, Quality::minor, Quality::major, Quality::major };
    }
    else
    {
        c.root = fullRoots()[rng.nextInt (fullRoots().size())];
        qualities = { Quality::dim, Quality::minor, Quality::major, Quality::aug };
    }
    if (opts.sus)
        qualities.addArray ({ Quality::sus2, Quality::sus4 });
    c.quality = qualities[rng.nextInt (qualities.size())];

    if (opts.sevenths)
    {
        switch (c.quality)
        {
            case Quality::sus2:
            case Quality::sus4:
                c.seventh = Seventh::seven; // 7sus2 / 7sus4, always dominant-style
                break;
            case Quality::dim:
                c.seventh = opts.simplified ? Seventh::seven // dim7 only, as always
                                            : (rng.nextBool() ? Seventh::seven : Seventh::majSeven);
                break;
            case Quality::minor:
                if (opts.simplified)
                    c.seventh = rng.nextBool() ? Seventh::sevenFlat5 : Seventh::seven;
                else
                {
                    const auto r = rng.nextInt (3);
                    c.seventh = r == 0 ? Seventh::sevenFlat5 : r == 1 ? Seventh::seven : Seventh::majSeven;
                }
                break;
            default: // major, aug
                c.seventh = rng.nextBool() ? Seventh::seven : Seventh::majSeven;
                break;
        }
    }

    // Ninths, strictly on the chords where they read cleanly: 9 (dom),
    // m9, Maj9, and add9 on plain major/minor triads. Never on dim, aug,
    // sus, m7b5 or m(Maj7).
    if (opts.ninths && (c.quality == Quality::major || c.quality == Quality::minor))
    {
        const bool eligible = c.seventh == Seventh::none
                           || c.seventh == Seventh::seven
                           || (c.seventh == Seventh::majSeven && c.quality == Quality::major);
        if (eligible && rng.nextBool())
            c.ninth = true;
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

    // The full interval table. Note the two cases where the seventh
    // reshapes the chord: m7b5 flattens the fifth, dim7 takes the
    // diminished (double-flat) seventh.
    juce::Array<int> intervals { 0 };
    switch (c.quality)
    {
        case Quality::major: intervals.addArray ({ 4, 7 }); break;
        case Quality::minor: intervals.addArray ({ 3, c.seventh == Seventh::sevenFlat5 ? 6 : 7 }); break;
        case Quality::dim:   intervals.addArray ({ 3, 6 }); break;
        case Quality::aug:   intervals.addArray ({ 4, 8 }); break;
        case Quality::sus2:  intervals.addArray ({ 2, 7 }); break;
        case Quality::sus4:  intervals.addArray ({ 5, 7 }); break;
    }
    switch (c.seventh)
    {
        case Seventh::none:       break;
        case Seventh::sevenFlat5: intervals.add (10); break;
        case Seventh::seven:      intervals.add (c.quality == Quality::dim ? 9 : 10); break;
        case Seventh::majSeven:   intervals.add (11); break;
    }
    if (c.ninth)
        intervals.add (14);

    juce::Array<int> notes;
    for (auto i : intervals)
        notes.add (root + i);
    return notes;
}

} // namespace chords

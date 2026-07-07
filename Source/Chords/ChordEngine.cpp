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

const juce::StringArray& scaleTypeNames()
{
    static const juce::StringArray types { "Major", "Minor", "Harm. minor" };
    return types;
}

namespace
{
    // Correctly spelled major scales for the 11 offered keys. Every leading
    // tone lands on a legal spelling - the reason F#/Gb is not offered.
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

    // Harmonic minor is offered only where the raised leading tone spells
    // legally (A minor raises G to G#; G# minor would need F##). Values are
    // indices into the major table of each tonic's RELATIVE major.
    constexpr int harmonicToMajor[8] = { 0, 1, 2, 3, 5, 6, 7, 9 }; // A Bb B C D E F G

    int pcOf (const juce::String& name)
    {
        static constexpr int letterPc[] = { 9, 11, 0, 2, 4, 5, 7 }; // A B C D E F G
        int pc = letterPc[juce::jlimit (0, 6, (int) (name[0] - 'A'))];
        if (name.endsWith ("b")) --pc;
        if (name.endsWith ("#")) ++pc;
        return (pc + 12) % 12;
    }

    struct Degree
    {
        juce::String root;
        Quality triad;        // quality without sevenths
        Quality with7;        // quality when sevenths are on (m7b5 spells as minor)
        Seventh seventh;
    };

    void buildKey (ScaleType type, int keyIndex, Degree out[7])
    {
        static constexpr Quality majTriad[7] = {
            Quality::major, Quality::minor, Quality::minor, Quality::major,
            Quality::major, Quality::minor, Quality::dim };
        static constexpr Quality majWith7[7] = {
            Quality::major, Quality::minor, Quality::minor, Quality::major,
            Quality::major, Quality::minor, Quality::minor }; // vii7 = m7b5
        static constexpr Seventh majSeventh[7] = {
            Seventh::majSeven, Seventh::seven, Seventh::seven, Seventh::majSeven,
            Seventh::seven, Seventh::seven, Seventh::sevenFlat5 };

        // Natural minor is the relative major rotated to start at its 6th
        // degree; the minor key lists are aligned so keyIndex maps straight
        // onto the relative major's table row.
        const bool minor = type != ScaleType::major;
        const int majorKey = juce::jlimit (0, 10,
            type == ScaleType::minorHarmonic ? harmonicToMajor[juce::jlimit (0, 7, keyIndex)]
                                             : keyIndex);
        const int rotate = minor ? 5 : 0;

        for (int i = 0; i < 7; ++i)
        {
            const int j = (i + rotate) % 7;
            out[i].root = keyScales[majorKey][j];
            out[i].triad = majTriad[j];
            out[i].with7 = majWith7[j];
            out[i].seventh = majSeventh[j];
        }

        if (type == ScaleType::minorHarmonic)
        {
            // Raise the leading tone (strip a flat or add a sharp), then
            // re-quality: i(mMaj7) iim7b5 III+(Maj7) ivm7 V7 VImaj7 viidim7.
            auto& leading = out[6].root;
            leading = leading.endsWith ("b") ? leading.dropLastCharacters (1) : leading + "#";

            static constexpr Quality hTriad[7] = {
                Quality::minor, Quality::dim, Quality::aug, Quality::minor,
                Quality::major, Quality::major, Quality::dim };
            static constexpr Quality hWith7[7] = {
                Quality::minor, Quality::minor, Quality::aug, Quality::minor,
                Quality::major, Quality::major, Quality::dim };
            static constexpr Seventh hSeventh[7] = {
                Seventh::majSeven, Seventh::sevenFlat5, Seventh::majSeven, Seventh::seven,
                Seventh::seven, Seventh::majSeven, Seventh::seven };
            for (int i = 0; i < 7; ++i)
            {
                out[i].triad = hTriad[i];
                out[i].with7 = hWith7[i];
                out[i].seventh = hSeventh[i];
            }
        }
    }

    // Key-locked rolls, flavors included - everything stays strictly in the
    // scale. Sus variants require a diatonic perfect fifth plus the 2/4;
    // 7sus needs the diatonic minor seventh; ninths need the diatonic ninth
    // (which is exactly what rules out the b9 degrees).
    Chord rollInKey (juce::Random& rng, const RollOptions& opts)
    {
        Degree degrees[7];
        buildKey ((ScaleType) opts.scaleType, opts.keyIndex, degrees);

        int scaleMask = 0;
        for (auto& d : degrees)
            scaleMask |= 1 << pcOf (d.root);
        auto inScale = [scaleMask] (int p) { return ((scaleMask >> (p % 12)) & 1) != 0; };

        const auto& d = degrees[rng.nextInt (7)];
        const int pc = pcOf (d.root);

        Chord c;
        c.root = d.root;
        c.quality = opts.sevenths ? d.with7 : d.triad;
        c.seventh = opts.sevenths ? d.seventh : Seventh::none;

        // Sus swap, ~1 roll in 5: needs an in-scale perfect fifth plus the
        // 2/4; the 7sus form needs the in-scale minor seventh.
        if (opts.sus && rng.nextInt (5) == 0 && inScale (pc + 7))
        {
            juce::Array<Quality> legal;
            if (inScale (pc + 2)) legal.add (Quality::sus2);
            if (inScale (pc + 5)) legal.add (Quality::sus4);
            if (! legal.isEmpty())
            {
                c.quality = legal[rng.nextInt (legal.size())];
                c.seventh = (opts.sevenths && inScale (pc + 10)) ? Seventh::seven : Seventh::none;
            }
        }

        if (opts.ninths && inScale (pc + 2)
            && (c.quality == Quality::major || c.quality == Quality::minor)
            && (c.seventh == Seventh::seven
                || (c.seventh == Seventh::majSeven && c.quality == Quality::major))
            && rng.nextBool())
            c.ninth = true;

        return c;
    }
}

const juce::StringArray& keyNamesFor (ScaleType type)
{
    static const juce::StringArray majors { "C", "Db", "D", "Eb", "E", "F", "G", "Ab", "A", "Bb", "B" };
    static const juce::StringArray minors = []
    {
        juce::StringArray a;
        for (int k = 0; k < 11; ++k)
            a.add (keyScales[k][5]); // the relative minor tonic
        return a;
    }();
    static const juce::StringArray harmonics { "A", "Bb", "B", "C", "D", "E", "F", "G" };

    switch (type)
    {
        case ScaleType::minorNatural:  return minors;
        case ScaleType::minorHarmonic: return harmonics;
        default:                       return majors;
    }
}

Chord roll (juce::Random& rng, const RollOptions& opts)
{
    if (opts.keyLock)
        return rollInKey (rng, opts);

    Chord c;

    // Roots and quality pool (sus arrives as a post-step, below). The odd
    // qualities are deliberately rare: dim ~14% simplified; dim and aug
    // 12.5% each in full mode (tuned down from uniform in QA).
    juce::Array<Quality> qualities;
    if (opts.simplified)
    {
        c.root = simpleRoots()[rng.nextInt (simpleRoots().size())];
        qualities = { Quality::dim,
                      Quality::minor, Quality::minor, Quality::minor,
                      Quality::major, Quality::major, Quality::major };
    }
    else
    {
        c.root = fullRoots()[rng.nextInt (fullRoots().size())];
        qualities = { Quality::dim, Quality::aug,
                      Quality::minor, Quality::minor, Quality::minor,
                      Quality::major, Quality::major, Quality::major };
    }
    c.quality = qualities[rng.nextInt (qualities.size())];

    if (opts.sevenths)
    {
        switch (c.quality)
        {
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

    // Sus swap, ~1 roll in 5 (a pool slot made sus half the dice - QA).
    if (opts.sus && rng.nextInt (5) == 0)
    {
        c.quality = rng.nextBool() ? Quality::sus2 : Quality::sus4;
        c.seventh = opts.sevenths ? Seventh::seven : Seventh::none; // 7sus, dominant-style
    }

    // Ninths, strictly on the seventh chords where they read cleanly:
    // 9 (dom), m9, Maj9. A ninth presumes its seventh (Yuval's rule), so
    // without the sevenths toggle nothing here fires. Never on dim, aug,
    // sus, m7b5 or m(Maj7).
    if (opts.ninths && (c.quality == Quality::major || c.quality == Quality::minor))
    {
        const bool eligible = c.seventh == Seventh::seven
                           || (c.seventh == Seventh::majSeven && c.quality == Quality::major);
        if (eligible && rng.nextBool())
            c.ninth = true;
    }

    return c;
}

juce::Array<int> midiNotes (const Chord& c, int octave)
{
    const int root = 12 * (juce::jlimit (2, 4, octave) + 1) + pcOf (c.root);

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

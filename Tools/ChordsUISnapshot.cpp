// Dev tool: renders the Chord Randomizer editor into a PNG so layout can be
// checked without launching the app.
// Usage: ChordsUISnapshot <out.png> [rolls] [sevenths 0/1] [simplify 0/1]
//                         [playing 0/1] [width] [height]
// (rolls > 0 pre-rolls the dice so the history ticker fills; playing=1
// runs the loop for a few seconds first so the shot looks mid-playing.)
// Or:    ChordsUISnapshot --vocab
// prints every chord the engine can roll (empirically, on root C where
// applicable) with its intervals - the music-theory QA table - then audits
// every voicing option combination (M5) against the standing rule: nothing
// may ever sound outside the printed chord. Non-zero exit on violation.
#include "../Source/Chords/ChordsEditor.h"
#include <iostream>
#include <map>
#include <vector>

// M5 voicing audit: for every rollable chord (all modes, real roots), every
// voicing option combination and every octave mask, no sounded pitch class
// may leave the printed chord, and the bass must be the root. Smooth
// voice-leading is exercised by probing single-note anchors across the
// range, which forces every inversion the movement metric can choose.
static int auditVoicings()
{
    juce::Random rng (7);
    std::vector<chords::Chord> pool;
    std::map<std::string, bool> seen;
    auto collect = [&] (const chords::RollOptions& o, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            auto c = chords::roll (rng, o);
            if (! seen[c.text().toStdString()])
            {
                seen[c.text().toStdString()] = true;
                pool.push_back (c);
            }
        }
    };
    for (const bool simplified : { false, true })
        for (const bool sevenths : { false, true })
            for (const bool flavors : { false, true })
            {
                chords::RollOptions o;
                o.simplified = simplified;
                o.sevenths = sevenths;
                o.sus = o.ninths = flavors;
                collect (o, 20000);
            }
    for (int t = 0; t < chords::scaleTypeNames().size(); ++t)
        for (int k = 0; k < chords::keyNamesFor ((chords::ScaleType) t).size(); ++k)
        {
            chords::RollOptions o;
            o.keyLock = true;
            o.scaleType = t;
            o.keyIndex = k;
            o.sevenths = o.sus = o.ninths = true;
            collect (o, 4000);
        }

    long checked = 0;
    int violations = 0;
    for (const auto& c : pool)
    {
        int chordMask = 0;
        for (int n : chords::midiNotes (c, 3))
            chordMask |= 1 << (n % 12);
        const int rootPc = chords::midiNotes (c, 3).getFirst() % 12;

        for (int mask = 1; mask <= 7; ++mask)
            for (const bool smooth : { false, true })
                for (const bool open : { false, true })
                    for (const bool bass : { false, true })
                        for (int probe = 34; probe <= 70; probe += 3)
                        {
                            chords::VoicingOptions v;
                            v.octaveMask = mask;
                            v.smooth = smooth;
                            v.open = open;
                            v.bass = bass;

                            juce::Array<int> anchor;
                            if (probe > 34) // 34 itself = the empty anchor (first chord of a series)
                                anchor.add (probe);
                            const auto notes = chords::voice (c, v, anchor);
                            ++checked;

                            if (bass && (notes.isEmpty() || notes.getFirst() % 12 != rootPc))
                            {
                                ++violations;
                                std::cout << "BASS VIOLATION: " << c.text() << " mask " << mask << "\n";
                            }
                            for (int n : notes)
                                if (((chordMask >> (n % 12)) & 1) == 0)
                                {
                                    ++violations;
                                    std::cout << "PITCH VIOLATION: " << c.text() << " sounds MIDI " << n
                                              << " (mask " << mask << (smooth ? " smooth" : "")
                                              << (open ? " open" : "") << (bass ? " bass" : "") << ")\n";
                                }
                        }
    }

    // A human-readable sample: the ear-check table for smooth voice-leading.
    auto demo = [] (const char* title, const std::vector<chords::Chord>& prog, chords::VoicingOptions v)
    {
        static const char* names[12] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
        std::cout << title << ":\n";
        juce::Array<int> anchor;
        for (const auto& c : prog)
        {
            std::cout << "  " << c.text().paddedRight (' ', 8).toStdString() << " ->";
            for (int n : chords::voice (c, v, anchor))
                std::cout << " " << names[n % 12] << n / 12 - 1;
            std::cout << "\n";
        }
    };
    const std::vector<chords::Chord> cadence {
        { "C", chords::Quality::major }, { "F", chords::Quality::major },
        { "G", chords::Quality::major }, { "C", chords::Quality::major } };
    chords::VoicingOptions plain;   plain.octaveMask = 0b010;
    auto smooth = plain;  smooth.smooth = true;
    auto full = smooth;   full.open = true; full.bass = true;
    std::cout << "\n== VOICING DEMO (C F G C, octave 3) ==\n";
    demo ("close, root position", cadence, plain);
    demo ("smooth voice-leading", cadence, smooth);
    demo ("smooth + open + bass", cadence, full);

    std::cout << "\n== VOICING AUDIT ==\n"
              << pool.size() << " chord types, " << checked << " voicings checked, "
              << violations << " violations\n";
    return violations == 0 ? 0 : 3;
}

static int dumpVocabulary()
{
    juce::Random rng (42);

    auto signature = [] (const chords::Chord& c)
    {
        auto notes = chords::midiNotes (c, 3);
        juce::String line = c.text().paddedRight (' ', 12) + " intervals:";
        for (auto n : notes)
            line << " " << (n - notes[0]);
        return line;
    };

    // Free modes: roll a lot, collect unique chord TYPES (normalized to C).
    for (const bool simplified : { false, true })
    {
        std::map<std::string, std::string> unique;
        for (int sevenths = 0; sevenths < 2; ++sevenths)
            for (int flavors = 0; flavors < 2; ++flavors)
                for (int i = 0; i < 40000; ++i)
                {
                    chords::RollOptions o;
                    o.simplified = simplified;
                    o.sevenths = sevenths != 0;
                    o.sus = o.ninths = flavors != 0;
                    auto c = chords::roll (rng, o);
                    c.root = "C"; // type table, not root table
                    unique[c.text().toStdString()] = signature (c).toStdString();
                }
        std::cout << (simplified ? "\n== SIMPLIFIED mode types ==\n" : "\n== FULL mode types ==\n");
        for (auto& [name, sig] : unique)
            std::cout << sig << "\n";
    }

    // Key lock: the exact diatonic sets per scale type and key - triads,
    // sevenths, and the full flavored set (sevenths + sus + ninths).
    for (int t = 0; t < chords::scaleTypeNames().size(); ++t)
    {
        std::cout << "\n== KEY LOCK - " << chords::scaleTypeNames()[t]
                  << " (triads | sevenths | +sus+9) ==\n";
        const auto keys = chords::keyNamesFor ((chords::ScaleType) t);
        for (int k = 0; k < keys.size(); ++k)
        {
            std::map<std::string, bool> triads, sevs, flavored;
            for (int i = 0; i < 12000; ++i)
            {
                chords::RollOptions o;
                o.keyLock = true;
                o.scaleType = t;
                o.keyIndex = k;
                triads[chords::roll (rng, o).text().toStdString()] = true;
                o.sevenths = true;
                sevs[chords::roll (rng, o).text().toStdString()] = true;
                o.sus = o.ninths = true;
                flavored[chords::roll (rng, o).text().toStdString()] = true;
            }
            std::cout << keys[k].paddedRight (' ', 3) << ": ";
            for (auto& [c, _] : triads) std::cout << c << " ";
            std::cout << " | ";
            for (auto& [c, _] : sevs) std::cout << c << " ";
            std::cout << " | ";
            for (auto& [c, _] : flavored) std::cout << c << " ";
            std::cout << "\n";
        }
    }
    return auditVoicings();
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    if (argc > 1 && juce::String (argv[1]) == "--vocab")
        return dumpVocabulary();

    ChordsProcessor processor;
    processor.useSevenths = argc > 3 && juce::String (argv[3]).getIntValue() != 0;
    processor.simplify    = argc > 4 && juce::String (argv[4]).getIntValue() != 0;
    for (int i = argc > 2 ? juce::String (argv[2]).getIntValue() : 0; i > 0; --i)
        processor.rollSeries();

    // A "mid-playing" pose: run the loop offline into the second chord, then
    // leave the transport on so the editor paints the purple card, progress
    // strip and lit monitor keys.
    const bool playingShot = argc > 5 && juce::String (argv[5]).getIntValue() != 0;
    if (playingShot)
    {
        processor.setPlayConfigDetails (0, 2, 44100.0, 512);
        processor.prepareToPlay (44100.0, 512);
        processor.playing.store (true);
        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;
        for (int block = 0; block < 380; ++block)
            processor.processBlock (buffer, midi);
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    if (argc > 7)
        editor->setSize (juce::jmax (600, juce::String (argv[6]).getIntValue()),
                         juce::jmax (400, juce::String (argv[7]).getIntValue()));
    if (playingShot) // let the 30 Hz timer pick the playing state up
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);

    const auto path = argc > 1 ? juce::String (argv[1]) : juce::String ("chords_ui_snapshot.png");
    juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile (path);
    out.deleteFile();

    juce::FileOutputStream stream (out);
    if (! stream.openedOk())
        return 1;

    juce::PNGImageFormat png;
    if (! png.writeImageToStream (image, stream))
        return 1;

    if (playingShot)
        return 0; // posed shot: the loop already ran, skip the smoke test

    // Audio smoke test: run the loop offline for ~4 seconds and report the
    // peak level - proof the transport schedules chords and the synth sounds.
    processor.setPlayConfigDetails (0, 2, 44100.0, 512); // what the wrapper would do
    processor.prepareToPlay (44100.0, 512);
    processor.playing.store (true);
    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    float peak = 0.0f;
    for (int block = 0; block < 350; ++block)
    {
        processor.processBlock (buffer, midi);
        peak = juce::jmax (peak, buffer.getMagnitude (0, 512));
    }
    processor.playing.store (false);
    processor.processBlock (buffer, midi); // note-offs on stop
    std::cout << "audio peak over 4s of playback: " << peak << std::endl;
    return peak > 0.01f ? 0 : 2;
}

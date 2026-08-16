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

// Regression check for the ear workout's history leak (QA Aug 14, 2026).
// A mid-loop roll files the still-sounding series into history before it
// stops playing, so THAT entry has to hide. The first version assumed it was
// always the newest entry, which is wrong the moment you roll twice before
// the boundary: the newest is then a roll that never sounded, and the one
// that IS sounding sits behind it - fully readable, which is the answer.
static int earHistoryTest()
{
    ChordsProcessor processor;
    processor.setPlayConfigDetails (0, 2, 44100.0, 512);
    processor.prepareToPlay (44100.0, 512);
    processor.playing.store (true);

    const auto sounding = processor.series;   // what the loop is on

    processor.rollSeries();                   // roll 1: files the sounding series
    processor.rollSeries();                   // roll 2, before any boundary

    int failures = 0;
    auto check = [&failures] (const char* what, bool ok)
    {
        std::cout << (ok ? "OK   " : "FAIL ") << what << "\n";
        failures += ok ? 0 : 1;
    };

    check ("the sounding series is still the pending one",
           processor.pendingOldSeries == sounding);
    check ("history holds both rolls", processor.history.size() >= 2);

    // The bug, stated as a test: the sounding series is NOT the newest entry.
    check ("the newest history entry is NOT the sounding one",
           ! processor.history.empty() && processor.history.front() != sounding);

    // The behaviour the fix actually depends on, asserted where it lives.
    const int found = processor.soundingHistoryIndex();
    check ("soundingHistoryIndex points behind the newest entry", found == 1);
    std::cout << "  (sounding series sits at history index " << found << ")\n";

    std::cout << (failures == 0 ? "ear-workout history OK\n" : "ear-workout history BROKEN\n");
    return failures == 0 ? 0 : 6;
}

// Store screenshots are posed by the app itself, from "--pose <name>", so the
// pipeline survives relayout instead of depending on tapped coordinates. If a
// pose stops setting what its shot is meant to show, the store quietly starts
// advertising the wrong thing - so each one is asserted here.
static int posesTest()
{
    int failures = 0;
    auto check = [&failures] (const char* what, bool ok)
    {
        std::cout << (ok ? "OK   " : "FAIL ") << what << "\n";
        failures += ok ? 0 : 1;
    };

    {   // every shot is mid-loop; a stopped app photographs as a mock-up
        ChordsProcessor p;
        p.applyScreenshotPose ("chords-01");
        check ("chords-01 is playing", p.playing.load());
        check ("chords-01 is not in the ear workout", ! p.earMode.load());
    }
    {   ChordsProcessor p;
        p.applyScreenshotPose ("chords-02");
        check ("chords-02 turns the ear workout on", p.earMode.load() && p.playing.load());
    }
    {   ChordsProcessor p;
        p.applyScreenshotPose ("chords-03");
        check ("chords-03 locks the key", p.keyLockOn && p.playing.load());
    }
    {   ChordsProcessor p;
        p.applyScreenshotPose ("chords-04");
        check ("chords-04 shows the voicing options",
               p.openVoicing.load() && p.bassNote.load() && p.smoothVoicing.load());
    }
    {   ChordsProcessor p;
        p.applyScreenshotPose ("chords-05");
        check ("chords-05 rolls the hardest chords the app can",
               ! p.simplify && p.useSevenths && p.ninthsOn && p.susOn);
    }
    {   ChordsProcessor p;
        const bool wasPlaying = p.playing.load();
        p.applyScreenshotPose ("");
        check ("an empty pose changes nothing", p.playing.load() == wasPlaying);
    }

    std::cout << (failures == 0 ? "chords poses OK\n" : "chords poses BROKEN\n");
    return failures == 0 ? 0 : 7;
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    if (argc > 1 && juce::String (argv[1]) == "--vocab")
        return dumpVocabulary();
    if (argc > 1 && juce::String (argv[1]) == "--eartest")
        return earHistoryTest();
    if (argc > 1 && juce::String (argv[1]) == "--posetest")
        return posesTest();

    ChordsProcessor processor;
    processor.useSevenths = argc > 3 && juce::String (argv[3]).getIntValue() != 0;
    processor.simplify    = argc > 4 && juce::String (argv[4]).getIntValue() != 0;
    for (int i = argc > 2 ? juce::String (argv[2]).getIntValue() : 0; i > 0; --i)
        processor.rollSeries();

    // A "mid-playing" pose: run the loop offline into the second chord, then
    // leave the transport on so the editor paints the purple card, progress
    // strip and lit monitor keys.
    // "ear" anywhere in the args poses the ear workout (spec M6): every card
    // showing its three dots and the monitor dark, which is the mode as you
    // actually meet it. Revealing is a per-card thing the editor owns, and
    // posing one open would mean adding API for a screenshot.
    bool earShot = false;
    for (int i = 2; i < argc; ++i)
        if (juce::String (argv[i]) == "ear")
            earShot = true;
    processor.earMode.store (earShot);

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

    // Audio smoke test: run the loop offline through EVERY internal sound
    // and report each peak - proof the transport schedules chords and every
    // flavour, the sampled piano included, actually makes sound.
    processor.setPlayConfigDetails (0, 2, 44100.0, 512); // what the wrapper would do
    processor.prepareToPlay (44100.0, 512);
    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    bool allSounding = true;
    for (const auto& f : alea::flavourTable())
    {
        processor.setStandaloneOutput (f.choice);
        processor.playing.store (true);
        float peak = 0.0f;
        for (int block = 0; block < 130; ++block) // ~1.5 s per flavour
        {
            processor.processBlock (buffer, midi);
            peak = juce::jmax (peak, buffer.getMagnitude (0, 512));
        }
        processor.playing.store (false);
        processor.processBlock (buffer, midi); // note-offs between flavours
        std::cout << juce::String (f.name).paddedRight (' ', 10) << " peak: " << peak
                  << (peak > 0.01f ? "" : "   <-- SILENT") << std::endl;
        allSounding = allSounding && peak > 0.01f;
    }
    return allSounding ? 0 : 2;
}

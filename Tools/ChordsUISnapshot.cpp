// Dev tool: renders the Chord Randomizer editor into a PNG so layout can be
// checked without launching the app.
// Usage: ChordsUISnapshot <out.png> [rolls] [sevenths 0/1] [simplify 0/1]
// (rolls > 0 pre-rolls the dice that many times so the history ticker fills.)
// Or:    ChordsUISnapshot --vocab
// prints every chord the engine can roll (empirically, on root C where
// applicable) with its intervals - the music-theory QA table.
#include "../Source/Chords/ChordsEditor.h"
#include <iostream>
#include <map>

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

    // Key lock: the exact diatonic sets, every key, triads and sevenths.
    std::cout << "\n== KEY LOCK (triads | sevenths) ==\n";
    for (int k = 0; k < chords::keyNames().size(); ++k)
    {
        std::map<std::string, bool> triads, sevs;
        for (int i = 0; i < 4000; ++i)
        {
            chords::RollOptions o;
            o.keyLock = true;
            o.keyIndex = k;
            o.sevenths = false;
            triads[chords::roll (rng, o).text().toStdString()] = true;
            o.sevenths = true;
            sevs[chords::roll (rng, o).text().toStdString()] = true;
        }
        std::cout << chords::keyNames()[k].paddedRight (' ', 3) << ": ";
        for (auto& [t, _] : triads) std::cout << t << " ";
        std::cout << " | ";
        for (auto& [t, _] : sevs) std::cout << t << " ";
        std::cout << "\n";
    }
    return 0;
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

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

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

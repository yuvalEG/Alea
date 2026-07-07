// Dev tool: renders the Chord Randomizer editor into a PNG so layout can be
// checked without launching the app.
// Usage: ChordsUISnapshot <out.png> [rolls] [sevenths 0/1] [simplify 0/1]
// (rolls > 0 pre-rolls the dice that many times so the history ticker fills.)
#include "../Source/Chords/ChordsEditor.h"

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

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

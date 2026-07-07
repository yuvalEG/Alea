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
    return png.writeImageToStream (image, stream) ? 0 : 1;
}

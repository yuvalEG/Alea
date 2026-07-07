// Dev tool: renders the plugin editor into a PNG so layout can be checked
// without loading the plugin in a host. Usage: AleaUISnapshot <out.png>
//
// The shot is posed mid-performance for the README: the Major -> Minor
// preset, morph parked at 70%, a fake host playhead running long enough to
// fill the history with notes from both scales, and a note sounding.
#include "../Source/PluginEditor.h"
#include "../Source/Presets.h"

namespace
{
    struct FakePlayHead : juce::AudioPlayHead
    {
        double ppq = 0.0;
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo p;
            p.setIsPlaying (true);
            p.setBpm (110.0);
            p.setPpqPosition (ppq);
            return p;
        }
    };
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    AleaAudioProcessor processor;
    processor.setPlayConfigDetails (0, 2, 44100.0, 512);
    processor.prepareToPlay (44100.0, 512);

    // Major -> Minor, sweep off, morph posed by hand.
    presets::apply (processor.apvts, presets::factory()[6]);
    processor.currentPreset.store (6); // keep its bubble lit
    if (auto* sweep = processor.apvts.getParameter ("autoSweep"))
        sweep->setValueNotifyingHost (0.0f);
    auto* morph = processor.apvts.getParameter ("morphPos");

    FakePlayHead playHead;
    processor.setPlayHead (&playHead);
    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    auto pump = [&]
    {
        processor.processBlock (buffer, midi);
        midi.clear();
        playHead.ppq += 512.0 / 44100.0 * (110.0 / 60.0);
    };

    // Two acts, so the history holds notes from BOTH scales: a stretch near
    // A (purple entries), then the pose position at 70% (cyan takes over).
    if (morph != nullptr)
        morph->setValueNotifyingHost (morph->convertTo0to1 (25.0f));
    for (int block = 0; block < 1400; ++block)
        pump();
    if (morph != nullptr)
        morph->setValueNotifyingHost (morph->convertTo0to1 (70.0f));
    for (int block = 0; block < 170; ++block) // a couple of notes at 70%, so the
        pump();                               // visible history keeps both colors
    for (int block = 0; block < 500 && processor.activeNote.load() < 0; ++block)
        pump(); // land on a sounding note

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    juce::MessageManager::getInstance()->runDispatchLoopUntil (300); // timers paint the live state

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
    editor.reset();
    processor.setPlayHead (nullptr);

    const auto path = argc > 1 ? juce::String (argv[1]) : juce::String ("ui_snapshot.png");
    juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile (path);
    out.deleteFile();

    juce::FileOutputStream stream (out);
    if (! stream.openedOk())
        return 1;

    juce::PNGImageFormat png;
    return png.writeImageToStream (image, stream) ? 0 : 1;
}

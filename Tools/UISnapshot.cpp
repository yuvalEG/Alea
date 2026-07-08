// Dev tool: renders the plugin editor into a PNG so layout can be checked
// without loading the plugin in a host. Usage: AleaUISnapshot <out.png>
//
// The shot is posed mid-performance for the README: the Major -> Minor
// preset, morph parked at 70%, a fake host playhead running long enough to
// fill the history with notes from both scales, and a note sounding.
#include "../Source/PluginEditor.h"
#include "../Source/Presets.h"
#include <iostream>

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

// Regression check for the "switch preset mid-sweep bleeds the morph into the
// next preset" bug: sweep a preset up, switch to another, and confirm the new
// preset's morph starts fresh at A instead of inheriting the old position.
static int morphSwitchTest()
{
    AleaAudioProcessor processor;
    processor.setPlayConfigDetails (0, 2, 44100.0, 512);
    processor.prepareToPlay (44100.0, 512);

    FakePlayHead playHead;
    processor.setPlayHead (&playHead);
    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    auto pump = [&] (int blocks)
    {
        for (int i = 0; i < blocks; ++i)
        {
            processor.processBlock (buffer, midi);
            midi.clear();
            playHead.ppq += 512.0 / 44100.0 * (110.0 / 60.0);
        }
    };
    auto applyPreset = [&] (int idx) // mirrors the editor's applyPresetAndMark
    {
        presets::apply (processor.apvts, presets::factory()[(size_t) idx]);
        processor.presetReanchor.store (true);
    };

    // Pentatonic Drift (index 2): a 16-bar Loop sweep. Run it a few bars in.
    applyPreset (2);
    pump (900);
    const double midMorph = processor.morphPercent.load();

    // Now switch presets mid-sweep, to another sweeping preset and to a
    // static one, and check the morph restarts at A each time.
    int failures = 0;
    for (int idx : { 3 /*Octave Climb, sweeps*/, 6 /*Major->Minor, static*/, 2 /*back to a sweep*/ })
    {
        applyPreset (idx);
        pump (2); // one block to consume the re-anchor, one to settle
        const double after = processor.morphPercent.load();
        const bool ok = after < 2.0; // fresh presets start at A (0%); allow a block of travel
        std::cout << "switch to preset " << idx << ": morph " << after
                  << "% " << (ok ? "OK" : "<-- BLED IN") << "\n";
        failures += ok ? 0 : 1;
    }

    std::cout << "(mid-sweep morph before switching was " << midMorph << "%)\n";
    processor.setPlayHead (nullptr);
    return failures == 0 ? 0 : 3;
}

// Renders the real OUT ComboBox popup (grouped SYNTH / INSTRUMENT sections)
// to a PNG so the section-header styling can be eyeballed without opening the
// app and clicking the menu.
static int menuShot (const juce::String& path)
{
    AleaAudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (940, 704); // realises the editor so its LookAndFeel is live

    juce::ComboBox box;
    box.setLookAndFeel (&editor->getLookAndFeel());
    for (int group : { alea::groupSynth, alea::groupInstrument })
    {
        box.addSectionHeading (alea::groupName (group));
        for (const auto& f : alea::flavourTable())
            if (f.group == group)
                box.addItem (f.name, 1 + f.flavour);
    }

    auto& menu = *box.getRootMenu();
    juce::Image image (juce::Image::ARGB, 300, 460, true);
    {
        juce::Graphics g (image);
        g.fillAll (juce::Colour (0xff12121a));
        // Draw each row through the live LookAndFeel, headers included.
        int y = 8;
        auto& lf = editor->getLookAndFeel();
        for (juce::PopupMenu::MenuItemIterator it (menu); it.next();)
        {
            const auto& item = it.getItem();
            const juce::Rectangle<int> area (8, y, 284, item.isSectionHeader ? 28 : 30);
            if (item.isSectionHeader)
                lf.drawPopupMenuSectionHeader (g, area, item.text);
            else
            {
                g.setColour (juce::Colour (0xffe8e8f0));
                g.setFont (juce::FontOptions (16.0f));
                g.drawText (item.text, area.reduced (14, 0), juce::Justification::centredLeft);
            }
            y += area.getHeight();
        }
    }
    editor.reset();

    juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile (path);
    out.deleteFile();
    juce::FileOutputStream stream (out);
    juce::PNGImageFormat png;
    return (stream.openedOk() && png.writeImageToStream (image, stream)) ? 0 : 1;
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    if (argc > 1 && juce::String (argv[1]) == "--morphtest")
        return morphSwitchTest();
    if (argc > 2 && juce::String (argv[1]) == "--menushot")
        return menuShot (argv[2]);

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

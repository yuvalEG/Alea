#include "PluginEditor.h"
#include "Presets.h"

using namespace ui;

namespace
{
    constexpr int kWidth = 940, kHeight = 646;

    const juce::Rectangle<int> scaleAPanel   { 10,  64, 455, 240 };
    const juce::Rectangle<int> scaleBPanel   { 475, 64, 455, 240 };
    const juce::Rectangle<int> timingPanel   { 10,  314, 290, 266 };
    const juce::Rectangle<int> morphPanel    { 310, 314, 330, 266 };
    const juce::Rectangle<int> outputPanel   { 650, 314, 280, 266 };
    const juce::Rectangle<int> presetsPanel  { 10,  590, 920, 46 };
}

AleaAudioProcessorEditor::AleaAudioProcessorEditor (AleaAudioProcessor& p)
    : AudioProcessorEditor (p), alea (p),
      keyboardA (p, 'a', 0, colors::purple),
      keyboardB (p, 'b', 1, colors::cyan),
      restsA (p, 'a', 0, colors::purple),
      restsB (p, 'b', 1, colors::cyan),
      intervalMode (*p.apvts.getParameter ("intervalMode"), params::timingModes, colors::text.withAlpha (0.9f)),
      lengthMode   (*p.apvts.getParameter ("lengthMode"),   params::timingModes, colors::text.withAlpha (0.9f)),
      morphBar (p),
      morphDurMode (*p.apvts.getParameter ("morphDurMode"), params::morphDurModes, colors::text.withAlpha (0.9f)),
      tempoSource  (*p.apvts.getParameter ("tempoSource"),  params::tempoSources, colors::text.withAlpha (0.9f)),
      output (p)
{
    addAndMakeVisible (keyboardA);
    addAndMakeVisible (keyboardB);
    addAndMakeVisible (restsA);
    addAndMakeVisible (restsB);
    addAndMakeVisible (intervalMode);
    addAndMakeVisible (lengthMode);
    addAndMakeVisible (morphBar);
    addAndMakeVisible (morphDurMode);
    addAndMakeVisible (tempoSource);
    addAndMakeVisible (output);

    // Octave is a position, not an amount: dot-on-a-track, no bar fill.
    setupSlider (aOctMin, "aOctMin", colors::purple, true);  setupSlider (aOctMax, "aOctMax", colors::purple, true);
    setupSlider (aVelMin, "aVelMin", colors::purple);        setupSlider (aVelMax, "aVelMax", colors::purple);
    setupSlider (bOctMin, "bOctMin", colors::cyan, true);    setupSlider (bOctMax, "bOctMax", colors::cyan, true);
    setupSlider (bVelMin, "bVelMin", colors::cyan);          setupSlider (bVelMax, "bVelMax", colors::cyan);
    setupSlider (intervalFree, "intervalFree", colors::text.withAlpha (0.6f));
    setupSlider (lengthFree, "lengthFree", colors::text.withAlpha (0.6f));
    setupSlider (morphDurFree, "morphDurFree", colors::amber);
    setupSlider (internalTempo, "internalTempo", colors::green);

    setupCombo (intervalSync, "intervalSync");
    setupCombo (lengthSync, "lengthSync");
    setupCombo (morphDurBars, "morphDurBars");
    setupCombo (morphDurUnit, "morphDurUnit");

    // Decorated labels (display only - host automation keeps clean names)
    setupCombo (morphMode, "morphMode", juce::StringArray {
        juce::String::fromUTF8 ("A \xe2\x86\x92 B"),
        juce::String::fromUTF8 ("A \xe2\x86\x92 B \xe2\x86\xbb"),
        juce::String::fromUTF8 ("A \xe2\x87\x84 B") });
    setupCombo (morphCurve, "morphCurve", juce::StringArray {
        juce::String::fromUTF8 ("Linear \xe2\x95\xb1"),
        juce::String::fromUTF8 ("Exponential x\xc2\xb2"),
        juce::String::fromUTF8 ("S-Curve \xe2\x88\xbf"),
        juce::String::fromUTF8 ("Logarithmic \xe2\x88\x9ax") });

    menuButton.setButtonText (juce::String::fromUTF8 ("\xe2\x8b\xaf"));
    menuButton.setColour (juce::TextButton::buttonColourId, colors::control);
    menuButton.setColour (juce::TextButton::textColourOffId, colors::text);
    menuButton.onClick = [this]
    {
        juce::PopupMenu m;
        m.addItem ("About Alea...", []
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                "Alea",
                juce::String::fromUTF8 (
                    "Aleatoric Scale Shifter\nVersion 0.1.0\n\n"
                    "HOW TO USE\n"
                    "Alea generates MIDI, not audio. Put it on its own MIDI track, "
                    "then route an instrument track's MIDI input from Alea and press Play. "
                    "In Ableton Live: on the instrument track set MIDI From to the Alea "
                    "track, pick 'Alea' in the lower chooser, and set Monitor to In.\n\n"
                    "ALEA was made with a particular vision in mind: exploring the "
                    "relationship between an improvising human player and a machine that "
                    "randomly shifts from a diatonic scale to complete dodecaphony over "
                    "time (hence the last factory preset).\n\n"
                    "That said, I'm sure any musician playing with it will have all kinds "
                    "of ideas, and I hope it can serve your musical aspirations.\n\n"
                    "I'll be more than happy to hear your feedback, ideas and music made "
                    "with ALEA! You can reach me through GitHub or my email: "
                    "yuvalprod@gmail.com\n\n"
                    "Made by Yuval Egozi"));
        });
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (menuButton));
    };
    addAndMakeVisible (menuButton);

    // Auto-sweep is the heart of the plugin - a full toggle button, not a tick.
    autoSweep.setButtonText (juce::String::fromUTF8 ("\xe2\x9f\xbf  AUTO-SWEEP"));
    autoSweep.setClickingTogglesState (true);
    autoSweep.setColour (juce::TextButton::buttonColourId, colors::control);
    autoSweep.setColour (juce::TextButton::buttonOnColourId, colors::text.withAlpha (0.9f));
    autoSweep.setColour (juce::TextButton::textColourOffId, colors::secondary);
    autoSweep.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    addAndMakeVisible (autoSweep);
    sweepAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        alea.apvts, "autoSweep", autoSweep);

    // Presets row
    for (size_t i = 0; i < presets::factory().size(); ++i)
    {
        auto b = std::make_unique<juce::TextButton> (juce::String::fromUTF8 (presets::factory()[i].name));
        b->setColour (juce::TextButton::buttonColourId, colors::control);
        b->setColour (juce::TextButton::textColourOffId, colors::text);
        b->onClick = [this, i] { presets::apply (alea.apvts, presets::factory()[i]); };
        addAndMakeVisible (*b);
        presetButtons.push_back (std::move (b));
    }

    for (auto* b : { &savePreset, &loadPreset })
    {
        b->setColour (juce::TextButton::buttonColourId, colors::panel);
        b->setColour (juce::TextButton::textColourOffId, colors::secondary);
        addAndMakeVisible (*b);
    }

    savePreset.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser> ("Save Alea preset",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.alea");
        fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                  | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
            [this] (const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file == juce::File())
                    return;
                auto state = alea.apvts.copyState();
                state.setProperty ("stateVersion", 2, nullptr);
                if (auto xml = state.createXml())
                    file.replaceWithText (xml->toString());
            });
    };

    loadPreset.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser> ("Load Alea preset",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.alea");
        fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (! file.existsAsFile())
                    return;
                if (auto xml = juce::XmlDocument::parse (file))
                    if (xml->hasTagName (alea.apvts.state.getType()))
                        alea.apvts.replaceState (juce::ValueTree::fromXml (*xml));
            });
    };

    setSize (kWidth, kHeight);
    updateModeVisibility();
    startTimerHz (15);
}

void AleaAudioProcessorEditor::setupSlider (juce::Slider& s, const juce::String& paramID, juce::Colour accent,
                                            bool positionStyle)
{
    if (positionStyle)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 30, 20);
        s.setColour (juce::Slider::thumbColourId, accent);
        s.setColour (juce::Slider::trackColourId, colors::control);
    }
    else
    {
        s.setSliderStyle (juce::Slider::LinearBar);
        s.setColour (juce::Slider::trackColourId, accent.withAlpha (0.55f));
    }
    s.setColour (juce::Slider::backgroundColourId, colors::control);
    s.setColour (juce::Slider::textBoxTextColourId, colors::text);
    s.setColour (juce::Slider::textBoxOutlineColourId, colors::border);
    addAndMakeVisible (s);
    sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        alea.apvts, paramID, s));
}

void AleaAudioProcessorEditor::setupCombo (juce::ComboBox& c, const juce::String& paramID,
                                           const juce::StringArray& customLabels)
{
    if (customLabels.size() > 0)
        c.addItemList (customLabels, 1);
    else if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (alea.apvts.getParameter (paramID)))
        c.addItemList (choice->choices, 1);

    c.setColour (juce::ComboBox::backgroundColourId, colors::control);
    c.setColour (juce::ComboBox::textColourId, colors::text);
    c.setColour (juce::ComboBox::outlineColourId, colors::border);
    c.setColour (juce::ComboBox::arrowColourId, colors::secondary);
    addAndMakeVisible (c);
    comboAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        alea.apvts, paramID, c));
}

void AleaAudioProcessorEditor::resized()
{
    // Header
    tempoSource.setBounds (586, 16, 150, 26);
    internalTempo.setBounds (746, 16, 148, 26);
    menuButton.setBounds (902, 16, 28, 26);

    auto scaleControls = [] (const juce::Rectangle<int>& panel, PianoKeyboard& kb, RestSelector& rests,
                                 juce::Slider& octMin, juce::Slider& octMax,
                                 juce::Slider& velMin, juce::Slider& velMax)
    {
        const int x = panel.getX() + 12, w = panel.getWidth() - 24;
        kb.setBounds (x, panel.getY() + 30, w, 96);
        rests.setBounds (x + 44, panel.getY() + 132, w - 44, 26);
        const int half = (w - 52) / 2;
        octMin.setBounds (x + 44, panel.getY() + 168, half, 24);
        octMax.setBounds (x + 52 + half, panel.getY() + 168, half, 24);
        velMin.setBounds (x + 44, panel.getY() + 202, half, 24);
        velMax.setBounds (x + 52 + half, panel.getY() + 202, half, 24);
    };

    scaleControls (scaleAPanel, keyboardA, restsA, aOctMin, aOctMax, aVelMin, aVelMax);
    scaleControls (scaleBPanel, keyboardB, restsB, bOctMin, bOctMax, bVelMin, bVelMax);

    // Timing
    {
        const int x = timingPanel.getX() + 12, w = timingPanel.getWidth() - 24;
        intervalMode.setBounds (x, timingPanel.getY() + 48, w, 26);
        intervalSync.setBounds (x, timingPanel.getY() + 80, w, 26);
        intervalFree.setBounds (x, timingPanel.getY() + 80, w, 26);
        lengthMode.setBounds (x, timingPanel.getY() + 146, w, 26);
        lengthSync.setBounds (x, timingPanel.getY() + 178, w, 26);
        lengthFree.setBounds (x, timingPanel.getY() + 178, w, 26);
    }

    // Morph
    {
        const int x = morphPanel.getX() + 12, w = morphPanel.getWidth() - 24;
        morphBar.setBounds (x, morphPanel.getY() + 34, w, 34);
        autoSweep.setBounds (x, morphPanel.getY() + 79, 150, 24);
        morphDurMode.setBounds (x + 160, morphPanel.getY() + 79, w - 160, 24);
        morphDurBars.setBounds (x, morphPanel.getY() + 122, w, 26);
        morphDurFree.setBounds (x, morphPanel.getY() + 122, w - 110, 26);
        morphDurUnit.setBounds (x + w - 102, morphPanel.getY() + 122, 102, 26);
        morphMode.setBounds (x, morphPanel.getY() + 168, w, 26);
        morphCurve.setBounds (x, morphPanel.getY() + 214, w, 26);
    }

    output.setBounds (outputPanel.getX() + 12, outputPanel.getY() + 34,
                      outputPanel.getWidth() - 24, outputPanel.getHeight() - 46);

    // Presets row
    {
        const int widths[] = { 118, 122, 118, 104, 178 };
        int x = presetsPanel.getX() + 90;
        const int y = presetsPanel.getY() + 10;
        for (size_t i = 0; i < presetButtons.size(); ++i)
        {
            const int w = widths[juce::jmin ((size_t) 4, i)];
            presetButtons[i]->setBounds (x, y, w, 26);
            x += w + 8;
        }
        savePreset.setBounds (presetsPanel.getRight() - 128, y, 58, 26);
        loadPreset.setBounds (presetsPanel.getRight() - 64, y, 54, 26);
    }
}

void AleaAudioProcessorEditor::timerCallback()
{
    updateModeVisibility();
    keyboardA.repaint();
    keyboardB.repaint();
    restsA.repaint();
    restsB.repaint();
    morphBar.repaint();
    output.repaint();
    repaint (0, 0, getWidth(), 60); // header status dot

    // Random-mode "current pick" readouts live in the timing panel
    if ((int) alea.apvts.getRawParameterValue ("intervalMode")->load() == params::random
        || (int) alea.apvts.getRawParameterValue ("lengthMode")->load() == params::random)
        repaint (timingPanel);
}

void AleaAudioProcessorEditor::updateModeVisibility()
{
    const int iMode = (int) alea.apvts.getRawParameterValue ("intervalMode")->load();
    intervalSync.setVisible (iMode == params::sync);
    intervalFree.setVisible (iMode == params::free);

    const int lMode = (int) alea.apvts.getRawParameterValue ("lengthMode")->load();
    lengthSync.setVisible (lMode == params::sync);
    lengthFree.setVisible (lMode == params::free);

    const bool durSync = (int) alea.apvts.getRawParameterValue ("morphDurMode")->load() == 0;
    morphDurBars.setVisible (durSync);
    morphDurFree.setVisible (! durSync);
    morphDurUnit.setVisible (! durSync);

    const bool freeRun = (int) alea.apvts.getRawParameterValue ("tempoSource")->load() == 1;
    internalTempo.setEnabled (freeRun);
    internalTempo.setAlpha (freeRun ? 1.0f : 0.4f);
}

void AleaAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (colors::background);

    // Header
    g.setColour (colors::text);
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("ALEA", 20, 10, 120, 38, juce::Justification::centredLeft);
    g.setColour (colors::secondary);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("Aleatoric Scale Shifter", 96, 16, 180, 28, juce::Justification::centredLeft);

    const bool playing = alea.hostIsPlaying.load();
    g.setColour (playing ? colors::green : colors::control);
    g.fillEllipse (262.0f, 22.0f, 12.0f, 12.0f);
    g.setColour (colors::secondary);
    g.drawText (playing ? "playing" : "stopped", 282, 14, 90, 28, juce::Justification::centredLeft);

    g.drawText ("TEMPO", 512, 16, 68, 26, juce::Justification::centredRight);

    auto drawPanel = [&g] (const juce::Rectangle<int>& r, const juce::String& title, juce::Colour accent)
    {
        g.setColour (colors::panel);
        g.fillRoundedRectangle (r.toFloat(), 8.0f);
        g.setColour (colors::border);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 8.0f, 1.0f);
        g.setColour (accent);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (title, r.getX() + 12, r.getY() + 8, r.getWidth() - 24, 18, juce::Justification::centredLeft);
    };

    drawPanel (scaleAPanel, "SCALE A", colors::purple);
    drawPanel (scaleBPanel, "SCALE B", colors::cyan);
    drawPanel (timingPanel, "TIMING", colors::text.withAlpha (0.9f));
    drawPanel (morphPanel, "MORPH", colors::text.withAlpha (0.9f));
    drawPanel (outputPanel, "OUTPUT", colors::green);
    drawPanel (presetsPanel, "", colors::secondary);
    g.setColour (colors::secondary);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText ("PRESETS", presetsPanel.getX() + 12, presetsPanel.getY(), 80, presetsPanel.getHeight(),
                juce::Justification::centredLeft);

    // Small row labels
    g.setColour (colors::secondary);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));

    for (const auto* panel : { &scaleAPanel, &scaleBPanel })
    {
        g.drawText ("RESTS", panel->getX() + 12, panel->getY() + 132, 40, 26, juce::Justification::centredLeft);
        g.drawText ("OCT",   panel->getX() + 12, panel->getY() + 168, 40, 24, juce::Justification::centredLeft);
        g.drawText ("VEL",   panel->getX() + 12, panel->getY() + 202, 40, 24, juce::Justification::centredLeft);
    }

    g.drawText ("INTERVAL", timingPanel.getX() + 12, timingPanel.getY() + 30, 100, 16, juce::Justification::centredLeft);
    g.drawText ("LENGTH",   timingPanel.getX() + 12, timingPanel.getY() + 128, 100, 16, juce::Justification::centredLeft);

    // Random-mode monitoring: show what the dice just rolled.
    auto drawRandomPick = [&g] (int y, int poolIndex)
    {
        g.setColour (colors::text.withAlpha (0.85f));
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.drawText (poolIndex >= 0 ? "now: " + params::randomPoolNames[poolIndex] : "now: -",
                    timingPanel.getX() + 12, y, timingPanel.getWidth() - 24, 26, juce::Justification::centred);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.setColour (colors::secondary);
    };

    if ((int) alea.apvts.getRawParameterValue ("intervalMode")->load() == params::random)
        drawRandomPick (timingPanel.getY() + 80, alea.lastRandomInterval.load());
    if ((int) alea.apvts.getRawParameterValue ("lengthMode")->load() == params::random)
        drawRandomPick (timingPanel.getY() + 178, alea.lastRandomLength.load());
    g.drawText ("DURATION", morphPanel.getX() + 12, morphPanel.getY() + 108, 100, 12, juce::Justification::centredLeft);
    g.drawText ("MODE",     morphPanel.getX() + 12, morphPanel.getY() + 154, 100, 12, juce::Justification::centredLeft);
    g.drawText ("CURVE",    morphPanel.getX() + 12, morphPanel.getY() + 200, 100, 12, juce::Justification::centredLeft);
}

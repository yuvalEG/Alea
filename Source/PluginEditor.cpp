#include "PluginEditor.h"
#include "Presets.h"
#include "BinaryData.h"

using namespace ui;

namespace
{
    constexpr int kWidth = 940, kHeight = 674;

    // About dialog: wordmark over a subtle vertical gradient, text below.
    struct AboutComponent : juce::Component
    {
        AboutComponent()
        {
            logo = juce::ImageCache::getFromMemory (BinaryData::logo_png, BinaryData::logo_pngSize);

            text.setMultiLine (true);
            text.setReadOnly (true);
            text.setCaretVisible (false);
            text.setScrollbarsShown (true);
            text.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
            text.setColour (juce::TextEditor::textColourId, colors::text);
            text.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
            text.setFont (juce::FontOptions (14.5f));
            text.setText (juce::String::fromUTF8 (
                "Aleatoric Scale Shifter - Version 0.2.0\n\n\n"
                "HOW TO USE\n\n"
                "Alea generates MIDI notes - it makes no sound of its own "
                "(unless you pick Internal Synth under OUT).\n\n"
                "1. Load Alea on a MIDI track.\n"
                "2. Create a second MIDI track and put any instrument on it.\n"
                "3. Route the instrument track's MIDI input from the Alea track "
                "(in Ableton Live: set 'MIDI From' to the Alea track and pick "
                "'Alea' in the chooser below it).\n"
                "4. Arm the instrument track and press Play - Alea follows the "
                "host transport and you should hear notes drawn from Scale A.\n"
                "5. From there: pick a preset, set up your own Scale A and "
                "Scale B, drag the morph bar to blend between them, or hit "
                "AUTO-SWEEP and let Alea travel on its own.\n\n"
                "Hearing nothing? Check the instrument track is armed and the "
                "header dot says 'playing'.\n\n\n"
                "THE IDEA\n\n"
                "ALEA was made with a particular vision in mind: exploring the "
                "relationship between an improvising human player and a machine "
                "that randomly shifts from a diatonic scale to complete "
                "dodecaphony over time (hence the last factory preset).\n\n"
                "That said, I'm sure any musician playing with it will have all "
                "kinds of ideas, and I hope it can serve your musical "
                "aspirations.\n\n\n"
                "GET IN TOUCH\n\n"
                "I'll be more than happy to hear your feedback, ideas and music "
                "made with ALEA! You can reach me through GitHub or my email: "
                "yuvalprod@gmail.com\n\n\n"
                "Plugin Made By Yuval Egozi"),
                juce::dontSendNotification);
            addAndMakeVisible (text);
            setSize (640, 620);
        }

        void paint (juce::Graphics& g) override
        {
            g.setGradientFill (juce::ColourGradient (colors::panel.brighter (0.08f), 0.0f, 0.0f,
                                                     colors::background, 0.0f, (float) getHeight(), false));
            g.fillRect (getLocalBounds());

            if (logo.isValid())
                g.drawImage (logo, juce::Rectangle<float> ((float) getWidth() / 2.0f - 62.0f, 16.0f, 124.0f, 48.0f),
                             juce::RectanglePlacement::centred);

            // A whisper of the scale colors under the wordmark
            g.setGradientFill (juce::ColourGradient (colors::purple.withAlpha (0.5f), 40.0f, 0.0f,
                                                     colors::cyan.withAlpha (0.5f), (float) getWidth() - 40.0f, 0.0f, false));
            g.fillRect (40, 74, getWidth() - 80, 1);
        }

        void resized() override
        {
            text.setBounds (getLocalBounds().withTrimmedTop (86).reduced (18, 10));
        }

        juce::Image logo;
        juce::TextEditor text;
    };

    const juce::Rectangle<int> presetsPanel  { 10,  64, 920, 76 };
    const juce::Rectangle<int> scaleAPanel   { 10,  148, 455, 240 };
    const juce::Rectangle<int> scaleBPanel   { 475, 148, 455, 240 };
    const juce::Rectangle<int> timingPanel   { 10,  398, 290, 266 };
    const juce::Rectangle<int> morphPanel    { 310, 398, 330, 266 };
    const juce::Rectangle<int> outputPanel   { 650, 398, 280, 266 };
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
      morphMode (*p.apvts.getParameter ("morphMode"),
                 juce::StringArray { juce::String::fromUTF8 ("A \xe2\x86\x92 B"),
                                     juce::String::fromUTF8 ("A \xe2\x86\x92 B \xe2\x86\xbb"),
                                     juce::String::fromUTF8 ("A \xe2\x87\x84 B") },
                 colors::text.withAlpha (0.9f),
                 juce::StringArray { "One-Shot: travel to B, then stay there",
                                     "Loop: travel to B, jump back to A, repeat",
                                     "Bounce: back and forth between A and B" }),
      morphCurve (*p.apvts.getParameter ("morphCurve"), colors::text.withAlpha (0.9f)),
      tempoSource  (*p.apvts.getParameter ("tempoSource"),  params::tempoSources, colors::text.withAlpha (0.9f)),
      standalone (p.wrapperType == juce::AudioProcessor::wrapperType_Standalone),
      output (p)
{
    // Space Grotesk everywhere - geometric, matches the wordmark.
    static const auto aleaTypeface = juce::Typeface::createSystemTypefaceFor (
        BinaryData::SpaceGroteskMedium_ttf, BinaryData::SpaceGroteskMedium_ttfSize);
    juce::LookAndFeel::getDefaultLookAndFeel().setDefaultSansSerifTypeface (aleaTypeface);

    addAndMakeVisible (keyboardA);
    addAndMakeVisible (keyboardB);
    addAndMakeVisible (restsA);
    addAndMakeVisible (restsB);
    addAndMakeVisible (intervalMode);
    addAndMakeVisible (lengthMode);
    addAndMakeVisible (morphBar);
    addAndMakeVisible (morphDurMode);
    addAndMakeVisible (morphMode);
    addAndMakeVisible (morphCurve);
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

    // Sync divisions are fractions of a bar; in 4/4 that maps 1:1 onto note
    // values (display only - the musical glyphs aren't in the UI font, so
    // the word "note" it is).
    const juce::StringArray divisionDisplay {
        "1/64 note", "1/32 note", "1/16 note", "1/8 note", "1/4 note", "1/2 note",
        "1 bar", "2 bars", "4 bars" };
    setupCombo (intervalSync, "intervalSync", divisionDisplay);
    setupCombo (lengthSync, "lengthSync", divisionDisplay);
    setupCombo (morphDurBars, "morphDurBars", juce::StringArray {
        "1 bar", "2 bars", "4 bars", "8 bars", "16 bars", "32 bars", "64 bars" });
    setupCombo (morphDurUnit, "morphDurUnit");

    menuButton.setButtonText (juce::String::fromUTF8 ("\xe2\x8b\xaf"));
    menuButton.setColour (juce::TextButton::buttonColourId, colors::control);
    menuButton.setColour (juce::TextButton::textColourOffId, colors::text);
    menuButton.onClick = [this]
    {
        juce::PopupMenu m;
        m.addItem ("Check for Updates...", []
        {
            // v1: opens the releases page; compare against the version in
            // About. An in-app version check can come once the repo is public.
            juce::URL ("https://github.com/yuvalEG/Alea/releases").launchInDefaultBrowser();
        });
        m.addSeparator();
        m.addItem ("About Alea...", []
        {
            juce::DialogWindow::LaunchOptions o;
            o.content.setOwned (new AboutComponent());
            o.dialogTitle = "About Alea";
            o.dialogBackgroundColour = colors::panel;
            o.escapeKeyTriggersCloseButton = true;
            o.useNativeTitleBar = true;
            o.resizable = false;
            o.launchAsync();
        });
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (menuButton));
    };
    addAndMakeVisible (menuButton);

    // Standalone: no host transport, so PLAY/STOP lives where the Host/
    // Free-Run selector would be (the clock is always internal there),
    // with spacebar as the shortcut - like any DAW. The plugin never
    // takes keyboard focus; space belongs to the host there.
    setWantsKeyboardFocus (standalone);
    tempoSource.setVisible (! standalone);
    playButton.setClickingTogglesState (true);
    playButton.setColour (juce::TextButton::buttonColourId, colors::control);
    playButton.setColour (juce::TextButton::buttonOnColourId, colors::green);
    playButton.setColour (juce::TextButton::textColourOffId, colors::text);
    playButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    playButton.onClick = [this]
    {
        const bool on = playButton.getToggleState();
        playButton.setButtonText (on ? "STOP" : "PLAY");
        alea.standaloneTransport.store (on);
    };
    addChildComponent (playButton);
    playButton.setVisible (standalone);

    // Plugin only: the most common support question is routing, so the
    // answer lives one click away. The standalone synth needs no routing.
    helpLink.setButtonText ("No sound? Routing Help");
    helpLink.setURL (juce::URL ("https://github.com/yuvalEG/Alea#troubleshooting"));
    helpLink.setFont (juce::FontOptions (12.0f), false, juce::Justification::centredRight);
    helpLink.setColour (juce::HyperlinkButton::textColourId, colors::secondary);
    addChildComponent (helpLink);
    helpLink.setVisible (! standalone);

    panicButton.setColour (juce::TextButton::buttonColourId, colors::red.withAlpha (0.85f));
    panicButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    panicButton.onClick = [this] { alea.panicRequested.store (true); };
    addAndMakeVisible (panicButton);

    // Freeze is a performance control and Panic an escape hatch - they live
    // far apart in the header so neither is hit by accident.
    freezeButton.setClickingTogglesState (true);
    freezeButton.setColour (juce::TextButton::buttonColourId, colors::control);
    freezeButton.setColour (juce::TextButton::buttonOnColourId, colors::cyan.withAlpha (0.9f));
    freezeButton.setColour (juce::TextButton::textColourOffId, colors::secondary);
    freezeButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    addAndMakeVisible (freezeButton);
    freezeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        alea.apvts, "freeze", freezeButton);

    // Auto-sweep is the heart of the plugin - a full toggle button, not a tick.
    autoSweep.setButtonText (juce::String::fromUTF8 ("AUTO-SWEEP \xe2\x86\x92"));
    autoSweep.setClickingTogglesState (true);
    autoSweep.setColour (juce::TextButton::buttonColourId, colors::control);
    autoSweep.setColour (juce::TextButton::buttonOnColourId, colors::text.withAlpha (0.9f));
    autoSweep.setColour (juce::TextButton::textColourOffId, colors::secondary);
    autoSweep.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    addAndMakeVisible (autoSweep);
    sweepAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        alea.apvts, "autoSweep", autoSweep);

    // Every preset is a one-click bubble; the active one stays lit.
    for (size_t i = 0; i < presets::factory().size(); ++i)
    {
        auto b = std::make_unique<juce::TextButton> (juce::String::fromUTF8 (presets::factory()[i].name));
        b->setColour (juce::TextButton::buttonColourId, colors::control);
        b->setColour (juce::TextButton::buttonOnColourId, colors::text.withAlpha (0.9f));
        b->setColour (juce::TextButton::textColourOffId, colors::text);
        b->setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        const int idx = (int) i;
        b->onClick = [this, idx] { applyPresetAndMark (idx); };
        addAndMakeVisible (*b);
        presetBtns.push_back (std::move (b));
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

    setSize (kWidth, standalone ? kHeight : kHeight + 20); // plugin: footer row for the help link
    updateModeVisibility();
    timerCallback(); // apply dimming/visibility state before first paint
    startTimerHz (15);
}

void AleaAudioProcessorEditor::applyPresetAndMark (int index)
{
    presets::apply (alea.apvts, presets::factory()[(size_t) index]);
    alea.currentPreset.store (index);
    shownPreset = index;
    // Don't snapshot yet: the host echoes parameter edits back asynchronously
    // with its own rounding, which would read as instant divergence.
    presetSnapshot.clear();
    snapshotCountdown = 4;
    markPreset (index);
}

void AleaAudioProcessorEditor::markPreset (int index)
{
    for (size_t k = 0; k < presetBtns.size(); ++k)
        presetBtns[k]->setToggleState ((int) k == index, juce::dontSendNotification);
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
    freezeButton.setBounds (380, 16, 80, 26);
    tempoSource.setBounds (530, 16, 140, 26);
    playButton.setBounds (566, 16, 104, 26);
    internalTempo.setBounds (678, 16, 130, 26);
    panicButton.setBounds (820, 16, 72, 26);
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

    // Presets panel: two rows of four bubbles, Save/Load stacked at the right
    {
        const int x0 = presetsPanel.getX() + 90;
        const int rowW = presetsPanel.getRight() - 76 - x0;
        const int perRow = 4;
        const int w = (rowW - (perRow - 1) * 8) / perRow;
        for (size_t k = 0; k < presetBtns.size(); ++k)
        {
            const int row = (int) k / perRow, col = (int) k % perRow;
            presetBtns[k]->setBounds (x0 + col * (w + 8), presetsPanel.getY() + 10 + row * 30, w, 26);
        }
        savePreset.setBounds (presetsPanel.getRight() - 64, presetsPanel.getY() + 10, 54, 26);
        loadPreset.setBounds (presetsPanel.getRight() - 64, presetsPanel.getY() + 40, 54, 26);
    }

    helpLink.setBounds (getWidth() - 250, getHeight() - 22, 240, 18);
}

bool AleaAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    if (standalone && key == juce::KeyPress::spaceKey)
    {
        playButton.triggerClick(); // toggles and fires onClick
        return true;
    }
    return false;
}

void AleaAudioProcessorEditor::timerCallback()
{
    updateModeVisibility();

    // The engine owns "which preset is active" (it survives editor close/
    // reopen and session reload); the bubbles follow it.
    {
        const int cp = alea.currentPreset.load();
        if (cp != shownPreset)
        {
            shownPreset = cp;
            markPreset (cp);
            presetSnapshot.clear();
            if (cp >= 0)
                snapshotCountdown = 2; // arm the divergence check
        }
    }

    // A fully one-sided morph means the other scale can't sound - dim it.
    {
        const bool sweep = alea.apvts.getRawParameterValue ("autoSweep")->load() > 0.5f;
        const float pct = sweep ? (float) alea.morphPercent.load()
                                : alea.apvts.getRawParameterValue ("morphPos")->load();
        const float newA = pct >= 99.95f ? 0.35f : 1.0f;
        const float newB = pct <= 0.05f  ? 0.35f : 1.0f;

        if (! juce::approximatelyEqual (newA, alphaA) || ! juce::approximatelyEqual (newB, alphaB))
        {
            alphaA = newA;
            alphaB = newB;
            for (auto* c : { (juce::Component*) &keyboardA, (juce::Component*) &restsA,
                             (juce::Component*) &aOctMin, (juce::Component*) &aOctMax,
                             (juce::Component*) &aVelMin, (juce::Component*) &aVelMax })
                c->setAlpha (alphaA);
            for (auto* c : { (juce::Component*) &keyboardB, (juce::Component*) &restsB,
                             (juce::Component*) &bOctMin, (juce::Component*) &bOctMax,
                             (juce::Component*) &bVelMin, (juce::Component*) &bVelMax })
                c->setAlpha (alphaB);
            repaint (scaleAPanel);
            repaint (scaleBPanel);
        }
    }

    // Any manual tweak after applying a preset un-marks it. The snapshot is
    // captured a few ticks after apply (host echoes settle), and compared
    // with a tolerance well above echo jitter but below any human tweak.
    if (snapshotCountdown > 0)
    {
        if (--snapshotCountdown == 0)
            for (auto* param : alea.getParameters())
                presetSnapshot.push_back (param->getValue());
    }
    else if (! presetSnapshot.empty())
    {
        const auto& ps = alea.getParameters();
        for (size_t i = 0; i < (size_t) ps.size() && i < presetSnapshot.size(); ++i)
        {
            if (std::abs (ps[(int) i]->getValue() - presetSnapshot[i]) > 4.0e-3f)
            {
                presetSnapshot.clear();
                alea.currentPreset.store (-1);
                shownPreset = -1;
                markPreset (-1);
                break;
            }
        }
    }

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

    const bool freeRun = standalone
                         || (int) alea.apvts.getRawParameterValue ("tempoSource")->load() == 1;
    internalTempo.setEnabled (freeRun);
    internalTempo.setAlpha (freeRun ? 1.0f : 0.4f);
}

void AleaAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (colors::background);

    // Header: the wordmark image, with the subtitle beside it
    {
        static const juce::Image logo = juce::ImageCache::getFromMemory (BinaryData::logo_png, BinaryData::logo_pngSize);
        if (logo.isValid())
            g.drawImage (logo, juce::Rectangle<float> (20.0f, 12.0f, 87.0f, 34.0f),
                         juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid);
    }
    g.setColour (colors::secondary);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("Aleatoric Scale Shifter", 120, 16, 180, 28, juce::Justification::centredLeft);

    const bool playing = alea.hostIsPlaying.load();
    g.setColour (playing ? colors::green : colors::control);
    g.fillEllipse (262.0f, 22.0f, 12.0f, 12.0f);
    g.setColour (colors::secondary);
    g.drawText (playing ? "playing" : "stopped", 282, 14, 90, 28, juce::Justification::centredLeft);

    g.drawText ("TEMPO", 462, 16, 60, 26, juce::Justification::centredRight);

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

    drawPanel (scaleAPanel, "SCALE A", colors::purple.withMultipliedAlpha (alphaA));
    drawPanel (scaleBPanel, "SCALE B", colors::cyan.withMultipliedAlpha (alphaB));
    drawPanel (timingPanel, "TIMING", colors::text.withAlpha (0.9f));
    drawPanel (morphPanel, "SCALE MORPH", colors::text.withAlpha (0.9f));
    drawPanel (outputPanel, "OUTPUT", colors::green);
    drawPanel (presetsPanel, "", colors::secondary);
    g.setColour (colors::secondary);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText ("PRESETS", presetsPanel.getX() + 12, presetsPanel.getY(), 76, presetsPanel.getHeight(),
                juce::Justification::centredLeft);

    // Small row labels
    g.setColour (colors::secondary);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));

    for (const auto& [panel, alpha] : { std::pair { &scaleAPanel, alphaA }, std::pair { &scaleBPanel, alphaB } })
    {
        g.setColour (colors::secondary.withMultipliedAlpha (alpha));
        g.drawText ("RESTS", panel->getX() + 12, panel->getY() + 132, 40, 26, juce::Justification::centredLeft);
        g.drawText ("OCT",   panel->getX() + 12, panel->getY() + 168, 40, 24, juce::Justification::centredLeft);
        g.drawText ("VEL",   panel->getX() + 12, panel->getY() + 202, 40, 24, juce::Justification::centredLeft);
    }
    g.setColour (colors::secondary);

    g.drawText ("NOTE RATE",   timingPanel.getX() + 12, timingPanel.getY() + 30, 120, 16, juce::Justification::centredLeft);
    g.drawText ("NOTE LENGTH", timingPanel.getX() + 12, timingPanel.getY() + 128, 120, 16, juce::Justification::centredLeft);

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
    g.drawText ("MORPH DURATION", morphPanel.getX() + 12, morphPanel.getY() + 108, 130, 12, juce::Justification::centredLeft);
    g.drawText ("MORPH MODE",     morphPanel.getX() + 12, morphPanel.getY() + 154, 130, 12, juce::Justification::centredLeft);
    g.drawText ("MORPH CURVE",    morphPanel.getX() + 12, morphPanel.getY() + 200, 130, 12, juce::Justification::centredLeft);
}
